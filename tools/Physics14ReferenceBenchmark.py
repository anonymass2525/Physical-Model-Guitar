#!/usr/bin/env python3
"""External-note measurement: independent 1.3 versus 1.4 observer variants.
References are never shipped. No tuning is done on the evaluated notes.
Sources are mismatched guitars/recording chains, and Meta files are lossy OGG.
"""
import argparse,json,subprocess,tempfile
from pathlib import Path
import numpy as np
from scipy import signal
from CalibratePhysicalModel import features,distance,download,META,SRC
from LargeCorpusCalibrator import electric_samples,STRINGS,TUNING,name_for


def nylon_notes(root):
    # Include upper-string, upper-fret notes unavailable in the earlier fret-5
    # study. Last 2 positions are deliberately high-register hold-outs.
    candidates=[(3,9),(3,12),(4,5),(4,7),(4,9),(4,12),
                (5,0),(5,3),(5,5),(5,7),(5,9),(5,12)]
    out=[]
    for s,fret in candidates:
        midi=TUNING[s]+fret
        fn=f'INSGuitarNylon_String{STRINGS[s]}_Vel02Pick_{name_for(midi)}_01.ogg'
        path=root/'references'/'nylon'/fn
        try:
            download(f'{META}/meta-nylon-guitar/ogg/{fn}',path)
        except Exception as e:
            print('MISSING nylon',s,fret,str(e)[:85],flush=True);continue
        out.append(dict(kind='nylon',midi=midi,string=s,fret=fret,path=path,
                        subset='high' if midi>=69 else 'mid'))
    return out


def electric_notes(root):
    out=[]
    try:
        for take in electric_samples(root/'references'/'ele',max_samples=80):
            if take['string'] not in (0,1,2,3,4,5) or take['fret'] not in (0,3,5,7,9):continue
            out.append(dict(kind='electric',midi=take['midi'],string=take['string'],
                            fret=take['fret'],path=Path(take['path']),subset='upper' if take['midi']>=64 else 'lower'))
    except Exception as e:print('WARNING DI',str(e)[:200],flush=True)
    return out[:24]


def band_ratio(path):
    import soundfile as sf
    data,sr=sf.read(path,always_2d=True)
    x=data.mean(axis=1)
    # Absolute dB cancels within low/mid vs high ratio; OGG upper-band caveat.
    x=x[int(.05*sr):int(.4*sr)]
    if len(x)<1000:return None
    f,t,z=signal.stft(x,fs=sr,nperseg=2048,noverlap=1024)
    p=np.mean(np.abs(z)**2,axis=1)
    lo=np.sum(p[(f>=500)&(f<2000)])+1e-15
    hi=np.sum(p[(f>=2000)&(f<8500)])+1e-15
    return float(10*np.log10(hi/lo))


def main(args):
    args.out.mkdir(parents=True,exist_ok=True)
    results=[]
    with tempfile.TemporaryDirectory() as temp:
        temp=Path(temp)
        takes=electric_notes(temp)+nylon_notes(temp)
        for take in takes:
            kind=take['kind'];m=1 if kind=='electric' else 3
            ref=features(take['path'],take['midi'])
            measured=[]
            for label,mag13,rad13,mag14,rad14 in [('v12',0,0,0,0),('v13',1,1,0,0),('v14',1,1,1,1)]:
                wav=temp/(f'r_{kind}_{take["string"]}_{take["fret"]}_{label}.wav')
                cmd=[str(args.render),'--single',str(m),str(take['midi']),'.65',str(wav),
                     f'--string={take["string"]}','--physics12=1',
                     f'--magnetic13={mag13}',f'--radiation13={rad13}',
                     f'--magnetic14={mag14}',f'--radiation14={rad14}',
                     '--magnetic-attack14=0.5','--nylon-treble14=0.5']
                subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL)
                feat=features(wav,take['midi']);score,parts=distance(ref,feat)
                measured.append(dict(name=label,score=float(score),components=parts,
                                     high_to_mid_db=band_ratio(wav)))
            results.append(dict(kind=kind,midi=take['midi'],string=take['string'],
                                fret=take['fret'],subset=take['subset'],
                                cases={x['name']:x for x in measured}))
            print('BENCH',kind,take['midi'],take['fret'],' '.join(f"{x['name']}:{x['score']:.3f}" for x in measured),flush=True)
        summary={}
        for kind in ('electric','nylon'):
            sel=[r for r in results if r['kind']==kind]
            for subset in sorted(set(x['subset'] for x in sel)):
                a=[r for r in sel if r['subset']==subset]
                if not a:continue
                p=summary[f'{kind}_{subset}']={'count':len(a),'midi_range':[min(x['midi'] for x in a),max(x['midi'] for x in a)]}
                for c in ('v12','v13','v14'):
                    p[c+'_mean']=float(np.mean([x['cases'][c]['score'] for x in a]))
                p['v14_better_than_v13_count']=sum(x['cases']['v14']['score']<x['cases']['v13']['score'] for x in a)
                p['v14_vs_v13_percent']=float(100*(p['v14_mean']/max(p['v13_mean'],1e-9)-1))
                p['attack13_mean']=float(np.mean([x['cases']['v13']['components']['attack_db'] for x in a]))
                p['attack14_mean']=float(np.mean([x['cases']['v14']['components']['attack_db'] for x in a]))
                p['harmonicmix13_mean']=float(np.mean([x['cases']['v13']['components']['early_harmonic_mix_db'] for x in a]))
                p['harmonicmix14_mean']=float(np.mean([x['cases']['v14']['components']['early_harmonic_mix_db'] for x in a]))
        report={'summary':summary,'records':results,'reference_sources':SRC,
          'model_warning':'Unmatched guitars and DI/microphone chains; not a physical calibration or perceptual score.',
          'evaluated_hf':'Measured synthetic high-to-mid ratio not calibrated far-field directivity. Reference OGG is lossy.',
          'reference_audio_redistributed':False,
          'candidate_controls':{'magneticAttack14':.5,'nylonTreble14':.5},
          'not_selected_using_this_benchmark':True}
        (args.out/'physics14_reference_scores.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
        text=['# 1.4 Independent observers: high register and transient validation','',
              'Data are not matched-instrument recordings. Lower is better for this *feature proxy only*.','',
              '| Kind/subset | N | 1.2 | 1.3 | 1.4 | 1.4 vs 1.3 | Attack 1.3/1.4 | Harmonic mix 1.3/1.4 |',
              '|---|---:|---:|---:|---:|---:|---:|---:|']
        for k,v in summary.items():
            text.append(f"| {k} | {v['count']} | {v['v12_mean']:.3f} | {v['v13_mean']:.3f} | {v['v14_mean']:.3f} | {v['v14_vs_v13_percent']:+.1f}% | {v['attack13_mean']:.2f}/{v['attack14_mean']:.2f} | {v['harmonicmix13_mean']:.2f}/{v['harmonicmix14_mean']:.2f} |")
        (args.out/'PHYSICS14_REFERENCE_SUMMARY.md').write_text('\n'.join(text)+'\n',encoding='utf-8')
        print('SUMMARY',json.dumps(summary),flush=True)
if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('--render',type=Path,required=True);a.add_argument('--out',type=Path,required=True);main(a.parse_args())
