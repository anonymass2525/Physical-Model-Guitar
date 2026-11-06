#!/usr/bin/env python3
"""SYNTHETIC UNIT TEST ONLY — no real guitar measurements present."""
import json
import sys
import tempfile
import hashlib
from types import SimpleNamespace
from pathlib import Path
import numpy as np
import soundfile as sf
from scipy.signal import lfilter

sys.path.insert(0,str(Path(__file__).resolve().parents[1] / 'tools'))
from MeasurementStation18 import default_manifest, record, ORDER
from QualifyJointMeasurement18 import qualify, inspect


def generate(root:Path,fs=24000):
    rng=np.random.default_rng(1818)
    m=default_manifest('SYNTHETIC-TEST-NOT-A-PHYSICAL-GUITAR','TEST-RIG-DO-NOT-REUSE',fs,'SYNTHETIC')
    m['is_physical_capture']=True # test fixture ONLY; proves parser cannot certify real hardware
    for idx,ch in enumerate(m['sensor_channels']):
        ch['serial']=f'SYNTHETIC-SENSOR-{idx}'
        ch['units_per_full_scale']=[.025,.012,3.0,3.0][idx]
        ch['calibration_method']='SYNTHETIC fixture constant'
        ch['hardware_delay_samples']=0.0
    for i in range(8):
        n=int(fs*1.5)
        sx=lfilter([1],[1,-.67],rng.normal(0,.035,n))
        bx=lfilter([1],[1,-.60],rng.normal(0,.029,n))
        def shift(x,k):return np.r_[np.zeros(k),x[:-k]]
        left=.73*shift(sx,7)+.65*lfilter([1],[1,-.90],shift(bx,13))
        right=.51*shift(sx,16)+.39*lfilter([1],[1,-.86],shift(bx,22))
        out=np.column_stack([sx,bx,left,right]).astype('float32')
        filename=f'synthetic_{i:02d}.wav'
        sf.write(root/filename,out,fs,subtype='FLOAT')
        m['takes'].append(dict(file=filename,sha256=hashlib.sha256((root/filename).read_bytes()).hexdigest(),
            guitar_id=m['guitar_id'],mic_geometry_id=m['mic_geometry_id'],capture_interface=m['capture_interface'],
            simultaneous_capture=True,string=1+i%3,fret=i%5,partition='train' if i<6 else 'holdout',
            excitation=['pluck','angled_pluck','bridge_drive'][i%3]))
    p=root/'manifest.json';p.write_text(json.dumps(m,indent=2))
    return p,m


def run():
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp);p,m=generate(root)
        report=qualify(p,nperseg=2048)
        print('SYNTHETIC ONLY: physically unverified capture fixture, not a real guitar')
        print(json.dumps({'pass':report['all_holdouts_passed'],'holdout':report['heldout'],
            'paths':len(report['calibrated_complex_transfer'])},indent=2))
        assert report['all_holdouts_passed']
        assert len(report['calibrated_complex_transfer'])==4
        assert report['candidate_replaces_provisional_coefficients'] is False
        assert report['source_separation_proven'] is False
        # Correlated string/bridge sensor intentionally defeats identifiability.
        for t in m['takes']:
            target=root/t['file'];wav,rate=sf.read(target,always_2d=True,dtype='float32');wav[:,1]=wav[:,0]
            sf.write(target,wav,rate,subtype='FLOAT')
            t['sha256']=hashlib.sha256(target.read_bytes()).hexdigest()
        p.write_text(json.dumps(m))
        try: qualify(p,nperseg=2048)
        except ValueError as ex: assert 'UNIDENTIFIABLE' in str(ex),str(ex)
        else: raise AssertionError('accepted collinear references')
        # Reject missing calibration even if claimed to be a physical capture.
        m['sensor_channels'][0]['units_per_full_scale']=None
        p.write_text(json.dumps(m))
        try: inspect(m,root)
        except ValueError as ex: assert 'units_per_full_scale' in str(ex),str(ex)
        else: raise AssertionError('accepted missing sensor calibration')
        # Reject relabelled different guitar.
        m['sensor_channels'][0]['units_per_full_scale']=.025
        m['takes'][0]['guitar_id']='DIFFERENT-GUITAR'
        p.write_text(json.dumps(m))
        try: inspect(m,root)
        except ValueError as ex: assert 'changed guitar' in str(ex),str(ex)
        else: raise AssertionError('accepted different guitar')
        # Mock ADC fixture: mapping [3,4,1,2] must become logical
        # [string, bridge, micL, micR] without altering the raw meaning.
        mapping_root=root/'fake_adc'
        mapping_root.mkdir()
        mapping_manifest=default_manifest('SYNTHETIC-MAP','FAKE-GEOM',24000,'FAKE-ADC',(3,4,1,2))
        mapping_path=mapping_root/'manifest.json'
        mapping_path.write_text(json.dumps(mapping_manifest))
        import types
        rng=np.random.default_rng(551)
        adc=rng.normal(0,.001,(24000,4)).astype('float32')
        adc[:,0]*=1;adc[:,1]*=2;adc[:,2]*=3;adc[:,3]*=4
        sys.modules['sounddevice']=SimpleNamespace(query_devices=lambda _:{'max_input_channels':4,'name':'Mock 4-ch device'},
            rec=lambda *_a,**_kw:adc,wait=lambda:None)
        record(mapping_path,None,1.,1,0,'pluck','medium','mapping')
        mdata=json.loads(mapping_path.read_text())
        assert not mdata['is_physical_capture'], 'capture must never mark physical calibration as verified'
        wav,_=sf.read(mapping_root/mdata['takes'][0]['file'],always_2d=True,dtype='float32')
        assert np.array_equal(wav,adc[:,[2,3,0,1]])
        print('PMG18 synthetic ADC mapping and provenance test: PASS')
        print('PMG18 synthetic transfer fit and strict rejection tests: PASS')

if __name__=='__main__':run()
