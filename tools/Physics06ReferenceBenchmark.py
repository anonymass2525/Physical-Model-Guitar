#!/usr/bin/env python3
"""Compare v0.5 and v0.6 against withheld real guitar notes; NO reference audio uploaded."""
import argparse, json, subprocess, tempfile
from pathlib import Path
import numpy as np
from CalibratePhysicalModel import download, features, distance, META, SRC
from LargeCorpusCalibrator import electric_samples, TUNING, STRINGS, name_for
def gather(root):
    out=[]
    for kind in ("steel","nylon"):
        for string in (0,2,4,5):
            fret=5; midi=TUNING[string]+fret
            layer="Vel02" if kind=="steel" else "Vel02Pick"
            fn=f'INSGuitar{"Steel" if kind=="steel" else "Nylon"}_String{STRINGS[string]}_{layer}_{name_for(midi)}_01.ogg'
            p=root/kind/fn
            try:
                download(f'{META}/meta-{kind}-guitar/ogg/{fn}',p)
                out.append(dict(kind=kind,string=string,midi=midi,fret=fret,path=str(p)))
            except Exception as e:print("MISSING",kind,string,str(e)[:80],flush=True)
    try:
        for item in electric_samples(root/"input",max_samples=70):
            if item["fret"]==5 and item["string"] in (0,2,4,5):
                out.append(dict(kind="electric",string=item["string"],midi=item["midi"],
                                fret=item["fret"],path=item["path"]))
    except Exception as e:print("WARNING: electric references unavailable",str(e)[:120],flush=True)
    return out
def run(render,take,variant,out):
    mid={"electric":1,"steel":2,"nylon":3}[take["kind"]]
    cmd=[str(render),"--single",str(mid),str(take["midi"]),".65",str(out),
         f'--string={take["string"]}',f'--physics06={variant}']
    subprocess.run(cmd,check=True,stdout=subprocess.DEVNULL)
    return features(out,take["midi"])
def main(a):
    a.out.mkdir(exist_ok=True,parents=True)
    reports=[]
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp)
        for t in gather(root):
            target=features(t["path"],t["midi"])
            values=[]
            for v in (0,1):
                s,parts=distance(target,run(a.render,t,v,root/"render.wav"))
                values.append(dict(score=float(s),components=parts))
            reports.append(dict(kind=t["kind"],string=t["string"],midi=t["midi"],fret=t["fret"],
                                previous=values[0],physics06=values[1],delta=values[1]["score"]-values[0]["score"]))
            print("COMPARE",t["kind"],t["string"],t["midi"],"0.5",round(values[0]["score"],3),
                  "0.6",round(values[1]["score"],3),flush=True)
    summary={}
    for kind in ("electric","steel","nylon"):
        rows=[r for r in reports if r["kind"]==kind]
        if rows:
            summary[kind]=dict(notes=len(rows),previous=float(np.mean([r["previous"]["score"] for r in rows])),
                physics06=float(np.mean([r["physics06"]["score"] for r in rows])),
                improved=sum(r["delta"]<0 for r in rows))
    (a.out/"physics06_reference_benchmark.json").write_text(json.dumps(
        dict(summary=summary,notes=reports,reference_source=SRC,reference_audio_distributed=False,
             metric="weighted acoustic features, NOT a perceptual score",
             limitations=["lossy OGG previews, limited repeated takes and capture metadata",
                          "effects of capture chain not individually identifiable",
                          "held-out fret 5 is used for evaluation, not optimization"]),indent=2))
    print("SUMMARY",json.dumps(summary),flush=True)
if __name__=="__main__":
    p=argparse.ArgumentParser();p.add_argument("--render",type=Path,required=True);p.add_argument("--out",type=Path,required=True)
    main(p.parse_args())
