#!/usr/bin/env python3
"""Estimate ten body-mode candidate corrections from an instrumented guitar bridge.

This is explicitly NOT an estimator from ordinary acoustic-guitar WAV. It needs
simultaneous excitation force [newtons] and bridge velocity [m/s] channels
from a measured shaker/impact-hammer+accelerometer setup, in calibrated units.
Candidate mode match is exploratory and needs verified phase and cross-direction
response before physical adoption. No MIDI note/sampled audio can substitute for
measured admittance.
"""
import argparse, json
from pathlib import Path
import numpy as np
import soundfile as sf
from scipy import signal, optimize

BASE_FREQ=np.array([82,111,182,210,284,397,535,742,1020,1580.],dtype=float)

def analyze(path,model='steel'):
    sig,sr=sf.read(path,dtype='float64',always_2d=True)
    if sig.shape[1]!=2:raise ValueError('Need precisely two synchronous channels: force[N], velocity[m/s]')
    if sr<8000 or len(sig)<sr*2:raise ValueError('Provide at least 2 seconds, >=8 kHz')
    force=sig[:,0]; velocity=sig[:,1]
    if not np.isfinite(sig).all():raise ValueError('nonfinite input')
    if np.max(abs(force))<1e-9 or np.max(abs(velocity))<1e-11:raise ValueError('silent input')
    win=min(32768,max(4096,int(sr*0.55)))
    f,Pxx=signal.welch(force,fs=sr,nperseg=win,noverlap=win//2,detrend='linear')
    _,Pxy=signal.csd(force,velocity,fs=sr,nperseg=win,noverlap=win//2,detrend='linear')
    _,coh=signal.coherence(force,velocity,fs=sr,nperseg=win,noverlap=win//2)
    H=Pxy/(Pxx+1e-19)
    mag=np.abs(H)
    good=(f>=50)&(f<=min(2100,sr*.38))&(coh>=.5)&(Pxx>np.max(Pxx)*1.e-5)
    # Relative gain ranking; scale from physical m/N/s if sensors are calibrated.
    # No constraint that ten observable modes will be present in all recordings.
    s=np.where(good,mag,0.)
    s=signal.savgol_filter(s,7,2) if len(s)>=7 else s
    peaks,props=signal.find_peaks(s,prominence=max(np.max(s)*.017,1e-10),distance=max(3,int(12/(sr/win))))
    peaks=sorted(peaks,key=lambda i:s[i],reverse=True)[:35]
    peaks=sorted(peaks,key=lambda i:f[i])
    # Match to existing ten modes by nearest frequency rather than claiming every
    # peak represents a unique eigenmode; unmatched/disallowed peaks are reported.
    model_multiplier=1.35 if model=='electric' else (.93 if model=='nylon' else 1.)
    selected={}; used=set();unmatched=[]
    # Globally assign measured peaks to the ten model modes, allowing unmatched
    # rows, rather than greedily stealing a high-frequency peak for a low mode.
    bases=BASE_FREQ*model_multiplier
    cost=np.full((len(bases),len(peaks)+len(bases)),.23,dtype=float)
    for k,base in enumerate(bases):
        for j,i in enumerate(peaks):
            ratio=float(f[i]/base)
            cost[k,j]=abs(np.log(ratio)) if .70<=ratio<=1.40 else 100.
    rows,cols=optimize.linear_sum_assignment(cost)
    for idx,j in zip(rows,cols):
        if j>=len(peaks) or cost[idx,j]>=.23:continue
        i=peaks[j]; used.add(i);base=bases[idx]
        # Half-power bandwidth estimate around the resonance, not a complete
        # structural parameter fit. Must be corroborated by phase in future.
        level=s[i]/np.sqrt(2)
        left=i
        while left>0 and s[left]>level:left-=1
        right=i
        while right<len(f)-1 and s[right]>level:right+=1
        bw=max((right-left)*sr/win,sr/win)
        Q=max(.6,min(80.,float(f[i]/bw)))
        qbase=(2.5 if model=='electric' else 5.7 if model=='nylon' else 8.1)+idx*.48
        selected[int(idx)]=dict(frequency_hz=round(float(f[i]),2),frequency_scale=round(float(f[i]/base),5),
                           apparent_Q=round(Q,3),q_scale=round(float(np.clip(Q/qbase,.4,2.5)),5),
                           coherence=round(float(coh[i]),3),relative_gain=round(float(s[i]/max(s[peaks])),4))
    unmatched=[round(float(f[i]),2) for i in peaks if i not in used]
    # Use only relative gains, normalisation depends on microphone/calibration.
    for idx,rec in selected.items():
        rec['gain_scale']=round(float(np.clip(np.sqrt(rec['relative_gain']),.25,2.)),5)
    return dict(source=str(path),rate=int(sr),duration_s=round(len(sig)/sr,2),
                model=model,mode_candidates={str(k):v for k,v in selected.items()},
                unmatched_peaks_hz=unmatched,
                warning='Candidate modal fit ONLY. Verify H(f) phase, excitation position and units; no automatic adoption.',
                example_cli_flags=[f'--mode{i}-freq={rec["frequency_scale"]} --mode{i}-q={rec["q_scale"]} --mode{i}-gain={rec["gain_scale"]}' for i,rec in selected.items()])

def main():
    p=argparse.ArgumentParser();p.add_argument('measurement',type=Path)
    p.add_argument('--model',choices=['electric','steel','nylon'],default='steel')
    p.add_argument('--out',type=Path,default=Path('bridge-mode-fit.json'))
    a=p.parse_args();result=analyze(a.measurement,a.model)
    a.out.write_text(json.dumps(result,indent=2,ensure_ascii=False),encoding='utf-8')
    print(f'Candidates matched: {len(result["mode_candidates"])}/10; write {a.out}')
    for i,rec in result['mode_candidates'].items():print(f'Mode{i}: f={rec["frequency_hz"]:.1f}Hz  Q~{rec["apparent_Q"]:.1f} coherence={rec["coherence"]:.2f}')

if __name__=='__main__':main()
