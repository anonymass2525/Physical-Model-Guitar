#!/usr/bin/env python3
"""Mechanism-specific reference comparison. Not blinded perception or material identification.

All cases render the same reference take and physical string. The v1.1 hybrid
has a separately generated legacy dry sound; the v1.2 unified path does not.
The difference is deliberately reported, not hidden by loudness compensation.
"""
import argparse, json, subprocess, tempfile, math
from pathlib import Path
import numpy as np
from Physics06ReferenceBenchmark import gather
from CalibratePhysicalModel import features, distance, SRC

CASES = {
    "hybrid11_full": {"physics12":0},
    "hybrid11_no_pick": {"physics12":0,"pick12":0},
    "hybrid11_no_fret": {"physics12":0,"fret12":0},
    "hybrid11_no_axial": {"physics12":0,"axial12":0},
    "hybrid11_no_pickup": {"physics12":0,"pickup12":0},
    "unified12_full": {"physics12":1},
    "unified12_no_pick": {"physics12":1,"pick12":0},
    "unified12_no_fret": {"physics12":1,"fret12":0},
    "unified12_no_axial": {"physics12":1,"axial12":0},
    "unified12_no_pickup": {"physics12":1,"pickup12":0},
}

def main(args):
    args.out.mkdir(parents=True,exist_ok=True)
    records=[]
    with tempfile.TemporaryDirectory() as t:
        folder=Path(t)
        references=gather(folder)
        for i,take in enumerate(references):
            kind=take['kind']
            if kind not in ('electric','steel','nylon'):continue
            model={'electric':1,'steel':2,'nylon':3}[kind]
            ref=features(take['path'],take['midi'])
            row={k:take[k] for k in ('kind','string','fret','midi')}
            row['results']={}
            for variant,override in CASES.items():
                wav=folder/'synthetic.wav'
                options={"physics06":1,"physics07":1,"physics08":1,"physics09":1,
                    "physics10":1,"physics11":1,"physics12":1,
                    "pick12":1,"fret12":1,"axial12":1,"pickup12":1}
                options.update(override)
                cmd=[str(args.render),'--single',str(model),str(take['midi']),'.65',str(wav),
                    f'--string={take["string"]}', '--bridge-measured08=0',
                    *[f'--{k}={v}' for k,v in options.items()]]
                subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL)
                try:
                    score,detail=distance(ref,features(wav,take['midi']))
                    row['results'][variant]={'score':float(score),'detail':detail}
                except ValueError as exc:
                    if 'silent reference' not in str(exc):
                        raise
                    row['results'][variant]={'score':None,'detail':{'status':'silent'}}
            records.append(row)
            print('COMPARED',i,kind,row['midi'],
                  'hybrid',round(row['results']['hybrid11_full']['score'],3),
                  'unified',round(row['results']['unified12_full']['score'],3),flush=True)
    summary={}
    for kind in ('electric','steel','nylon'):
        samples=[x for x in records if x['kind']==kind]
        if not samples: continue
        metrics={}
        for v in CASES:
            numbers=[x['results'][v]['score'] for x in samples]
            valid=[x for x in numbers if x is not None]
            metrics[v]=float(np.mean(valid)) if len(valid)==len(numbers) else None
        full=metrics['hybrid11_full']
        # Independent mechanism contribution to the 1.1 misfit, not physical
        # proof of causation. The other three modules remain enabled.
        diff={k:(round(metrics[k]-full,4) if metrics[k] is not None else None)
              for k in CASES if k.startswith('hybrid11_no_')}
        summary[kind]={'count':len(samples),'scores':metrics,'hybrid_ablation_delta':diff,
            'unified_beats_hybrid':metrics['unified12_full']<metrics['hybrid11_full']}
    report={'source':SRC,'summary':summary,'notes':records,'n_cases':len(CASES),
        'scope':'Independent external single-note recordings. 1.1 hybrid vs 1.2 exclusive unified output; separately ablate pick, fret, reduced-order axial and non-loading pickup readout.',
        'limitations':'Not matched to same instrument, microphone, player, dynamics, or sampling format; 4 notes/model only. Abruptly removing a mechanism changes excitation energy. Values cannot be interpreted as proved physical-causal decomposition, perceptual realism, or parameter calibration. Do not auto-enable an ablation from these data.'}
    (args.out/'physics12_ablation.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
    rows=['# Physical Model Guitar 1.2 — mechanism ablation against public recordings','','Scores are lower-is-closer *selected features*, not listener preference.','','| Model | 1.1 hybrid | 1.2 unified | Unified better? |','|---|---:|---:|---|']
    for model,row in summary.items():
        sc=row['scores']
        rows.append(f'| {model} | {sc["hybrid11_full"]:.3f} | {sc["unified12_full"]:.3f} | {"yes" if row["unified_beats_hybrid"] else "no"} |')
    rows+=['','## Mechanism-off effects in 1.1 hybrid','', 'Positive delta: turning mechanism OFF increased reference error. Negative: turning OFF improved fit.','','| Model | No pick | No fret | No axial | No pickup |','|---|---:|---:|---:|---:|']
    for model,row in summary.items():
        d=row['hybrid_ablation_delta']
        rows.append('| '+model+' | '+' | '.join(
            f'{d["hybrid11_no_"+k]:+.3f}' if d['hybrid11_no_'+k] is not None else 'silent'
            for k in ('pick','fret','axial','pickup'))+' |')
    rows+=['','The reported score does not identify real material constants, and does not replace blind listening.',report['limitations']]
    (args.out/'PHYSICS12_ABLATION_SUMMARY.md').write_text('\n'.join(rows)+'\n',encoding='utf-8')
    print('SUMMARY',json.dumps(summary),flush=True)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--render',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    main(p.parse_args())
