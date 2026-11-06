#!/usr/bin/env python3
"""Deterministic level-matched blind A/B for model-specific observers.
The output is original synthetic sound only; no copyrighted third-party takes.
"""
import argparse,json,subprocess,tempfile,wave,hashlib,random
from pathlib import Path
import numpy as np
import soundfile as sf

def main(args):
    args.out.mkdir(parents=True,exist_ok=True)
    rng=random.Random(1309)
    key=[]
    notecases={"electric":(1,[(45,1),(55,3),(64,5),(69,5)]),
               "nylon":(3,[(45,1),(55,3),(64,5),(69,5)]),
               "steel-control":(2,[(45,1),(64,5)])}
    with tempfile.TemporaryDirectory() as temp:
        temp=Path(temp)
        for kind,(model,notes) in notecases.items():
            for midi,string in notes:
                renders=[]
                for new in (False,True):
                    path=temp/f'{kind}_{midi}_{new}.wav'
                    subprocess.run([str(args.render),'--single',str(model),str(midi),'.65',str(path),
                        '--physics12=1',f'--string={string}',
                        f'--magnetic13={int(new)}',f'--radiation13={int(new)}'],
                        check=True,stdout=subprocess.DEVNULL)
                    data,sr=sf.read(path,dtype='float64',always_2d=True)
                    renders.append(data)
                rms=[np.sqrt(np.mean(x*x)) for x in renders]
                target=min(rms)
                audio=[x*target/max(r,1e-9) for x,r in zip(renders,rms)]
                order=[0,1];rng.shuffle(order)
                key.append({'name':f'{kind}_{midi}', 'X':'1.3' if order[0] else '1.2',
                    'Y':'1.3' if order[1] else '1.2', 'physical_string':string,
                    'RMS_pre_match':[float(a) for a in rms]})
                for label,ix in zip('XY',order):
                    sf.write(args.out/f'{kind}_{midi}_{label}.wav',audio[ix],sr,subtype='PCM_16')
    (args.out/'ANSWER_KEY_OPEN_LAST.json').write_text(json.dumps(key,indent=2),encoding='utf-8')
    (args.out/'README_JA.txt').write_text('X/YはRMS音量一致の48kHz 16-bit WAVです。まず比較し、ANSWER_KEY_OPEN_LAST.jsonは最後に開いてください。steel-controlは音が一致する負の対照です。\n',encoding='utf-8')
    print('Created',len(key),'blind A/B pairs, 2 notes Steel negative controls')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--render',type=Path,required=True);p.add_argument('--out',type=Path,required=True);main(p.parse_args())
