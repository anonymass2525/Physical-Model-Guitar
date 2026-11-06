#!/usr/bin/env python3
"""Synthetic four-channel known-transfer test and unidentifiable-input gate."""
from pathlib import Path
import json
import tempfile
import numpy as np
import soundfile as sf
from scipy.signal import lfilter
from JointRadiation17 import fit

def run():
    rng=np.random.default_rng(1707)
    fs=48000
    with tempfile.TemporaryDirectory() as tmp:
        root=Path(tmp)
        takes=[]
        for idx in range(5):
            n=fs*2
            # Independent source references, continuously exciting all bins.
            s=lfilter([1],[1,-.72],rng.normal(size=n))*.018
            b=lfilter([1],[1,-.60],rng.normal(size=n))*.017
            left=.50*np.roll(s,9)+.38*lfilter([1],[1,-.95],np.roll(b,19))
            right=.30*np.roll(s,23)-.25*lfilter([1],[1,-.92],np.roll(b,27))
            y=np.stack([s,b,left,right],axis=1)
            sf.write(root/f'{idx}.wav',y,fs,subtype='FLOAT')
            takes.append({'file':f'{idx}.wav','guitar_id':'synthetic-A',
                          'mic_geometry_id':'fixed','simultaneous_capture':True})
        manifest={'sample_rate':fs,'channel_order':['string_velocity_reference','bridge_velocity_reference',
                'mic_left_pressure','mic_right_pressure'],'guitar_id':'synthetic-A',
                'mic_geometry_id':'fixed','takes':takes}
        result=fit(manifest,root,8192)
        print('Synthetic holdout',result['heldout_two_source_normalized_stft_error'],
              result['heldout_body_only_normalized_stft_error'])
        assert result['identified_conditional_transfer'],str(result)
        assert result['heldout_two_source_normalized_stft_error']<.28
        # Perfect correlation makes separation mathematically impossible.
        # Swapping both reference signals to one is not valid measurement.
        for i in range(5):
            data,sr=sf.read(root/f'{i}.wav',always_2d=True)
            data[:,1]=data[:,0]
            sf.write(root/f'{i}.wav',data,sr,subtype='FLOAT')
        try: fit(manifest,root,8192)
        except ValueError as e:
            assert 'UNIDENTIFIABLE' in str(e), str(e)
        else: raise AssertionError('failed to reject collinear source references')
        # Matched rig metadata must also be enforced.
        manifest['takes'][0]['mic_geometry_id']='other-position'
        try: fit(manifest,root,8192)
        except ValueError as e: assert 'same instrument' in str(e)
        else: raise AssertionError('accepted changed microphone placement')
        print('PMG1.7 synthetic identification and rejection gates PASS')
if __name__=='__main__':run()
