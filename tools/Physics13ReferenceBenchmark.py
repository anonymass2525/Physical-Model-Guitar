#!/usr/bin/env python3
"""Independently evaluate v1.3 electric/nylon *observation* changes.

Public notes are validation references, not matched-instrument calibration.
No third-party reference audio is exported. The same note, string, velocity,
and all synthesis parameters are held fixed across sensor ON/OFF variants.
"""
import argparse, json, subprocess, tempfile
from pathlib import Path
import numpy as np
import soundfile as sf
from Physics06ReferenceBenchmark import gather
from CalibratePhysicalModel import features, distance, SRC

def rms(path):
    audio,_=sf.read(path,always_2d=True)
    return float(np.sqrt(np.mean(audio**2)))

def main(args):
    args.out.mkdir(parents=True,exist_ok=True)
    rows=[]
    with tempfile.TemporaryDirectory() as temp:
        root=Path(temp)
        for take in gather(root):
            if take['kind'] not in ('electric','nylon','steel'):continue
            kind=take['kind']
            model={'electric':1,'steel':2,'nylon':3}[kind]
            reference=features(take['path'],take['midi'])
            row={'model':kind,'midi':take['midi'],'string':take['string'],
                 'fret':take['fret'],'cases':{}}
            for case in ('v12','magnetic13','radiation13','both13'):
                path=root/'render.wav'
                mag=int(case in ('magnetic13','both13'))
                rad=int(case in ('radiation13','both13'))
                cmd=[str(args.render),'--single',str(model),str(take['midi']),'.65',str(path),
                    f'--string={take["string"]}','--physics12=1',
                    f'--magnetic13={mag}',f'--radiation13={rad}',
                    '--bridge-measured08=0']
                subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL)
                score,parts=distance(reference,features(path,take['midi']))
                row['cases'][case]={'score':float(score),'parts':parts,'rms':rms(path)}
            rows.append(row)
            print('BENCH',kind,take['midi'],f'{row["cases"]["v12"]["score"]:.4f}',
                  '->',f'{row["cases"]["both13"]["score"]:.4f}',flush=True)
    summary={}
    for kind in ('electric','nylon','steel'):
        subset=[r for r in rows if r['model']==kind]
        if not subset:continue
        baseline=np.array([r['cases']['v12']['score'] for r in subset]);
        candidate=np.array([r['cases']['both13']['score'] for r in subset]);
        summary[kind]={'n':len(subset),'v12':float(np.mean(baseline)),
                       'v13':float(np.mean(candidate)),
                       'relative_change_percent':float(100*(np.mean(candidate)/np.mean(baseline)-1)),
                       'improved_count':int(np.sum(candidate<baseline)),
                       'independence_check': all(abs(r['cases']['v12']['score']-
                                     r['cases']['radiation13' if kind=='electric' else 'magnetic13']['score'])<1e-10
                                     for r in subset) if kind!='steel' else True,
                       'sensor_only_measured':True}
    report={'summary':summary,'records':rows,'reference_sources':SRC,
        'interpretation':'Selected temporal/partial/pitch features; not a listening realism metric or measured absolute transducer constants.',
        'limitations':['Only up to four takes per model from heterogeneous public corpora.',
          'Reference instruments and pickup/room/processing chains not matched to synthesizer models.',
          'Electric uses DI; Nylon and steel use lossy OGG preview recordings.',
          'No randomized blinded perceptual listening carried out by CI.',
          'No instrument-level radiated power back-action identified.',
          'Do not enable v1.3 defaults solely from this benchmark.']}
    (args.out/'physics13_reference_scores.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    lines=['# v1.3 observation-only reference comparison','','Lower feature error is closer to selected recording features; NOT established perceptual quality.','','| Model | Notes | v1.2 | v1.3 | Difference | Improved notes |','|---|---:|---:|---:|---:|---:|']
    for kind,q in summary.items():
        lines.append(f'| {kind} | {q["n"]} | {q["v12"]:.3f} | {q["v13"]:.3f} | {q["relative_change_percent"]:+.1f}% | {q["improved_count"]}/{q["n"]} |')
    lines+=['','The acoustic observation signal does not feed back physical radiation loss. Sensor parameters remain unmeasured research approximations.']
    (args.out/'PHYSICS13_REFERENCE_SUMMARY.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
    print('SUMMARY',json.dumps(summary),flush=True)
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--render',type=Path,required=True);ap.add_argument('--out',type=Path,required=True);main(ap.parse_args())
