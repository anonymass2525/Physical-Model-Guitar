#!/usr/bin/env python3
"""Measurement-conditioned, explicitly conservative late pressure-radiation model.

This method uses Robert Mores' force/bridge acceleration/near-field mic signals
on *one* measured guitar. A regularized pressure/velocity inverse is evaluated
for 0.125 s (the v1.5 basis) and 0.35 s (a contaminated later-time hypothesis).
Fixed stable poles are seeded by independent *bridge mobility* modal fits.
A phase-aware LS fit solves four cross-position *real impulse* residues for a
causal late tail following the 96-tap early pressure/velocity FIR. It assesses
heldout frequency bins and gates extreme coefficients. Because there is only
one room/capture and closely spaced frequency bins are correlated, this is
NOT independent verification of physical late decay or room-free radiation.
Raw third-party waveforms are never redistributed.
"""
import argparse, hashlib, json, math, urllib.request, zipfile
from pathlib import Path
import numpy as np
from scipy.io import loadmat
from scipy.signal import find_peaks, savgol_filter
from GenerateMeasuredObserver15 import ZENODO_URL, ZENODO_MD5, FS, get_file, bridge_matrix

ROOT=Path(__file__).resolve().parents[1]
MODES=16
DELAY=96
# Structural modal prior from PMG09 joint positive-real fit of guitar #1.
# Entries are frequencies (Hz) and Q, from the 2021 Mores dataset.
PHYSICAL_MODES=[(118.,8.429),(226.,6.647),(242.,7.118),(410.,29.286),
 (472.,13.882),(560.,31.111),(936.,42.545),(1372.,45.733),
 (1440.,40.0),(1518.,44.647)]

def longer_matrix(cache,guitar=1):
    if not cache.exists():get_file(ZENODO_URL,cache)
    if hashlib.md5(cache.read_bytes()).hexdigest()!=ZENODO_MD5:
        raise RuntimeError('Checksum differs from published Mores dataset')
    array=loadmat(cache,variable_names=['qualified_selected_impulses'])['qualified_selected_impulses']
    if array.shape!=(65,144000,6):raise RuntimeError('Unexpected Mores dataset shape')
    d=array[guitar-1]
    n=48000;fft=131072
    f=np.fft.rfftfreq(fft,1/FS)
    # No full room isolation beyond ~125 ms can be claimed here. Experimental
    # longer gate measures mixed body+room nearfield, and this is disclosed.
    g=np.ones(n)
    start=int(.15*FS);end=int(.35*FS)
    g[start:end]=.5+.5*np.cos(np.linspace(0,np.pi,end-start))
    g[end:]=0.
    vv=np.zeros((len(f),2,2),complex)
    pp=np.zeros_like(vv)
    w=[]
    for col,impact in enumerate((0,2)):
        d0=d[impact*n:(impact+1)*n]
        force=np.fft.rfft(d0[:,0]*(10000/92.90)*4.4482*g,fft)
        p=np.abs(force)**2
        reg=max(float(p.max())*1e-7,1e-18)
        w.append(p)
        for k,ch in enumerate((2,1)):
            acc=np.fft.rfft(d0[:,ch]*(10000/10.64)*9.80665*g,fft)
            vv[:,k,col]=(acc*np.conj(force)/(p+reg))/(1j*np.maximum(2*np.pi*f,1.))
        for k,ch in enumerate((3,4)):
            mic=np.fft.rfft(d0[:,ch]*(10000/50)*g,fft)
            pp[:,k,col]=mic*np.conj(force)/(p+reg)
    h=np.zeros((len(f),2,2),complex)
    for i in range(1,len(f)):
        v=vv[i];p=pp[i]
        A=v@v.conj().T
        reg=.035*float(np.trace(A).real)+1e-16
        h[i]=p@v.conj().T@np.linalg.inv(A+np.eye(2)*reg)
    mask=(f>=90)&(f<=4200)&(np.minimum(w[0],w[1])>max(w[0].max(),w[1].max())*1e-8)
    # To be comparable with the original 1.5 fit, scale using the early
    # nearfield dataset median rather than arbitrarily re-normalizing the tail.
    ef,eh,_=bridge_matrix(cache,guitar)
    early_med=93.73032034931414 if guitar==1 else 1.
    # old bridge_matrix returns unit-median data; use early unnormalized
    # estimate for guitar#1 as published 1.5 normalization constant.
    return f[mask],h[mask]/early_med,{'gate':'0-150ms full;150-350ms cosine; late region includes room',
         'measurement':'Mores guitar #1, two bridge accelerometers, two nearfield microphones',
         'frequency_bins':int(mask.sum()),'early_96_model_ref':'MeasuredObserver15Coeffs.h'}

