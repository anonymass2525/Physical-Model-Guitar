#!/usr/bin/env python3
"""Original synthetic only, level-matched and randomized. Answers kept separate."""
import argparse, json, subprocess, tempfile, random
from pathlib import Path
import numpy as np
import soundfile as sf

def main(args):
    (args.out/'listen').mkdir(parents=True,exist_ok=True)
    rng=random.Random(1414);answers=[]
    cases={
      'electric':(1,[(45,1),(55,3),(64,5),(69,5)]),
      'nylon-upper':(3,[(64,5),(67,5),(69,5),(72,5),(74,5),(76,5)]),
      'steel-negative-control':(2,[(45,1),(64,5)]),
    }
    with tempfile.TemporaryDirectory() as temp:
        temp=Path(temp)
        for kind,(model,notes) in cases.items():
            for midi,physicalString in notes:
                audio=[]
                for use14 in (False,True):
                    wav=temp/'render.wav'
                    subprocess.run([str(args.render),'--single',str(model),str(midi),'.65',str(wav),
                      '--physics12=1',f'--string={physicalString}',
                      '--magnetic13=1','--radiation13=1',f'--magnetic14={int(use14)}',
                      f'--radiation14={int(use14)}'],check=True,stdout=subprocess.DEVNULL)
                    data,sr=sf.read(wav,always_2d=True)
                    audio.append(data)
                rms=[float(np.sqrt(np.mean(x*x))) for x in audio]
                target=min(rms)
                order=[0,1];rng.shuffle(order)
                for axis,i in zip('XY',order):
                    normalized=audio[i]*target/max(rms[i],1e-10)
                    sf.write(args.out/'listen'/f'{kind}_{midi}_{axis}.wav',normalized,48000,subtype='PCM_16')
                answers.append(dict(pair=f'{kind}_{midi}',X='1.4' if order[0] else '1.3',
                                    Y='1.4' if order[1] else '1.3',source_rms=rms))
    (args.out/'ANSWER_KEY_OPEN_LAST.json').write_text(json.dumps(answers,indent=2),encoding='utf-8')
    (args.out/'README_JA.txt').write_text('各X/YはRMS音量を揃えた合成WAVです。listen/を先に聴き、ANSWER_KEYは最後に確認してください。Steelは変更がないことの対照です。\n',encoding='utf-8')
    print(len(answers),'matched blind A/B pairs created')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--render',type=Path,required=True);p.add_argument('--out',type=Path,required=True);main(p.parse_args())
