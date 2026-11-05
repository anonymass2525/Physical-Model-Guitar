#!/usr/bin/env python3
"""Level-matched blind A/B test of v0.6 and opt-in v0.7 on matched string/fret attacks."""
from pathlib import Path
import argparse, json, math, random, subprocess, tempfile
import numpy as np
import soundfile as sf
TUNING=(40,45,50,55,59,64)
MODEL=(('Electric',1),('Steel',2),('Nylon',3))

def render(exe,model,note,string,enabled,path):
    subprocess.run([str(exe),'--single',str(model),str(note),'.78',str(path),
                    f'--string={string}','--physics06=1',f'--physics07={int(enabled)}'],
                   check=True,stdout=subprocess.DEVNULL)
    return sf.read(path,always_2d=True,dtype='float64')

def main(args):
    rng=random.Random(2707)
    (args.out/'listen').mkdir(parents=True,exist_ok=True)
    answer=[]
    with tempfile.TemporaryDirectory() as folder:
        tmp=Path(folder)
        for model_name,model in MODEL:
            for s,fret in ((0,3),(2,5),(4,3),(5,5)):
                note=TUNING[s]+fret
                x,sr=render(args.render,model,note,s,False,tmp/'legacy.wav')
                y,sr2=render(args.render,model,note,s,True,tmp/'new.wav')
                assert sr==sr2
                l=int(.07*sr);r=min(len(x),int(1.1*sr))
                ra=math.sqrt(np.mean(x[l:r]*x[l:r]));rb=math.sqrt(np.mean(y[l:r]*y[l:r]));
                if min(ra,rb)<1.e-7:raise RuntimeError('silence')
                target=math.sqrt(ra*rb)
                x*=min(2.5,target/ra);y*=min(2.5,target/rb)
                ceiling=max(float(np.max(np.abs(x))),float(np.max(np.abs(y))))
                guard=min(1.,.93/max(1.e-9,ceiling))
                x*=guard;y*=guard
                swap=rng.choice((True,False))
                first,second=(y,x) if swap else (x,y)
                name=f'{model_name}_S{s+1}_F{fret}'
                sf.write(args.out/'listen'/f'{name}_X.wav',first,sr,subtype='PCM_24')
                sf.write(args.out/'listen'/f'{name}_Y.wav',second,sr,subtype='PCM_24')
                answer.append({'pair':name,'v07':'X' if swap else 'Y','v06':'Y' if swap else 'X',
                               'rms_match_error_db':float(20*math.log10(max(1e-15,math.sqrt(np.mean(first[l:r]**2)))/
                                                                     max(1e-15,math.sqrt(np.mean(second[l:r]**2)))))})
    (args.out/'ANSWER_KEY_READ_AFTER_LISTEN.json').write_text(json.dumps(answer,indent=2),encoding='utf-8')
    (args.out/'README_JA.md').write_text('''# v0.7 vs v0.6 音量一致ブラインド比較

- listen フォルダー内のX/Yを聴いて、リアリティ・アタック・余韻を先に判断してください。
- 差が分からない場合は「差なし」と記録してください。
- 解答は比較後にのみ ANSWER_KEY_READ_AFTER_LISTEN.json で確認してください。
- すべて単一ノートで、0.7の繰り返し演奏・残響持ち越しの変化はこの比較では評価できません。
- RMS一致は70〜1100ms窓での機械的な値であり、主観音量まで一致するとは限りません。
''',encoding='utf-8')
    print('Finished',len(answer),'blinded pairs')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--render',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    main(p.parse_args())
