#!/usr/bin/env python3
"""Quantitative, cross-validated guitar model calibration against actual recorded notes.

Electric:  CC-BY-SA-4.0 guitar-fretboard-notes (source=ele, raw Stratocaster DI).
Steel/Nylon: CC0 Meta Open Sound OGG preview (recorded guitar, lossy preview only).
No recordings or sample slices become part of the instrument or its installer.
Use training notes E2/A2/D3, hold out G3/B3/E4. An OGG fit is exploratory.
"""
import argparse, json, math, shutil, subprocess, sys, tempfile, time, urllib.request, urllib.parse
from pathlib import Path
import numpy as np
import soundfile as sf
from scipy import signal, optimize

META = "https://raw.githubusercontent.com/facebookincubator/Meta-open-sound/main/systems/instruments"
HFACE = "https://datasets-server.huggingface.co"
SRC = {
  'electric': 'https://huggingface.co/datasets/collegefishiesd/guitar-fretboard-notes',
  'steel': 'https://github.com/facebookincubator/Meta-open-sound/tree/main/systems/instruments/meta-steel-guitar',
  'nylon': 'https://github.com/facebookincubator/Meta-open-sound/tree/main/systems/instruments/meta-nylon-guitar'
}
NOTES = [('LowE',40,'E2',6),('A',45,'A2',5),('D',50,'D3',4),('G',55,'G3',3),('B',59,'B3',2),('HighE',64,'E4',1)]
CONTROL_DEFAULTS = {'brightness':.72, 'hardness':.60, 'decay':4.0, 'body':.24, 'pluck':.18, 'pickup':.15}
CONTROL_BOUNDS = {'brightness':(.40,.94), 'hardness':(.15,.9), 'decay':(1.5,8.), 'body':(.08,.68)}
# Tuning only three physical parameters to reduce overfitting of mic/pickup response.
TUNE = ['brightness','hardness','decay']


def download(url,dest):
    dest.parent.mkdir(parents=True,exist_ok=True)
    if dest.exists() and dest.stat().st_size>1000:
        return dest
    req=urllib.request.Request(url,headers={'User-Agent':'PMG-real-guitar-calibration/0.1'})
    for attempt in range(4):
        try:
            with urllib.request.urlopen(req, timeout=75) as r, dest.open('wb') as f:
                shutil.copyfileobj(r,f)
            if dest.stat().st_size<1000: raise RuntimeError('download too short')
            return dest
        except Exception:
            dest.unlink(missing_ok=True)
            if attempt==3: raise
            time.sleep(2**attempt)


def electric_rows(root):
    # Hugging Face Dataset Viewer serves direct, unprocessed electric DI files.
    qs=urllib.parse.urlencode({'dataset':'collegefishiesd/guitar-fretboard-notes','config':'default','split':'train','offset':0,'length':100})
    try:
        obj=json.loads(urllib.request.urlopen(urllib.request.Request(HFACE+'/rows?'+qs,headers={'User-Agent':'PMG-calibration/0.1'}),timeout=90).read())
    except Exception as exc:
        print('WARNING electric rows could not be accessed:',str(exc));return {}
    selected={}
    for item in obj.get('rows',[]):
        row=item.get('row',{})
        if row.get('source')!='ele' or int(row.get('fret',-1))!=0: continue
        key=(int(row.get('midi_number',-1)),int(row.get('string_number',-1)))
        aud=row.get('audio') or {}
        link=aud.get('src') if isinstance(aud,dict) else None
        if link: selected[key]=link
    out={}
    for _,midi,_,stringNo in NOTES:
        if (midi,stringNo) not in selected: continue
        dest=root/f'electric_{midi}.wav'
        try:download(selected[(midi,stringNo)],dest);out[midi]=dest
        except Exception as exc:print('WARNING electric note',midi,str(exc))
    return out


def get_references(root,include_electric=True):
    ref={}
    for name,model in [('steel','Steel'),('nylon','Nylon')]:
        coll=f'meta-{name}-guitar'
        ref[name]={}
        for key,midi,note,_ in NOTES:
            layer='Vel02' if name=='steel' else 'Vel02Pick'
            filename=f'INSGuitar{model}_String{key}_{layer}_{note}_01.ogg'
            url=f'{META}/{coll}/ogg/{filename}'
            dest=root/name/filename
            try:download(url,dest);sf.info(dest);ref[name][midi]=dest
            except Exception as exc:print('WARNING',name,midi,exc)
    if include_electric:
        ref['electric']=electric_rows(root/'electric')
    return ref


