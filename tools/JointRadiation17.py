#!/usr/bin/env python3
"""Research v1.7: identify TWO simultaneously observed source-reference paths.

Use real, synchronized 4-channel PCM recordings of ONE guitar. The channels
must carry independent velocity-referenced string, bridge, and two microphone
signals with a common capture clock. The reference sensors must be calibrated
for velocity units before absolute-pressure interpretation. Correlated inputs
are rejected instead of 'separating' one microphone waveform by assumption.

Do not equate a measured bridge-to-mic IR with isolated string/body radiation.
This tool estimates *conditional* transfer functions to recorded reference
signals. It cannot prove complete separation without physical source controls.
"""
import argparse
import json
from pathlib import Path
import numpy as np
import soundfile as sf
from scipy.signal import stft, istft, fftconvolve


def _frames(path, fs):
    y, rate = sf.read(path, always_2d=True, dtype='float64')
    if rate != fs or y.shape[1] != 4 or y.shape[0] < fs:
        raise ValueError(f'{path}: require >1s synchronized 4-channel PCM at {fs}Hz')
    if not np.isfinite(y).all() or any(np.max(abs(y[:,i])) < 1.e-7 for i in range(4)):
        raise ValueError(f'{path}: nonfinite or missing channel')
    if np.max(abs(y)) >= .999 and np.issubdtype(y.dtype, np.floating):
        raise ValueError(f'{path}: possible clipped samples (peak >=.999)')
    return y


