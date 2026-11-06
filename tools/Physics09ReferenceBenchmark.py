#!/usr/bin/env python3
"""Actual clean guitar recordings vs v0.8/v0.9 power-wave bridge.
Same external recordings as v0.8. Source instruments are NOT the same as
bridge-measurement archive guitars. Scores are not human realism ratings.
"""
import argparse,json,tempfile,subprocess
from pathlib import Path
import numpy as np
from Physics06ReferenceBenchmark import gather
from CalibratePhysicalModel import features,distance,SRC

def run(args):
    args.out.mkdir(parents=True,exist_ok=True)
    results=[]
    with tempfile.TemporaryDirectory() as td:
        root=Path(td)
        for take in gather(root):
            model={'electric':1,'steel':2,'nylon':3}[take['kind']]
            ref=features(take['path'],take['midi'])
            scores={}
            for label,enabled in [('0.8',0),('0.9',1)]:
                fp=root/'rendered.wav'
                subprocess.run([str(args.render),'--single',str(model),str(take['midi']),'.65',str(fp),
                                f'--string={take["string"]}','--physics06=1','--physics07=1','--physics08=1',
                                f'--physics09={enabled}','--bridge-measured08=0'],check=True,stdout=subprocess.DEVNULL)
                score,detail=distance(ref,features(fp,take['midi']))
                scores[label]={'score':float(score),'details':detail}
            rec={k:take[k] for k in ['kind','string','fret','midi']}
            rec.update(scores);rec['delta']=scores['0.9']['score']-scores['0.8']['score'];results.append(rec)
            print(f"NOTE {rec['kind']} str{rec['string']} midi{rec['midi']} {scores['0.8']['score']:.4f}->{scores['0.9']['score']:.4f}",flush=True)
    summary={}
    for kind in ('electric','steel','nylon'):
        batch=[x for x in results if x['kind']==kind]
        if batch:
            summary[kind]={'n':len(batch),'mean08':float(np.mean([x['0.8']['score'] for x in batch])),
                           'mean09':float(np.mean([x['0.9']['score'] for x in batch])),
                           'improved':sum(1 for x in batch if x['delta']<0)}
    report={'source':SRC,'source_instrument':'Not identical to archival body measurements',
            'warning':'Small, unmatched-instrument external dataset; objective features only. New two-port bridge is not physically calibrated in SI wave variables.',
            'energy_boundary':'Exact for six virtual velocity-wave ports and ten passive bridge modes; old displacement waveguide still approximate.',
            'summary':summary,'records':results}
    (args.out/'physics09_reference_benchmark.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('SUMMARY',json.dumps(summary),flush=True)

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--render',type=Path,required=True);ap.add_argument('--out',type=Path,required=True)
    run(ap.parse_args())
