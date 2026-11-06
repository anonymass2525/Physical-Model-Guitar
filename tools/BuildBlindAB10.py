#!/usr/bin/env python3
"""Offline double-blind level-matched render using independent new/legacy switches."""
import argparse,csv,random,subprocess,tempfile,zipfile,io
from pathlib import Path
import numpy as np
from scipy.io import wavfile

def build(render,out):
    out.parent.mkdir(parents=True,exist_ok=True)
    configurations=[('electric',1),('steel',2),('nylon',3)]
    notes=[(0,43),(2,53),(4,62),(5,67)]
    rng=random.Random(8102026)
    answers=[]
    with tempfile.TemporaryDirectory() as temporary,zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=7) as z:
        for name,m in configurations:
            for s,midi in notes:
                pair=[]
                for enabled in (0,1):
                    fp=Path(temporary)/f'model{m}_str{s}_{midi}_phys{enabled}.wav'
                    subprocess.run([str(render),'--single',str(m),str(midi),'.76',str(fp),
                        f'--string={s}','--physics06=1','--physics07=1','--physics08=1','--physics09=1',
                        f'--physics10={enabled}'],check=True,stdout=subprocess.DEVNULL)
                    sr,wav=wavfile.read(fp); pair.append(wav.astype('float64')/32768.)
                # Match *each* RMS to a common -26dBFS goal; suppress accidental loudness cue.
                target=10**(-26/20.)
                processed=[]
                for audio in pair:
                    rms=np.sqrt(np.mean(audio**2))
                    scale=min(target/max(rms,1e-9),.95/max(np.max(np.abs(audio)),1e-6))
                    processed.append(np.clip(np.round(audio*scale*32767),-32768,32767).astype('<i2'))
                order=[0,1];rng.shuffle(order)
                key=f'{name}_string{s+1}_midi{midi}'
                for letter,selector in zip(('X','Y'),order):
                    tmp=Path(temporary)/f'{key}_{letter}.wav';wavfile.write(tmp,sr,processed[selector])
                    z.write(tmp,f'listen/{key}_{letter}.wav')
                answers.append({'case':key,'X':'v1.0' if order[0] else 'v0.9','Y':'v1.0' if order[1] else 'v0.9'})
        buffer=io.StringIO();writer=csv.DictWriter(buffer,fieldnames=('case','X','Y'))
        writer.writeheader();writer.writerows(answers)
        z.writestr('ANSWER_KEY_OPEN_LAST.csv',buffer.getvalue())
        z.writestr('README_JA.txt','各X/Yを音量だけ統一した同一MIDI条件で比較してください。\n実ギターとの比較ではありません。音の自然さは個別に判断してください。\n比較が終わるまでANSWER_KEY_OPEN_LAST.csvは開かないでください。\n')
    print(f'Created {len(answers)} pairs at {out}')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--render',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();build(a.render,a.out)
