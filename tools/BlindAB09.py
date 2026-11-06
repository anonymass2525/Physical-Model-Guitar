#!/usr/bin/env python3
"""RMS matched (first 1.2 seconds), blinded 0.8 vs 0.9 listening kit."""
import argparse,json,random,subprocess
from pathlib import Path
import numpy as np
from scipy.io import wavfile

def run(args):
    args.output.mkdir(parents=True,exist_ok=True)
    listen=args.output/'listen';listen.mkdir(exist_ok=True)
    rng=random.Random(20261106);labels=[]
    tuning=[40,45,50,55,59,64]
    for model,name in [(1,'Electric'),(2,'Steel'),(3,'Nylon')]:
        for string,fret in [(0,0),(1,3),(3,5),(5,7)]:
            tag=f'{name}_String{string+1}_Fret{fret}'
            audio=[]
            for option in (0,1):
                tmp=args.output/f'.temporary_{name}_{string}_{fret}_{option}.wav'
                subprocess.run([str(args.render),'--single',str(model),str(tuning[string]+fret),'.73',str(tmp),
                    f'--string={string}','--physics06=1','--physics07=1','--physics08=1',
                    f'--physics09={option}','--bridge-measured08=0'],check=True,stdout=subprocess.DEVNULL)
                rate,samples=wavfile.read(tmp);audio.append(samples.astype(np.float64)/32768.)
                tmp.unlink()
            rms=[float(np.sqrt(np.mean(x[:int(rate*1.2)]**2))) for x in audio]
            reference=max(1e-9,min(rms))
            norm=[audio[i]*min(8.,reference/max(1e-10,rms[i])) for i in range(2)]
            peak=max(float(np.max(np.abs(x))) for x in norm)
            common=min(1.,.93/max(1e-9,peak))
            norm=[np.int16(np.round(np.clip(x*common,-1.,1.)*32767.)) for x in norm]
            flip=rng.randrange(2)
            left,right=(norm[1],norm[0]) if flip else (norm[0],norm[1])
            wavfile.write(listen/f'{tag}_X.wav',rate,left)
            wavfile.write(listen/f'{tag}_Y.wav',rate,right)
            labels.append({'pair':tag,'X':'0.9' if flip else '0.8','Y':'0.8' if flip else '0.9',
                           'rms_08':rms[0],'rms_09':rms[1], 'target_rms':reference})
    (args.output/'ANSWER_KEY_OPEN_LAST.json').write_text(json.dumps(labels,indent=2),encoding='utf-8')
    (args.output/'README_JA.txt').write_text('Physical Model Guitar 0.8対0.9のブラインド試聴です。X/Yは同じ楽器・弦・フレット・強さで、1.2秒RMSを一致させています。先入観を避けるため解答は最後に開いてください。差が小さい組は差なしと記録してください。実録音の参照サンプルは含みません。',encoding='utf-8')
    print('created',len(labels),'matched A/B pairs')

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--render',type=Path,required=True);ap.add_argument('--output',type=Path,required=True)
    run(ap.parse_args())
