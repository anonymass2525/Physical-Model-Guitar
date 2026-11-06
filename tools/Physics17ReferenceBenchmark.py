#!/usr/bin/env python3
"""v1.6 vs phase-coherent v1.7; independent *guitar* reference notes, not
independently observed string/body source paths. The results cannot validate
source separation or absolute radiation SPL."""
import argparse,json,subprocess,tempfile
from pathlib import Path
import numpy as np
from CalibratePhysicalModel import features,distance,download,META,SRC
from LargeCorpusCalibrator import STRINGS,TUNING,name_for

def run(args):
    args.out.mkdir(parents=True,exist_ok=True)
    positions=[(3,9),(3,12),(4,5),(4,7),(4,9),(4,12),
               (5,0),(5,3),(5,5),(5,7),(5,9),(5,12)]
    rows=[]
    with tempfile.TemporaryDirectory() as d:
        d=Path(d)
        for string,fret in positions:
            note=TUNING[string]+fret
            fn=f'INSGuitarNylon_String{STRINGS[string]}_Vel02Pick_{name_for(note)}_01.ogg'
            src=d/fn
            try: download(f'{META}/meta-nylon-guitar/ogg/{fn}',src)
            except Exception as ex:
                print('MISSING',fn,str(ex)[:90],flush=True);continue
            target=features(src,note);result={}
            for v,on in [('v16',0),('v17',1)]:
                wav=d/f'{v}_{string}_{fret}.wav'
                cmd=[str(args.render),'--single','3',str(note),'.65',str(wav),
                     f'--string={string}','--physics12=1','--radiation15=1',
                     '--radiation16=1','--late-body16=.5',
                     f'--radiation17={on}','--direct-phase17=.5','--direct-mix17=.5']
                subprocess.run(cmd,stdout=subprocess.DEVNULL,check=True)
                score,parts=distance(target,features(wav,note))
                result[v]={'score':score,'components':parts}
            rows.append(dict(note=note,string=string,fret=fret,
                             high=note>=69,cases=result))
            print('COMPARE',note,result['v16']['score'],result['v17']['score'],flush=True)
    summary={}
    for group in ['all','high','other']:
        x=[r for r in rows if group=='all' or r['high']==(group=='high')]
        if not x:continue
        a=float(np.mean([r['cases']['v16']['score'] for r in x]))
        b=float(np.mean([r['cases']['v17']['score'] for r in x]))
        summary[group]={'n':len(x),'v16':a,'v17':b,'percent_change':100*(b/a-1),
                        'v17_better_notes':sum(r['cases']['v17']['score']<r['cases']['v16']['score'] for r in x)}
    result={'summary':summary,'notes':rows,'reference':SRC,
      'note':'External reference note audio is from a different guitar/capture chain and lossy previews. Does not demonstrate two-source phase identification.',
      'new_observer_default':False}
    (args.out/'FEATURE_SCORE17.json').write_text(json.dumps(result,indent=2),encoding='utf8')
    (args.out/'SUMMARY_JA.md').write_text('# 1.7 未使用音符特徴量比較（暫定）\n\n'
       +'低い方が特定の特徴量に近いだけで聴感上の優位性ではない。直接音／ボディ音の独立収録は未実施。\n\n'
       +'| Group | N | v1.6 | v1.7 | 変化 | 改善音数 |\n|---|---:|---:|---:|---:|---:|\n'
       +''.join(f"| {k} | {v['n']} | {v['v16']:.3f} | {v['v17']:.3f} | {v['percent_change']:+.1f}% | {v['v17_better_notes']} |\n" for k,v in summary.items()),encoding='utf8')
    print('SUMMARY',json.dumps(summary),flush=True)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--render',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    run(p.parse_args())
