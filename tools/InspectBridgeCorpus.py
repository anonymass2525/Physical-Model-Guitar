#!/usr/bin/env python3
"""Probe original Zenodo force/velocity measurements; report shape only, no raw redistribution.
The dataset copyright/license must be reviewed before any numerical model is released.
"""
import json, hashlib, urllib.request, os
from pathlib import Path
from scipy.io import whosmat
from urllib.parse import quote

BASE='https://zenodo.org/records/4604577/files/'
FILES=['batch_extract_bridge_mobility_archive.m', 'create_fig_archive.m', 'qualified_selected_impulses.mat']
root=Path(os.environ.get('DATA_DIR','bridge-probe'));root.mkdir(exist_ok=True)
rows=[]
for file in FILES:
    dest=root/file
    print('FETCH',file,flush=True)
    try:
        req=urllib.request.Request(BASE+quote(file)+'?download=1',headers={'User-Agent':'PMG08-CorpusInspection/1.0 (+research)'} )
        with urllib.request.urlopen(req,timeout=100) as response, dest.open('wb') as out:
            while True:
                block=response.read(1<<20)
                if not block: break
                out.write(block)
        row={'name':file,'size':dest.stat().st_size,'md5':hashlib.md5(dest.read_bytes()).hexdigest()}
        if file.endswith('.mat'):
            try: row['variables']=[{'name':n,'shape':s,'class':t} for n,s,t in whosmat(dest)]
            except Exception as e: row['whosmat_error']=str(e)
        else:
            row['sample_text']=dest.read_text(errors='replace')[:12000]
        rows.append(row)
        print(json.dumps(row,ensure_ascii=False),flush=True)
    except Exception as e:
        print('FETCH_ERROR',file,type(e).__name__,str(e),flush=True)
        rows.append({'name':file,'error':str(e)})
(root/'PROBE_REPORT.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2))
if not any('variables' in r for r in rows):raise RuntimeError('Measurement matrix unavailable')
