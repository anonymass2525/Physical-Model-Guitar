#!/usr/bin/env python3
"""Create 0.5/0.6 volume-matched blind comparison WAV files, with a separate answer key."""
import argparse,json,math,random,subprocess
from pathlib import Path
import soundfile as sf
import numpy as np
TUNING=(40,45,50,55,59,64)
def render(exe,model,note,string,new,out):
    subprocess.run([str(exe),"--single",str(model),str(note),".78",str(out),
                    f"--string={string}",f"--physics06={int(new)}"],check=True,stdout=subprocess.DEVNULL)
    return sf.read(out,always_2d=True,dtype="float64")
def match(a,b,sr):
    lo=int(.07*sr);hi=min(len(a),int(1.1*sr))
    rms=lambda x: math.sqrt(float(np.mean(x[lo:hi]**2)))
    ra,rb=rms(a),rms(b)
    if min(ra,rb)<1e-7:raise ValueError("silent recording")
    target=math.sqrt(ra*rb)
    aa,bb=a*(target/ra),b*(target/rb)
    guard=min(1.,.90/max(np.max(np.abs(aa)),np.max(np.abs(bb)),1e-6))
    return aa*guard,bb*guard
def main(args):
    rng=random.Random(args.seed);out=args.out;clips=out/"listen";clips.mkdir(exist_ok=True,parents=True)
    key=[]
    for name,model in (("Electric",1),("Steel",2),("Nylon",3)):
        for string,fret in ((0,3),(2,5),(4,3),(5,5)):
            note=TUNING[string]+fret
            a,sr=render(args.render,model,note,string,False,out/"old.wav")
            b,sr2=render(args.render,model,note,string,True,out/"new.wav")
            assert sr==sr2
            a,b=match(a,b,sr)
            swap=rng.choice((False,True))
            x,y=(b,a) if swap else (a,b)
            tag=f"{name}_S{string+1}_F{fret}"
            sf.write(clips/(tag+"_X.wav"),x,sr,subtype="PCM_24")
            sf.write(clips/(tag+"_Y.wav"),y,sr,subtype="PCM_24")
            key.append(dict(pair=tag,version06="X" if swap else "Y",
                            version05="Y" if swap else "X",midi=note))
    (out/"old.wav").unlink(missing_ok=True);(out/"new.wav").unlink(missing_ok=True)
    (out/"ANSWER_KEY_DO_NOT_OPEN.json").write_text(json.dumps(key,indent=2))
    (out/"GUIDE_JA.md").write_text("# Blind A/B\n対応するX/Yペアを聴き、先に好みとリアリティを記録してください。\n正解表は聴き終えるまで開かないでください。\n音量は0.07〜1.1秒のRMSで一致させています。完全なLUFS一致や人間の主観一致は保証しません。\n")
if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("--render",type=Path,required=True)
    p.add_argument("--out",type=Path,required=True);p.add_argument("--seed",type=int,default=2606)
    main(p.parse_args())
