#!/usr/bin/env python3
"""Estimate relative channel delays from a physically shared calibration pulse.

A common electrical test pulse injected into ADC paths measures ADC/line delay;
it does NOT by itself measure internal sensing latency of a vibrometer or
accelerometer conditioning circuit. That latency must be characterized with
an appropriate physical calibration experiment.
"""
import argparse
import json
import numpy as np
import soundfile as sf
from scipy.signal import correlate, correlation_lags


def measure(wave, fs, threshold=.93):
    if wave.ndim != 2 or wave.shape[1] != 4:
        raise ValueError('four-channel loopback WAV needed')
    if fs<22050 or fs>192000:
        raise ValueError('invalid fs')
    ref=wave[:,0]-np.mean(wave[:,0]);ref/=np.linalg.norm(ref)+1.e-30
    results=[{'channel':1,'delay_vs_ch1_samples':0.,'normalized_correlation':1.}]
    for c in range(1,4):
        y=wave[:,c]-np.mean(wave[:,c]);y/=np.linalg.norm(y)+1.e-30
        corr=correlate(y,ref,mode='full',method='fft')
        lags=correlation_lags(len(y),len(ref),mode='full')
        center=len(ref)-1
        window=np.abs(lags)<=min(512,len(ref)//3)
        k=np.flatnonzero(window)[np.argmax(np.abs(corr[window]))]
        peak=np.abs(corr[k])
        if peak<threshold:
            raise ValueError(f'channel {c+1}: no verified common pulse correlation ({peak:.3f})')
        if k<=0 or k>=len(corr)-1:
            raise ValueError('bad correlation peak')
        a,b,d=np.abs(corr[k-1]),np.abs(corr[k]),np.abs(corr[k+1])
        denominator=(a-2*b+d)
        frac=.5*(a-d)/denominator if abs(denominator)>1e-14 else 0.
        lag=float(lags[k]+np.clip(frac,-.5,.5))
        results.append({'channel':c+1,'delay_vs_ch1_samples':round(lag,5),
                        'normalized_correlation':round(float(peak),6)})
    return {'sample_rate':fs,'adc_path_delays_only':True,'internal_sensor_delay_calibrated':False,
            'channel_results':results,
            'warning':'Do not use this ADC-only pulse test as evidence of LDV/mic/accelerometer internal sensor latency.'}


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--wav',required=True);p.add_argument('--out',required=True)
    a=p.parse_args()
    y,fs=sf.read(a.wav,always_2d=True)
    r=measure(y,fs)
    with open(a.out,'w',encoding='utf8') as f:json.dump(r,f,indent=2)
    print(json.dumps(r,indent=2))

if __name__=='__main__': main()
