#!/usr/bin/env python3
"""Physical Model Guitar: larger-corpus, string-aware, held-out calibration.

Not a machine-learning instrument. References are transient calibration inputs
and never included with VST3/installer. The sampled guitars are single-take,
not sufficient to uniquely identify material parameters; claims are restricted
to the recorded acoustic features and the held-out distribution.

Source: Meta Open Sound steel/nylon (CC0 preview), and optionally HF guitar-
fretboard-notes (CC BY-SA) electric DI. See ANALYSIS_LIMITATIONS_JA.md.
"""
from __future__ import annotations
import argparse, json, math, subprocess, time, urllib.parse, urllib.request
from collections import defaultdict
from pathlib import Path
import numpy as np
from CalibratePhysicalModel import features, distance, download, META, HFACE, SRC

TUNING = [40,45,50,55,59,64]
STRINGS = ['LowE','A','D','G','B','HighE']
PITCH_CLASS=['C','Cs','D','Ds','E','F','Fs','G','Gs','A','As','B']
# train and held-out frets are disjoint; hold out extreme velocity/touch.
FRETS_TRAIN=[0,3,7]
FRETS_TEST=[5,9]
KINDS=['electric','steel','nylon']

def name_for(midi):return PITCH_CLASS[midi%12]+str(midi//12-1)


def meta_samples(kind,root,max_samples=72):
    assert kind in ('steel','nylon')
    coll=f'meta-{kind}-guitar'; prefix='Steel' if kind=='steel' else 'Nylon'
    layers=('Vel02', 'Vel01', 'Vel03') if kind=='steel' else ('Vel02Pick','Vel01Finger','Vel03Pick')
    takes=[]
    for s in range(6):
        for fret in FRETS_TRAIN+FRETS_TEST:
            midi=TUNING[s]+fret
            for layeridx,layer in enumerate(layers):
                split='train' if fret in FRETS_TRAIN and layeridx==0 else 'holdout'
                # To limit download cost, hold out one of two alt velocity layers per fret.
                if split=='holdout' and layeridx != (1 if fret==FRETS_TEST[0] else 2):continue
                if fret in FRETS_TRAIN and layeridx!=0:continue
                filename=f'INSGuitar{prefix}_String{STRINGS[s]}_{layer}_{name_for(midi)}_01.ogg'
                url=f'{META}/{coll}/ogg/{filename}'
                dest=root/kind/filename
                try:
                    download(url,dest)
                    takes.append(dict(kind=kind,string=s,midi=midi,fret=fret,velocity=(.65 if layeridx==0 else (.34 if layeridx==1 else .90)),
                                      layer=layer,split=split,path=str(dest),url=url))
                except Exception as exc:
                    print('MISSING',kind,s,fret,layer,str(exc)[:120],flush=True)
                if len(takes)>=max_samples:return takes
    return takes


def electric_samples(root,max_samples=72):
    # Metadata comes from the HF public Dataset Viewer. Record player ID; don't
    # treat different pickup/recording chains as the same instrument without proof.
    allrows=[]
    for offset in range(0,400,100):
        qs=urllib.parse.urlencode(dict(dataset='collegefishiesd/guitar-fretboard-notes',config='default',split='train',offset=offset,length=100))
        try:
            req=urllib.request.Request(HFACE+'/rows?'+qs,headers={'User-Agent':'PMG-PhysicalCalibration-v0.5'})
            result=json.load(urllib.request.urlopen(req,timeout=70))
        except Exception as exc:
            print('WARNING HF row fetch',offset,str(exc)[:120],flush=True);break
        allrows.extend([r.get('row',{}) for r in result.get('rows',[])])
        if offset+100>=result.get('num_rows_total',0):break
    out=[]
    eligible=[]
    for row in allrows:
        if row.get('source')!='ele':continue
        s=int(row.get('string_number',0))
        fret=int(row.get('fret',-1));midi=int(row.get('midi_number',-1))
        if not 1<=s<=6 or fret not in FRETS_TRAIN+FRETS_TEST:continue
        if TUNING[6-s]+fret!=midi:continue  # dataset 1 = high E, 6 = low E
        aud=row.get('audio') or []
        if isinstance(aud,dict):aud=[aud]
        src=next((a.get('src') for a in aud if isinstance(a,dict) and a.get('src')),None)
        if not src:continue
        eligible.append((6-s,fret,int(row.get('player_id',-1)),midi,src))
    # The same player on each string is preferable to mixing instruments.
    byplayer=defaultdict(list)
    for item in eligible:byplayer[item[2]].append(item)
    if byplayer:
        player=max(byplayer, key=lambda p:len({(e[0],e[1]) for e in byplayer[p]}))
        eligible=byplayer[player]
    for s,fret,player,midi,src in sorted(eligible):
        split='train' if fret in FRETS_TRAIN else 'holdout'
        dest=root/'electric'/f'player{player}_string{s}_fret{fret}.wav'
        try:download(src,dest)
        except Exception as exc:
            print('MISSING electric',s,fret,str(exc)[:120],flush=True);continue
        out.append(dict(kind='electric',string=s,midi=midi,fret=fret,velocity=.65,layer='single_DI',split=split,
                        player_id=player,path=str(dest),url=SRC['electric']))
        if len(out)>=max_samples:break
    return out


def fit_controls(s,kind, contact=1.,loss=0.,pickup=0.,body_freq=1.,body_q=1.,body_gain=1.):
    c={f'contact{s}':round(float(contact),5),f'pickup-offset{s}':round(float(pickup),5)}
    # Monotone loss across harmonic bands; bounded passivity-preserving setting.
    factors=(.00,.19,.53,1.00)
    for b,f in enumerate(factors):c[f'loss{s}{b}']=round(float(loss*f),6)
    if kind!='electric':
        for i in (0,1,2):
            c[f'mode{i}-freq']=round(float(body_freq),5)
            c[f'mode{i}-q']=round(float(body_q),5)
            c[f'mode{i}-gain']=round(float(body_gain),5)
    return c


def run_render(exe,take,params,out,cache):
    key=(take['kind'],take['string'],take['midi'],take['velocity'],tuple(sorted(params.items())))
    if key in cache:return cache[key]
    out.mkdir(parents=True,exist_ok=True)
    dest=out / ('r_'+str(len(cache))+'.wav')
    model=KINDS.index(take['kind'])+1
    cmd=[str(exe),'--single',str(model),str(take['midi']),str(take['velocity']),str(dest),f"--string={take['string']}"]
    cmd.extend([f'--{name}={v}' for name,v in sorted(params.items())])
    proc=subprocess.run(cmd,text=True,capture_output=True)
    if proc.returncode:
        raise RuntimeError('render failed '+str(proc.returncode)+' '+proc.stderr[:150])
    ft=features(dest,take['midi'])
    dest.unlink(missing_ok=True)
    cache[key]=ft
    return ft


def evaluate(exe,takes,params,out,cache,ref):
    result=[]
    for take in takes:
        k=take['id']
        gen=run_render(exe,take,params,out,cache)
        score,comp=distance(ref[k],gen)
        result.append((float(score),comp))
    if not result:return None
    return {'score':float(np.mean([x[0] for x in result])),
            'attack':float(np.mean([x[1]['attack_db'] for x in result])),
            'slopes':float(np.mean([x[1]['harmonic_decay_db_per_second'] for x in result])),
            'mix':float(np.mean([x[1]['early_harmonic_mix_db'] for x in result])),
            'pitch':float(np.mean([x[1]['pitch_relaxation_cents'] for x in result])),
            'notes':len(result)}


def calibrate(exe,takes,out,max_takes=None):
    if max_takes:takes=takes[:max_takes]
    kind=takes[0]['kind']
    result={'kind':kind,'license':('CC0-1.0' if kind!='electric' else 'CC-BY-SA-4.0'),
            'source':SRC[kind],'reference_format':'OGG mono lossy preview' if kind!='electric' else 'electric DI WAV',
            'selection':{'fret_train':FRETS_TRAIN,'fret_holdout':FRETS_TEST,'layer_train':'medium',
                         'layer_holdout':'alternate velocity/technique' if kind!='electric' else 'single'},
            'samples':len(takes),'sample_count_by_string':{},'candidate_per_string':{},'accepted_per_string':{},
            'shared_body':{'status':'configurable_not_identifiable_from_single_on-axis_sample'},
            'limitations':['No separate mechanical contact sensor: inferred parameter is an equivalent contact stiffness, not measured N/m.',
                           'Body modes and pickup tone are confounded with microphone/electronics.',
                           'Only one take per string/note/layer in Meta sample library.',
                           'Held-out fret and dynamic are required for adoption.']}
    ref={}
    for i,t in enumerate(takes):
        t['id']=i
        ref[i]=features(t['path'],t['midi'])
    cache={}; rend=out/'temporary_renders'
    for s in range(6):
        tr=[t for t in takes if t['string']==s and t['split']=='train']
        va=[t for t in takes if t['string']==s and t['split']=='holdout']
        result['sample_count_by_string'][str(s)]={'train':len(tr),'holdout':len(va)}
        if len(tr)<2 or len(va)<1:continue
        default=fit_controls(s,kind)
        base_train=evaluate(exe,tr,default,rend,cache,ref)
        base_hold=evaluate(exe,va,default,rend,cache,ref)
        params={'contact':1.,'loss':0.,'pickup':0.}
        record=[]
        # Coordinate search, no holdout used here. Regularized to neutral priors.
        steps=[('contact',[.72,.86,1.,1.16,1.36],.15),
               ('loss',[0.,.004,.010,.022,.040],.35)]
        if kind=='electric':steps.append(('pickup',[-.035,-.018,0.,.018,.035],.40))
        for name,candidates,penalty in steps:
            chosen=params[name];best=float('inf')
            for v in candidates:
                c=dict(params);c[name]=v
                scores=evaluate(exe,tr,fit_controls(s,kind,**c),rend,cache,ref)
                regularize = penalty*(v if name!='contact' else v-1.)**2
                objective=scores['score']+regularize
                record.append({'parameter':name,'value':v,'train_score':scores['score']})
                if objective<best:best=objective;chosen=v
            params[name]=chosen
        candidate=fit_controls(s,kind,**params)
        cand_train=evaluate(exe,tr,candidate,rend,cache,ref)
        cand_hold=evaluate(exe,va,candidate,rend,cache,ref)
        # Score improvement must generalise. Attack may not deteriorate materially.
        accept=bool(cand_train['score']<base_train['score']*.985
                    and cand_hold['score']<base_hold['score']*.97
                    and cand_hold['attack']<=base_hold['attack']*1.12)
        result['candidate_per_string'][str(s)]={'params':params,'baseline_train':base_train,'candidate_train':cand_train,
            'baseline_holdout':base_hold,'candidate_holdout':cand_hold,'search':record,'accepted':accept}
        result['accepted_per_string'][str(s)]=accept
        print(f'STRING {kind} {s+1}/6 train {base_train["score"]:.3f}->{cand_train["score"]:.3f} '
              f'holdout {base_hold["score"]:.3f}->{cand_hold["score"]:.3f} accepted={accept}',flush=True)
    # Shared body mode frequency/Q/gain is exposed and bounded in DSP but intentionally
    # not fitted here: the particular mono mic positions cannot independently identify
    # body modes vs radiated transfer function. Distinguish adjustable from identified.
    result['renders_cached']=len(cache)
    n=sum(x['train']+x['holdout'] for x in result['sample_count_by_string'].values())
    if n<12:result['warning']='Fewer than 12 usable references; not reliable.'
    return result


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--render',type=Path,required=True)
    ap.add_argument('--out',type=Path,default=Path('expanded-calibration-results'))
    ap.add_argument('--models',nargs='+',choices=KINDS,default=KINDS)
    ap.add_argument('--max-samples',type=int,default=72)
    ap.add_argument('--dry-run',action='store_true')
    args=ap.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    results={}
    for kind in args.models:
        try:
            print('COLLECT',kind,flush=True)
            takes=electric_samples(out/'references',args.max_samples) if kind=='electric' else meta_samples(kind,out/'references',args.max_samples)
            print(f'COLLECTED {kind} {len(takes)} references; {sum(t["split"]=="train" for t in takes)} training; {sum(t["split"]=="holdout" for t in takes)} holdout',flush=True)
            if args.dry_run:continue
            if len(takes)<8:raise RuntimeError('too few real recordings for reproducible per-string calibration')
            res=calibrate(args.render.resolve(),takes,out/kind)
            results[kind]=res
        except Exception as e:
            print('FAILED',kind,repr(e),flush=True)
            results[kind]={'kind':kind,'failed':str(e),'samples':0}
    (out/'report.json').write_text(json.dumps(results,indent=2,ensure_ascii=False,allow_nan=False),encoding='utf-8')
    lines=['# Larger-corpus, string-specific calibration — research experiment',
           'No audio files distributed. Only fitted metrics and instrument-independent DSP source.',
           '| Kind | References | Strings fitted | String holdout passes |','|---|---:|---:|---:|']
    for kind,res in results.items():
        fitted=len(res.get('candidate_per_string',{})); acc=sum(res.get('accepted_per_string',{}).values())
        lines.append(f'| {kind} | {res.get("samples",0)} | {fitted} | {acc} |')
    lines += ['','The accepted flag indicates reduced held-out *feature error*, not proof of better human-perceived realism.',
              'Body mode identification requires separately measured bridge impedance or multi-position microphone responses.',
              'The new controls do not alter old v0.4 sound when neutral. No presets auto-applied.']
    (out/'SUMMARY.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print('\n'.join(lines),flush=True)
    if not any(len(v.get('candidate_per_string',{})) for v in results.values()):raise SystemExit('No successful per-string calibration fits')

if __name__=='__main__': main()
