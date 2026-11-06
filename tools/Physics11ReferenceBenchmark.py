#!/usr/bin/env python3
"""Controlled feature comparison of 1.0-vs-1.1 on *external recordings*.
Models are not the same specific instruments as the references. Not a realism score.
"""
import argparse, json, subprocess, tempfile
from pathlib import Path
import numpy as np
from Physics06ReferenceBenchmark import gather
from CalibratePhysicalModel import features, distance, SRC

def main(args):
    args.out.mkdir(parents=True,exist_ok=True)
    notes=[]
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp)
        for take in gather(root):
            model={'electric':1,'steel':2,'nylon':3}[take['kind']]
            featureRef=features(take['path'],take['midi'])
            scores={}
            for name,enabled in [('v10',0),('v11',1)]:
                output=root/'synth.wav'
                subprocess.run([str(args.render),'--single',str(model),str(take['midi']),'.65',str(output),
                    f'--string={take["string"]}','--physics06=1','--physics07=1','--physics08=1',
                    '--physics09=1','--physics10=1',f'--physics11={enabled}','--bridge-measured08=0'],
                    check=True,stdout=subprocess.DEVNULL)
                score,detail=distance(featureRef,features(output,take['midi']))
                scores[name]={'score':float(score),'detail':detail}
            rec={k:take[k] for k in ('kind','string','fret','midi')}
            rec.update(scores)
            rec['difference']=scores['v11']['score']-scores['v10']['score']
            notes.append(rec)
            print(f"{rec['kind']} midi{rec['midi']} {scores['v10']['score']:.5f} -> {scores['v11']['score']:.5f}",flush=True)
    summary={}
    for kind in ('electric','steel','nylon'):
        samples=[x for x in notes if x['kind']==kind]
        if samples:
            summary[kind]={'count':len(samples),'v10_mean':float(np.mean([x['v10']['score'] for x in samples])),
                'v11_mean':float(np.mean([x['v11']['score'] for x in samples])),
                'better_count':sum(x['difference']<0 for x in samples)}
    report={'source':SRC,'summary':summary,'notes':notes,
            'scope':'1.0 power-wave subsystem OFF/ON of v1.1 contact/axial/pickup - parallel legacy dry renderer unchanged',
            'limitations':'Small unmatched electric/steel/nylon sources. 1.0 and v1.1 compared with identical parameters; scores measure only selected features, not perception or material identification. Energy ledger covers new wave subsystem, not parallel legacy DSP.'}
    (args.out/'physics11_reference_benchmark.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print('SUMMARY',json.dumps(summary),flush=True)
if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--render',type=Path,required=True);parser.add_argument('--out',type=Path,required=True)
    main(parser.parse_args())
