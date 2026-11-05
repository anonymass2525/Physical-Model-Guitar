#!/usr/bin/env python3
"""Physical Model Guitar v0.7: controlled, feature-distance comparison against real recordings.

This does NOT claim perceptual or physical-identification equivalence. The same
reference is scored against v0.6 (legacy) and v0.7 (new distributed contacts).
"""
import argparse, json, tempfile, subprocess
from pathlib import Path
import numpy as np
from Physics06ReferenceBenchmark import gather
from CalibratePhysicalModel import features, distance, SRC

def run(args):
    args.out.mkdir(parents=True,exist_ok=True)
    records=[]
    with tempfile.TemporaryDirectory() as temp:
        root=Path(temp)
        for take in gather(root):
            ref=features(take['path'],take['note'])
            scores={}
            model={'electric':1,'steel':2,'nylon':3}[take['kind']]
            for name,enabled in [('physics06',False),('physics07',True)]:
                out=root/'render.wav'
                subprocess.run([str(args.render),'--single',str(model),str(take['note']),'.65',str(out),
                                f'--string={take["string"]}','--physics06=1',
                                f'--physics07={int(enabled)}'],check=True,stdout=subprocess.DEVNULL)
                score,detail=distance(ref,features(out,take['note']))
                scores[name]={'score':float(score),'components':detail}
            rec={'kind':take['kind'],'string':take['string'],'fret':take['fret'],
                 'midi':take['note'],**scores,
                 'delta':scores['physics07']['score']-scores['physics06']['score']}
            records.append(rec)
            print(f"BENCH {rec['kind']} str{rec['string']} note{rec['midi']} "
                  f"{scores['physics06']['score']:.4f} -> {scores['physics07']['score']:.4f}",flush=True)
    report={}
    for kind in ('electric','steel','nylon'):
        seq=[r for r in records if r['kind']==kind]
        if seq:
            report[kind]={"notes":len(seq),"mean06":float(np.mean([r['physics06']['score'] for r in seq])),
                          "mean07":float(np.mean([r['physics07']['score'] for r in seq])),
                          "improved":int(sum(r['delta']<0 for r in seq)),
                          "unchanged":int(sum(abs(r['delta'])<1.e-7 for r in seq))}
    data={'method':'onset / partial balance / harmonic decay / pitch; not perceived realism',
          'source':SRC,'recordings_embedded':False,'records':records,'summary':report,
          'caveats':['Reference recordings were made on different instruments, capture chains and attack intensities.',
                     'Not a same-instrument matched set. Cannot identify physical parameters uniquely.',
                     'This benchmark examines only individual plucks. Repeated note carryover needs a separate test.',
                     'A reduction in this objective distance cannot establish perceptual superiority.']}
    (args.out/'physics07_vs_06.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
    print('RESULTS',json.dumps(report),flush=True)

if __name__=='__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('--render',type=Path,required=True)
    ap.add_argument('--out',type=Path,required=True)
    run(ap.parse_args())