def load_fir():
    # Extract 96 real numbers x4 from the compiled generated header without
    # evaluating code. The source header was already created from Mores #1.
    s=(ROOT/'DSP/MeasuredObserver15Coeffs.h').read_text()
    start=s.index('kMeasuredNylon15{{')+len('kMeasuredNylon15{{')
    section=s[start:s.index('}};',start)]
    import re
    groups=re.findall(r'\{([^{}]+)\}',section)
    vals=[np.array([float(x) for x in group.split(',') if x.strip()]) for group in groups]
    assert len(vals)==4 and all(len(x)==96 for x in vals)
    return vals

def candidate_modes(freq,transfer):
    res=list(PHYSICAL_MODES)
    # Optional additional spectral maxima, constrained to not duplicate an
    # independently fitted structural mode.
    amp=np.mean(np.abs(transfer),axis=(1,2))
    smooth=savgol_filter(np.log(amp+1e-9), min(51,len(amp)//2*2-1),2)
    peaks,_=find_peaks(smooth,prominence=.12,distance=13)
    ranked=sorted(peaks,key=lambda k:float(smooth[k]),reverse=True)
    for k in ranked:
        f=float(freq[k]);
        if f>3600 or f<85 or any(abs(f-x)<max(38.,.075*x) for x,_ in res):continue
        # Q of novel modes is not known; conservative prior is 10-35.
        Q=25. if f<500 else 32.
        res.append((f,Q))
        if len(res)>=MODES:break
    for f in [96,320,690,1060,1800,2800,3500]:
        if len(res)>=MODES:break
        if not any(abs(f-x)<30. for x,_ in res):res.append((f,16.))
    res=res[:MODES]
    # Envelope taus from fitted structural Q, with bounded uncertainty. A
    # physical prediction, NOT a direct measurement of 0.35s room-free decay.
    return [(f,float(np.clip(Q/(np.pi*f),.008,.32))) for f,Q in res]

def basis(freq,modes):
    omega=2*np.pi*freq/FS
    z=np.exp(-1j*omega)
    delay=np.exp(-1j*omega*DELAY)
    columns=[]
    for hz,tau in modes:
        r=np.exp(-1/(FS*tau))
        p=r*np.exp(2j*np.pi*hz/FS)
        a=1/(1-p*z)
        b=1/(1-np.conj(p)*z)
        columns.extend([delay*.5*(a+b), delay*.5j*(a-b)])
    return np.stack(columns,axis=1)

def estimate(freq,h):
    fir=load_fir(); z=np.exp(-2j*np.pi*np.outer(freq,np.arange(96))/FS)
    old=np.stack([z@x for x in fir],axis=1)
    target=h.reshape(-1,4)
    modes=candidate_modes(freq,h)
    M=basis(freq,modes)
    valid=np.isfinite(target).all(axis=1)
    freq=freq[valid];M=M[valid];old=old[valid];target=target[valid]
    # 4/5 train: validation is interleaved bins from the same capture (NOT
    # independent recording or independent room). Tail-only LS regularization.
    train=np.arange(len(freq))%5!=0;hold=~train
    A=np.vstack([M[train].real,M[train].imag]);
    ridge=.018*np.sqrt(len(A)/M.shape[1]);
    A=np.vstack([A,ridge*np.eye(M.shape[1])]);
    coeffs=np.zeros((4,M.shape[1]))
    for j in range(4):
        residual=(target-old)[train,j]
        rhs=np.r_[residual.real,residual.imag,np.zeros(M.shape[1])]
        coeffs[j]=np.linalg.lstsq(A,rhs,rcond=None)[0]
    # Bound tail amplitude and remove outlying coefficients. Bounded modal
    # gain prevents any frequency bin from exploiting the ill-conditioned
    # inverse to create a large autonomous late resonance.
    coeffs=np.clip(coeffs,-.01,.01)
    correction=M@coeffs.T
    alpha=1.
    def norm(x,y):return float(np.linalg.norm(x-y)/(np.linalg.norm(y)+1e-15))
    errs=[]
    for k in [train,hold]:
        before=norm(old[k],target[k]); after=norm((old+alpha*correction)[k],target[k]);
        errs.append(dict(early_only=before,early_plus_late=after))
    # If residual improvement is too small, candidate is NOT auto-enabled.
    pass_gate=bool(errs[1]['early_plus_late']<errs[1]['early_only']*.98 and
                   errs[0]['early_plus_late']<errs[0]['early_only']*.99)
    return modes,coeffs,dict(train=errs[0],heldout_bins=errs[1],
                pass_gate=pass_gate,mode_count=len(modes),
                note='heldout bins are correlated within ONE recording; no physical-room late field claim')

def save_header(path,modes,coeff):
    def fmt(x):return f'{float(x):.11g}'
    lines=['// AUTO-GENERATED from Robert Mores nearfield/bridge recordings, Guitar #1.',
    '// 1.6 long-tail residues CONDITIONED on measurement; not an independently',
    '// validated room-free acoustic-radiation measurement. Late time >125ms',
    '// may contain recorded room reflections. See FIT_LONG_RADIATION16.json.',
    '#pragma once','#include <array>','namespace pmg {','inline constexpr int kLongModeCount16=16;',
    'struct RadiationPole16 { double hz,tau; std::array<double,8> residues; };',
    'inline constexpr bool kRadiationModesMeasured16=true;',
    'inline constexpr std::array<RadiationPole16,kLongModeCount16> kRadiationModes16{{']
    for i,(hz,tau) in enumerate(modes):
        nums=[coeff[j,2*i+q] for j in range(4) for q in (0,1)]
        lines.append('    {'+fmt(hz)+','+fmt(tau)+',{'+','.join(map(fmt,nums))+'}},')
    lines+=['}};','}']
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text('\n'.join(lines)+'\n')

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--cache',type=Path,default=Path('research/qualified_selected_impulses.mat'))
    ap.add_argument('--header',type=Path,default=Path('DSP/LongRadiation16Coeffs.h'))
    ap.add_argument('--report',type=Path,default=Path('research/FIT_LONG_RADIATION16.json'))
    args=ap.parse_args()
    f,h,meta=longer_matrix(args.cache)
    modes,c,results=estimate(f,h)
    if len(modes)!=MODES:raise RuntimeError('Not enough candidate modes')
    save_header(args.header,modes,c)
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps({'source':'https://zenodo.org/records/4604577',
       'physical_mode_prior':'PMG09 two-port bridge fit, guitar #1, same source',
       'measurement':meta,'stats':results,'poles':[{'hz':f,'tau_seconds':t} for f,t in modes],
       'no_raw_audio_distributed':True,'fit_is_not_real_guitar_listening_test':True},indent=2))
    print('FITTING',json.dumps(results),flush=True)
    print('SAVED',args.header,args.report,flush=True)
if __name__=='__main__':main()
