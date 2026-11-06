#!/usr/bin/env python3
"""Nylon 1.5 vs 1.6 feature proxy, including late envelope; not blinded listening."""
import argparse, json, subprocess, tempfile
from pathlib import Path
import numpy as np
from CalibratePhysicalModel import features,distance,download,META,SRC
from LargeCorpusCalibrator import STRINGS,TUNING,name_for

def notes(root):
    # Holdout notes are independent of pressure/bridge impulse data, but the
    # guitar and capture chain differ from the single measured Guitar #1.
    positions=[(3,9),(3,12),(4,5),(4,7),(4,9),(4,12),
               (5,0),(5,3),(5,5),(5,7),(5,9),(5,12)]
    result=[]
    for s,fret in positions:
        midi=TUNING[s]+fret
        fn=f'INSGuitarNylon_String{STRINGS[s]}_Vel02Pick_{name_for(midi)}_01.ogg'
        path=root/'reference'/fn
        try:download(f'{META}/meta-nylon-guitar/ogg/{fn}',path)
        except Exception as ex:
            print('UNAVAILABLE',fn,str(ex)[:90],flush=True);continue
        result.append((s,fret,midi,path))
    return result

def band_rms(feat):
    # Report the late body/string ratio proxy from onset-aligned frame 5.
    a=feat['attack'];return {'first_15ms_db':a[0],'140_280ms_db':a[4],
                             '280_550ms_db':a[5]}

def main(a):
    a.out.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory() as d:
        td=Path(d);records=[]
        for s,fret,midi,ref in notes(td):
            reference=features(ref,midi);outs={}
            for name,enabled in [('v15',0),('v16',1)]:
                wav=td/f'{name}_{s}_{fret}.wav'
                cmd=[str(a.render),'--single','3',str(midi),'.65',str(wav),
                  f'--string={s}','--physics12=1','--radiation15=1',
                  f'--radiation16={enabled}','--late-body16=.5','--direct-phase15=.5']
                subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL)
                generated=features(wav,midi);score,details=distance(reference,generated)
                outs[name]={'score':score,'components':details,
                            'late_envelope':band_rms(generated)}
            records.append(dict(string=s,fret=fret,midi=midi,
                                high_register=midi>=69,cases=outs))
            print('BENCH',midi,round(outs['v15']['score'],4),round(outs['v16']['score'],4),flush=True)
        summary={}
        for name in ('all','high','lower'):
            group=[x for x in records if name=='all' or x['high_register']==(name=='high')]
            if not group:continue
            d={'count':len(group)}
            for version in ('v15','v16'):
                d[version+'_mean']=float(np.mean([x['cases'][version]['score'] for x in group]))
                for feature in ('attack_db','harmonic_decay_db_per_second','early_harmonic_mix_db'):
                    d[version+'_'+feature]=float(np.mean([x['cases'][version]['components'][feature] for x in group]))
            d['v16_better_count']=sum(x['cases']['v16']['score']<x['cases']['v15']['score'] for x in group)
            d['change_percent']=100*(d['v16_mean']/d['v15_mean']-1)
            summary[name]=d
        report={'summary':summary,'notes':records,'source':SRC,
            'limitations':'Different guitar, microphone positions and lossy OGG; not absolute late-body fit validation or subjective listening. Impulse pole fit and note benchmark share no notes but both measured chains differ.',
            'raw_audio_distributed':False,'default_enabled':False}
        (a.out/'PHYSICS16_NOTES.json').write_text(json.dumps(report,indent=2))
        lines=['# 1.6 Late Body Modes — independent pluck-note evaluation','',
            'Lower acoustic-feature score indicates closer *proxy* match, not perceptual realism.',
            '','| Group | N | v1.5 | v1.6 | Change | Improved |',
            '|---|---:|---:|---:|---:|---:|']
        for key,v in summary.items():
            lines.append(f"| {key} | {v['count']} | {v['v15_mean']:.3f} | {v['v16_mean']:.3f} | {v['change_percent']:+.2f}% | {v['v16_better_count']}/{v['count']} |")
        (a.out/'SUMMARY_JA.md').write_text('\n'.join(lines)+'\n')
        print('SUMMARY',json.dumps(summary),flush=True)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--render',type=Path,required=True);p.add_argument('--out',type=Path,required=True);main(p.parse_args())
