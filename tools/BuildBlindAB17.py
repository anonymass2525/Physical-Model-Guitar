#!/usr/bin/env python3
"""Five RMS-matched blind pairs; generated notes are original synthesis, no third-party audio."""
import json,hashlib,subprocess,tempfile,zipfile
from pathlib import Path
import numpy as np
import soundfile as sf

def run(render,out):
    configs=[('nylon_G3',3,55,3),('nylon_E4',3,64,5),('nylon_A4',3,69,5),
             ('steel_G3_control',2,55,3),('electric_G3_control',1,55,3)]
    rng=np.random.default_rng(1707)
    with tempfile.TemporaryDirectory() as temp:
        temp=Path(temp)
        key={};stats=[]
        for label,model,note,string in configs:
            versions={};ratios={}
            for name,on in [('old',0),('new',1)]:
                p=temp/f'{label}_{name}.wav'
                subprocess.run([str(render),'--single',str(model),str(note),'.65',str(p),
                    f'--string={string}','--physics12=1','--radiation15=1','--radiation16=1',
                    '--late-body16=.5',f'--radiation17={on}','--direct-phase17=.86',
                    '--direct-mix17=.75'],check=True,stdout=subprocess.DEVNULL)
                y,sr=sf.read(p,dtype='float64',always_2d=True)
                y-=y.mean(axis=0,keepdims=True)
                rms=np.sqrt(np.mean(y*y))
                if rms<1.e-7:raise RuntimeError(f'Silence in {label} {name}')
                # Cap peak first; then match both variants to an identical
                # RMS that both can reach without limiting or clipping.
                ratios[name]=float(np.max(np.abs(y))/rms)
                versions[name]=y/rms
            target=min(10**(-21/20),.85/max(ratios.values()))
            versions={name:y*target for name,y in versions.items()}
            order=['old','new'];rng.shuffle(order)
            key[label]={'X':order[0],'Y':order[1]}
            for pos,name in zip(('X','Y'),order):
                sf.write(temp/f'{label}_{pos}.wav',versions[name],48000,subtype='PCM_16')
            diff=float(np.mean(np.abs(versions['old']-versions['new'])))
            stats.append({'pair':label,'mean_absolute_difference_before_quantization':diff,
                          'expected_unchanged':model!=3,
                          'matched_rms_dbfs':float(20*np.log10(target))})
        with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=7) as z:
            for f in temp.glob('*_[XY].wav'):z.write(f,'listen/'+f.name)
            z.writestr('ANSWER_KEY_OPEN_LAST.json',json.dumps(key,ensure_ascii=False,indent=2))
            z.writestr('METRICS.json',json.dumps(stats,indent=2))
            z.writestr('README_JA.txt','1.6 vs 1.7 同一条件・各ペア内RMS音量一致（クリッピング防止のためペア間音量は異なる）。\n'
                       'まずlisten内XとYを比較して、最後にANSWER_KEYを開いてください。\n'
                       'Steel/Electricは変更されない対照音です。\n'
                       '実楽器の別録音ではなく、DSP生成音声の比較です。\n')
        print('AB summary',stats)
if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser();p.add_argument('--render',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();run(a.render,a.out)
