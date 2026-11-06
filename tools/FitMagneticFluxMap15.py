#!/usr/bin/env python3
"""Hall-probe volume field measurement -> interpolation/gradient diagnostics.

Input CSV required fields x_mm,y_mm,z_mm,B_tesla. This script NEVER invents
measurement points: no bundled measured spatial field map exists in 1.5.
Fit is a surrogate on the actually sampled field geometry; full Maxwell
consistency and exact pickup flux linkage also require coil winding geometry.
"""
import argparse, csv, json
from pathlib import Path
import numpy as np
from scipy.interpolate import RBFInterpolator

def analyze(input_csv:Path,out:Path):
    rows=list(csv.DictReader(input_csv.open(encoding='utf-8-sig')))
    cols=['x_mm','y_mm','z_mm','B_tesla']
    if not rows or any(k not in rows[0] for k in cols):raise ValueError('Measured CSV columns required: '+','.join(cols))
    x=np.asarray([[float(row[k]) for k in cols[:3]] for row in rows],dtype=float)
    y=np.asarray([float(row['B_tesla']) for row in rows],dtype=float)
    if not np.isfinite(x).all() or not np.isfinite(y).all():raise ValueError('NaN in Hall probe data')
    if len(rows)<32 or min(np.ptp(x,axis=0))<1.e-3:raise ValueError('Need >=32 points and 3D scanned volume')
    train=np.arange(len(rows))%5!=0
    model=RBFInterpolator(x[train],y[train],kernel='thin_plate_spline',smoothing=1e-8)
    pred=model(x[~train]); truth=y[~train]
    rmse=float(np.sqrt(np.mean((pred-truth)**2)))
    center=np.mean(x[train],axis=0)
    dz=.15 # mm; only estimate inside sampled volume
    probes=np.repeat(center[None,:],2,axis=0);probes[0,2]-=dz;probes[1,2]+=dz
    grad=float((model(probes)[1]-model(probes)[0])/(2*dz*.001))
    report=dict(source_file=str(input_csv),number_points=len(rows),train_points=int(train.sum()),
       heldout_points=int((~train).sum()),heldout_field_rmse_tesla=rmse,
       center_field_tesla=float(model(center[None,:])[0]),
       local_vertical_gradient_tesla_per_m=grad,
       sampled_spatial_extent_mm=np.ptp(x,axis=0).tolist(),
       caveat='single B component Hall probe, not complete vector flux linkage; cannot infer magnetization outside scanned volume')
    out.parent.mkdir(parents=True,exist_ok=True)
    out.write_text(json.dumps(report,indent=2),encoding='utf-8');return report
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('measured_csv',type=Path);ap.add_argument('--out',type=Path,default=Path('research/flux_fit.json'))
    args=ap.parse_args();print(json.dumps(analyze(args.measured_csv,args.out),indent=2))