def read(path,sr=48000):
    x,r=sf.read(path,always_2d=True,dtype='float64')
    x=x.mean(axis=1)
    if r!=sr:
        d=math.gcd(r,sr);x=signal.resample_poly(x,sr//d,r//d)
    return np.nan_to_num(x)


def onset_align(x,sr=48000):
    # First crossing of a robust 1.5-ms smoothed RMS gate, compensating recording latency.
    rms=np.sqrt(signal.fftconvolve(x*x,np.ones(72)/72.,mode='same').clip(min=0))
    peak=np.max(rms)
    if peak<1e-7:raise ValueError('silent reference')
    # Bound attack gate relative to note peak, to reject low background noise.
    noise=float(np.median(rms[:min(len(rms),sr//20)]))
    thresh=min(.16*peak,max(.028*peak,noise*4))
    above=np.flatnonzero(rms>thresh)
    if len(above)==0:raise ValueError('no onset')
    start=max(0,int(above[0])-int(.001*sr))
    return x[start:start+int(1.2*sr)]


def rms(x):return float(np.sqrt(np.mean(x*x))) if len(x) else 0.


def features(path,midi):
    sr=48000;x=onset_align(read(path),sr)
    f0=440*2**((midi-69)/12)
    rr=max(rms(x[int(.07*sr):int(.5*sr)]),1e-9)
    frames=[(.002,.015),(.015,.04),(.04,.08),(.08,.14),(.14,.28),(.28,.55)]
    attack=np.array([np.clip(20*np.log10(max(rms(x[int(a*sr):int(b*sr)]),1e-8)/rr),-70,30) for a,b in frames])
    # Analytic resonator bank centred on up to eight partials. 86ms window -> ~12Hz bin.
    partials=[]
    for t in (.09,.16,.25,.38,.53,.72):
        y=x[int(t*sr):int((t+.09)*sr)]; n=len(y)
        if n<1000: continue
        window=signal.windows.hann(n,sym=False)
        fft=np.abs(np.fft.rfft(y*window,n=16384))+1e-12
        hz=np.fft.rfftfreq(16384,1/sr)
        spectrum=[]
        for k in range(1,9):
            target=f0*k
            ix=np.where((hz>=target-max(7,f0*.045))&(hz<=target+max(7,f0*.045)))[0]
            spectrum.append(float(np.max(fft[ix])) if len(ix) else 1e-12)
        partials.append(spectrum)
    p=20*np.log10(np.asarray(partials)+1e-12)
    slopes=[];mix=[];irregular=[]
    if len(p)>=3:
        ts=np.array((.09,.16,.25,.38,.53,.72)[:len(p)])
        for k in range(8):
            valid=np.isfinite(p[:,k])&(p[:,k]>p[0,k]-36)
            if np.sum(valid)<3:
                slopes.append(float('nan'));irregular.append(float('nan'))
            else:
                coeff=np.polyfit(ts[valid],p[valid,k],1)
                slopes.append(float(np.clip(coeff[0],-100,10)))
                irregular.append(float(np.std(p[valid,k]-np.polyval(coeff,ts[valid]))))
        mix=(p[0,1:]-p[0,0]).clip(-55,30)
    else:
        slopes=np.full(8,np.nan);irregular=np.full(8,np.nan);mix=np.zeros(7)
    # Stable relative pitch measurement. Difference of early vs late pitch removes tuning offset.
    pitch=[]
    for t in [.05,.10,.20,.32,.55]:
        y=x[int(t*sr):int((t+.12)*sr)]
        if len(y)<1000 or rms(y)<1e-7:continue
        yy=y-y.mean();ac=signal.correlate(yy,yy,method='fft',mode='full')[len(yy)-1:]
        lo=max(2,int(sr/(f0*1.08)));hi=min(len(ac)-2,int(sr/(f0*.92)))
        if hi<=lo:continue
        idx=lo+int(np.argmax(ac[lo:hi+1]));den=ac[idx-1]-2*ac[idx]+ac[idx+1]
        delta=np.clip(.5*(ac[idx-1]-ac[idx+1])/den,-.5,.5) if abs(den)>1e-7 else 0
        pitch.append((t,float(np.clip(1200*np.log2(sr/(idx+delta)/f0),-70,70))))
    early=np.median([v for t,v in pitch if t<=.10]) if any(t<=.10 for t,v in pitch) else np.nan
    late=np.median([v for t,v in pitch if t>=.32]) if any(t>=.32 for t,v in pitch) else np.nan
    return {'attack':attack.tolist(),'slopes':[float(v) if np.isfinite(v) else None for v in slopes],'mix':np.asarray(mix).tolist(),
            'irregular':[float(v) if np.isfinite(v) else None for v in irregular],'pitch_early_to_late':float(early-late) if np.isfinite(early-late) else None,
            'rms':rr,'peak':float(np.max(np.abs(x))),'pitch_track':pitch}


def distance(a,b):
    # A proxy for acoustic match, not a perceptual realism score.
    aa=np.array(a['attack']);bb=np.array(b['attack']);
    attack=float(np.mean(np.minimum(np.abs(aa-bb),24.)))
    aa=np.array(a['slopes'],dtype=float);bb=np.array(b['slopes'],dtype=float); mask=np.isfinite(aa)&np.isfinite(bb)
    slopes=float(np.median(np.minimum(np.abs(aa[mask]-bb[mask]),40))) if np.sum(mask)>=3 else 25
    aa=np.array(a['mix']);bb=np.array(b['mix']);
    mix=float(np.mean(np.minimum(np.abs(aa-bb),30)))
    aperiod=a.get('pitch_early_to_late'); bperiod=b.get('pitch_early_to_late')
    pitch=float(min(abs(aperiod-bperiod),30)) if aperiod is not None and bperiod is not None and np.isfinite(aperiod) and np.isfinite(bperiod) else 10
    comps={'attack_db':attack,'harmonic_decay_db_per_second':slopes,'early_harmonic_mix_db':mix,'pitch_relaxation_cents':pitch}
    # Weights discount capture-chain dependent timbre, favour envelopes & decay.
    return float(attack + .18*slopes + .10*mix + .07*pitch),comps


def render(exe,model,midi,velocity,controls,dest):
    cmd=[str(exe),'--single',str(model),str(midi),str(velocity),str(dest)]
    cmd.extend([f'--{k}={v:.6f}' for k,v in sorted(controls.items())])
    p=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
    if p.returncode:raise RuntimeError(f'render: {p.stderr[:250]}')
    return dest


def calibrate_kind(exe,kind,refs,outdir,maxfev=20):
    model={'electric':1,'steel':2,'nylon':3}[kind]
    available=sorted(refs)
    train=[n for n in [40,45,50] if n in refs]
    validation=[n for n in [55,59,64] if n in refs]
    if len(train)<2 or not validation:raise RuntimeError('insufficient real recordings, cannot validate')
    refs_feat={note:features(path,note) for note,path in refs.items()}
    fnotes=list(train)+list(validation)
    render_dir=outdir/kind;render_dir.mkdir(parents=True,exist_ok=True)
    evaluated={}
    def measure(c,notes):
        result={}
        for note in notes:
            path=render(exe,model,note,.65,c,render_dir/f'{note}.wav')
            feat=features(path,note)
            score,parts=distance(refs_feat[note],feat)
            result[note]={'score':round(score,4),'components':parts,'synthetic_features':feat}
        return result
    base=measure(CONTROL_DEFAULTS,fnotes)
    best={'control':dict(CONTROL_DEFAULTS),'loss':float(np.mean([base[n]['score'] for n in train]))}
    cache={}
    def objective(x):
        c=dict(CONTROL_DEFAULTS);c.update(dict(zip(TUNE,x)))
        key=tuple(round(float(v),5) for v in x)
        if key not in cache:
            vals=measure(c,train)
            cache[key]=float(np.mean([v['score'] for v in vals.values()]))
        if cache[key] < best['loss']:
            best['loss']=cache[key];best['control']=c
        return cache[key]
    x0=np.array([CONTROL_DEFAULTS[k] for k in TUNE])
    bounds=[CONTROL_BOUNDS[k] for k in TUNE]
    # L-BFGS uses finite differences; quantisation of floats/noise is problematic.
    # Powell bounded passes deterministic candidate values, independent of individual samples.
    try: optimize.minimize(objective,x0,method='Powell',bounds=bounds,
                           options={'maxfev':maxfev,'xtol':.08,'ftol':.035})
    except Exception as e:print('WARNING optimiser',kind,e)
    cal=measure(best['control'],fnotes)
    btrain=np.mean([base[n]['score'] for n in train]);atrain=np.mean([cal[n]['score'] for n in train])
    bval=np.mean([base[n]['score'] for n in validation]);aval=np.mean([cal[n]['score'] for n in validation])
    # Gate on held-out notes; at most 2% worse allowed due to noise.
    accepted=bool(aval < bval*.98 and atrain < btrain*.98)
    return {'kind':kind,'reference_source':SRC[kind],'reference_license':'CC0-1.0' if kind!='electric' else 'CC-BY-SA-4.0',
            'source_quality':'lossy OGG preview; exploratory' if kind!='electric' else '16-bit WAV direct-input, sample-based',
            'train_notes':train,'heldout_notes':validation,'baseline_controls':CONTROL_DEFAULTS,
            'fitted_controls':best['control'],'evaluations':len(cache),
            'baseline':base,'candidate':cal,'baseline_train':float(btrain),'candidate_train':float(atrain),
            'baseline_heldout':float(bval),'candidate_heldout':float(aval),
            'accepted_by_holdout_gate':accepted,
            'notes':['Score is a frequency/decay proxy, not a perceptual similarity rating.',
                     'No calibration reference audio is used during real-time synthesis.',
                     'Mismatched recording chains and single takes limit generalisation.']}


def main():
    p=argparse.ArgumentParser();p.add_argument('--render',type=Path,required=True)
    p.add_argument('--out',type=Path,default=Path('calibration-results'))
    p.add_argument('--maxfev',type=int,default=22)
    p.add_argument('--skip-electric',action='store_true')
    args=p.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    refs=get_references(out/'references',not args.skip_electric)
    results={}
    for kind in ('electric','steel','nylon'):
        if kind not in refs or len(refs[kind])<3:
            print('WARNING insufficient samples for',kind);continue
        try:
            r=calibrate_kind(args.render.resolve(),kind,refs[kind],out/'renders',args.maxfev)
            results[kind]=r
            print(f"RESULT {kind} baseline={r['baseline_heldout']:.3f} candidate={r['candidate_heldout']:.3f} accept={r['accepted_by_holdout_gate']} controls={r['fitted_controls']}",flush=True)
        except Exception as e:
            print('FAILED',kind,str(e),flush=True)
    if not results:raise SystemExit('No real references could be evaluated')
    (out/'real-calibration-metrics.json').write_text(json.dumps(results,indent=2,allow_nan=False),encoding='utf-8')
    (out/'CALIBRATION_SUMMARY.md').write_text('\n'.join([
        '# Real guitar recording calibration — experimental results',
        'This compares separately played notes and uses *held-out* notes before recommending parameters.',
        'Not a claim of indistinguishability. Meta references are lossy previews, not full-quality WAV.\n',
        '| Kind | Train notes | Held-out | Score before | Candidate | Accepted |',
        '|---|---|---|---:|---:|---|',
        *[f"| {k} | {v['train_notes']} | {v['heldout_notes']} | {v['baseline_heldout']:.3f} | {v['candidate_heldout']:.3f} | {'yes' if v['accepted_by_holdout_gate'] else 'no'} |" for k,v in results.items()],
        '', '## Sources',
        *[f"- {k}: {v['reference_source']} ({v['reference_license']})" for k,v in results.items()],
        '', '## Method', 'Onset-aligned attacks, early harmonic mix, harmonic decay slopes, relative pitch relaxation; no loudness or EQ equalisation of recordings. Audio files excluded from report artifact.',
    ]),encoding='utf-8')
    # Keep only derived metrics in downloadable artifacts, not third-party reference audio.
    print((out/'CALIBRATION_SUMMARY.md').read_text(),flush=True)

if __name__=='__main__':main()