def fit(manifest, root, nperseg=8192, cond_limit=80., reg=3e-3):
    fs=int(manifest['sample_rate'])
    if fs < 22050 or fs>192000:
        raise ValueError('sample rate out of supported range')
    if manifest['channel_order'] != ['string_velocity_reference','bridge_velocity_reference',
                                     'mic_left_pressure','mic_right_pressure']:
        raise ValueError('explicit four-channel order required')
    takes=manifest['takes']
    if len(takes)<4 or len({x['file'] for x in takes})!=len(takes):
        raise ValueError('need four or more independent multichannel takes')
    if not all(x.get('guitar_id')==manifest['guitar_id'] and
               x.get('mic_geometry_id')==manifest['mic_geometry_id'] and
               x.get('simultaneous_capture') is True for x in takes):
        raise ValueError('same instrument, same mic geometry, simultaneous capture needed')
    # Stronger claim about capture clocks/sensor calibration is not inferred.
    arrays=[_frames(root/x['file'],fs) for x in takes]
    k=max(512,int(nperseg))
    hop=k//4
    def spec(arr):
        return np.asarray([stft(arr[:,i],fs=fs,nperseg=k,noverlap=k-hop,boundary='zeros')[2]
                           for i in range(4)])
    spect=[spec(a) for a in arrays]
    freqs=np.fft.rfftfreq(k,1/fs)
    train=spect[:-1]
    sums=np.zeros((len(freqs),4,4),dtype=np.complex128)
    # 2x2 cross-spectral reference Gram and pressure/reference covariance.
    # stack segments over physical takes to avoid single-note rank problems.
    X=np.concatenate([v[:2] for v in train],axis=2)
    Y=np.concatenate([v[2:] for v in train],axis=2)
    H=np.zeros((2,2,len(freqs)),dtype=complex)
    coherence=np.zeros(len(freqs))
    condition=np.zeros(len(freqs))
    energies=np.zeros(len(freqs))
    for i in range(len(freqs)):
        a=X[:,i,:]
        b=Y[:,i,:]
        gram=a@a.conj().T
        energies[i]=np.real(np.trace(gram))
        if energies[i]<1e-17: continue
        d=np.sqrt(max(0.,np.real(gram[0,0]*gram[1,1])))
        coherence[i]=float(np.abs(gram[0,1])/d) if d else 1.
        diag=np.diag(gram).real
        avg=max(float(np.trace(gram).real/2),1e-30)
        condition[i]=float(np.linalg.cond(gram/avg+np.eye(2)*1.e-6))
        lam=reg*avg
        H[:,:,i]=(b@a.conj().T)@np.linalg.inv(gram+lam*np.eye(2))
    active=(freqs>100)&(freqs<min(8000.,.45*fs))&(energies>np.max(energies)*1e-6)
    if active.sum()<15: raise ValueError('too little excited frequency range')
    weak=float(np.mean((coherence[active]>.98)|(condition[active]>cond_limit)))
    if weak>.35:
        raise ValueError(f'UNIDENTIFIABLE: {weak:.1%} active bins highly collinear; '
                         'use multiple independent excitations/sensor references, '
                         'do not claim direct/body source separation')
    # Reject if the two recorded source channels are effectively the same
    # even when only a few strongly excited frequency bins are retained.
    baseline=np.zeros_like(H)
    baseline[:,1,:] = np.sum(Y*np.conj(X[1:2,:,:]),axis=2) / (np.sum(np.abs(X[1:2,:,:])**2,axis=2)+reg*energies)
    def validate(h):
        hold=spect[-1]
        fitted=np.einsum('mrf,rft->mft',h,hold[:2])
        y=hold[2:]
        err=np.sum(np.abs(fitted-y)**2)
        energy=np.sum(np.abs(y)**2)
        return float(np.sqrt(err/max(energy,1e-25)))
    v2=validate(H)
    v1=validate(baseline)
    # Transfer phases/delays are relative to sensor reference waveforms;
    # they include sensor and synchronization delays unless calibrated away.
    paths=[]
    for m in range(2):
        per_mic=[]
        for ref in range(2):
            h=H[m,ref,:].copy()
            h[0]=h[0].real
            h[-1]=h[-1].real
            impulse=np.fft.irfft(h,n=k)
            # Only report causal part: noncausal energy is a red flag.
            after=impulse[:k//2]
            before=impulse[k//2:]
            noncausal=float(np.sum(before**2)/(np.sum(impulse**2)+1e-30))
            arrival=int(np.argmax(np.abs(after)))
            periods=np.arange(arrival+int(.01*fs),min(k//2,arrival+int(.09*fs)),max(1,int(.002*fs)))
            if len(periods)>5:
                # broad energy trend of late impulse, qualitative in small corpus.
                env=np.array([np.sqrt(np.mean(after[t:min(t+int(.008*fs),len(after))]**2)+1e-24) for t in periods])
                slope=np.polyfit(periods/fs,np.log(np.maximum(env,1e-12)),1)[0]
                tau=float(-1/slope) if slope<-.1 else None
            else: tau=None
            per_mic.append({'mic':m,'source':('string','bridge')[ref],
                            'peak_arrival_ms':round(1000*arrival/fs,4),
                            'late_amplitude_tau_s':None if tau is None else round(tau,5),
                            'noncausal_ir_energy_ratio':round(noncausal,5),
                            'frequency_hz':freqs[::max(1,len(freqs)//240)].round(4).tolist(),
                            'transfer_real':h[::max(1,len(freqs)//240)].real.round(7).tolist(),
                            'transfer_imag':h[::max(1,len(freqs)//240)].imag.round(7).tolist()})
        paths.extend(per_mic)
    report={'sample_rate':fs,'nperseg':k,'train_takes':len(takes)-1,'holdout_take':takes[-1]['file'],
            'active_bins':int(active.sum()),'fraction_rank_unreliable':round(weak,5),
            'median_reference_coherence':round(float(np.median(coherence[active])),5),
            'median_reference_condition':round(float(np.median(condition[active])),5),
            'heldout_two_source_normalized_stft_error':round(v2,6),
            'heldout_body_only_normalized_stft_error':round(v1,6),
            'identified_conditional_transfer':bool(v2<v1*.98 and weak<.35),
            'physical_separation_proven':False,
            'path_estimations':paths,
            'warning':('The estimated transfers are conditional on the TWO sensor references, '
                        'not independent true guitar radiation responses. Absolute Pa/m/s '
                        'requires calibrated sensor gains, microphone geometry and timing. '
                        'Archive bridge/mic data alone do not contain isolated string radiation.')}
    return report


def main():
    p=argparse.ArgumentParser()
    p.add_argument('--manifest',required=True)
    p.add_argument('--out',required=True)
    p.add_argument('--nperseg',type=int,default=8192)
    args=p.parse_args()
    path=Path(args.manifest).resolve()
    manifest=json.loads(path.read_text(encoding='utf8'))
    result=fit(manifest,path.parent,args.nperseg)
    Path(args.out).write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf8')
    print(json.dumps({k:v for k,v in result.items() if k!='path_estimations'},ensure_ascii=False,indent=2))

if __name__=='__main__': main()
