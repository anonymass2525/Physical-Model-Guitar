#!/usr/bin/env python3
"""Compare a physical-model single note with a matching real guitar WAV.

Research tool, not an audio sampler. No reference audio is embedded in the VST3.
Dependencies: pip install numpy scipy soundfile

Usage:
 python tools/CompareRealGuitar.py --reference ref_E3.wav \
   --synthetic PMG04_steel_E3.wav --midi 52 --kind steel --out metrics.json

Require identical pitch/string, comparable picking velocity, an unprocessed
reference, and no amp or room processing difference. Electric must be DI.
Steel/nylon require consistent close-mic position before numerical fitting.
"""
from __future__ import annotations
import argparse
import json
import math
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy import signal


def read_audio(path: Path, target_sr=48000):
    x, sr = sf.read(path, dtype="float64", always_2d=True)
    x = x.mean(axis=1)
    if sr != target_sr:
        common = math.gcd(int(sr), target_sr)
        x = signal.resample_poly(x, target_sr // common, int(sr) // common)
    return signal.detrend(x, type="constant"), target_sr


def align_onset(x, sr):
    """Attack-alignment using smoothed 2 ms RMS and a robust noise-floor gate."""
    envelope = np.sqrt(np.convolve(x*x, np.ones(max(8, int(.002*sr)))/max(8, int(.002*sr)), mode="same"))
    peak = float(np.max(envelope)) if len(x) else 0.0
    if peak < 1e-8:
        raise ValueError("Audio is silent or too quiet to analyse")
    noise = float(np.median(envelope[:max(1, min(len(envelope)//8, int(.035*sr)))]))
    threshold = min(max(peak*.035, noise*4.0), peak*.20)
    candidates = np.flatnonzero(envelope > threshold)
    if len(candidates) == 0:
        raise ValueError("No detectable attack")
    onset = max(0, int(candidates[0])-int(.001*sr))
    return x[onset:], onset/sr


def stable_rms(x, sr, start, end):
    a,b = max(0,int(start*sr)), min(len(x),int(end*sr))
    return float(np.sqrt(np.mean(x[a:b]**2))) if b>a else 0.0


def fft_data(x, sr, start, duration):
    a = int(start*sr)
    n = min(len(x)-a, int(duration*sr))
    if n < 64:
        return np.array([0.0]), np.array([0.0])
    frame = x[a:a+n]
    win = signal.windows.hann(n, sym=False)
    spectrum = abs(np.fft.rfft(frame*win)) / (np.sum(win)*.5+1e-12)
    return np.fft.rfftfreq(n,1/sr), spectrum


def harmonic_features(x, sr, f0, count=16):
    """Narrow-band peak tracking: robust to small real-world inharmonicity."""
    times=np.arange(.10, min(1.75, len(x)/sr-.12), .06)
    levels=np.full((count, len(times)), np.nan)
    frequencies=np.full_like(levels,np.nan)
    for j,t in enumerate(times):
        f,s=fft_data(x,sr,float(t),.12)
        for k in range(1,count+1):
            target=k*f0
            if target > sr*.45: continue
            width=max(6.0,target*.025)
            select=np.where((f>=target-width)&(f<=target+width))[0]
            if len(select):
                i=select[np.argmax(s[select])]
                levels[k-1,j]=max(float(s[i]),1e-12)
                frequencies[k-1,j]=float(f[i])
    db=20*np.log10(levels)
    fit=[]
    for k in range(count):
        ok=np.isfinite(db[k,:])
        if np.count_nonzero(ok)<4:fit.append(None);continue
        # Reject partials that decay into the noise floor.
        good=ok & (db[k,:]>np.max(db[k,ok])-42)
        if np.count_nonzero(good)<4:fit.append(None);continue
        slope,intercept=np.polyfit(times[good], db[k,good],1)
        resid=db[k,good]-(slope*times[good]+intercept)
        fit.append({"harmonic":k+1,"decay_db_per_sec":float(slope),
                    "irregularity_db_std":float(np.std(resid)),
                    "start_relative_db":float(db[k,0]-db[0,0]) if np.isfinite(db[k,0]) and np.isfinite(db[0,0]) else None})
    return fit


def pitch_track(x,sr,f0):
    values=[]
    for t in np.arange(.045,min(.95,len(x)/sr-.08),.035):
        a=int(t*sr);n=int(.075*sr)
        y=x[a:a+n]
        if len(y)<n or np.sqrt(np.mean(y*y))<1.e-6:continue
        y=y-np.mean(y)
        ac=signal.correlate(y,y,mode='full',method='fft')[n-1:]
        laglo=max(2,int(sr/(f0*1.09)));laghi=min(n-3,int(sr/(f0*.91)))
        if laghi<=laglo:continue
        p=laglo+int(np.argmax(ac[laglo:laghi+1]))
        den=ac[p-1]-2*ac[p]+ac[p+1]
        delta=float(.5*(ac[p-1]-ac[p+1])/den) if abs(den)>1.e-10 else 0
        actual=sr/(p+np.clip(delta,-.5,.5))
        values.append((float(t),float(1200*np.log2(actual/f0))))
    return values


def features(x,sr,f0):
    x,offset=align_onset(x,sr)
    total=min(len(x)/sr,2.0)
    onset_levels={f"{round(a*1000)}-{round(b*1000)}ms":stable_rms(x,sr,a,b)
                  for a,b in [(0,.010),(.010,.030),(.030,.060),(.060,.100),(.100,.200)]}
    f,s=fft_data(x,sr,0,.060)
    centroid=float(np.sum(f*s)/max(np.sum(s),1e-12))
    pitch=pitch_track(x,sr,f0)
    return {"onset_seconds_in_file":offset,"duration_seconds":total,
            "onset_rms":onset_levels,"attack_centroid_hz":centroid,
            "harmonics":harmonic_features(x,sr,f0),
            "pitch_cents":pitch,
            "peak":float(np.max(np.abs(x))),
            "rms_0_5s":stable_rms(x,sr,.01,.5)}


def compare(ref,syn):
    # Avoid nonphysical comparisons of raw loudness; align RMS per note in analysis.
    sr=ref['rms_0_5s'] / max(syn['rms_0_5s'],1e-12)
    result={"synthetic_gain_to_match_reference":sr,
            "attack_centroid_hz_difference":syn['attack_centroid_hz']-ref['attack_centroid_hz']}
    ratios={}
    for key,ref_rms in ref['onset_rms'].items():
        syn_rms=syn['onset_rms'][key]*sr
        ratios[key]=float(20*np.log10(max(syn_rms,1e-9)/max(ref_rms,1e-9)))
    result['attack_window_difference_db']=ratios
    harm=[]
    for a,b in zip(ref['harmonics'],syn['harmonics']):
        if a is None or b is None:continue
        harm.append({"harmonic":a['harmonic'],
                     "decay_slope_difference_db_per_sec":b['decay_db_per_sec']-a['decay_db_per_sec'],
                     "irregularity_difference_db":b['irregularity_db_std']-a['irregularity_db_std'],
                     "early_level_difference_db":None if a['start_relative_db'] is None or b['start_relative_db'] is None else b['start_relative_db']-a['start_relative_db']})
    result['harmonics']=harm
    def med(v): return float(np.median(v)) if v else None
    result['initial_pitch_cents_difference']=None
    pr=[c for t,c in ref['pitch_cents'] if t<.14]
    ps=[c for t,c in syn['pitch_cents'] if t<.14]
    if pr and ps: result['initial_pitch_cents_difference']=med(ps)-med(pr)
    result['notes']=["Unmatched microphone/pickup positions change apparent timbre.",
                     "No subjective realism score is inferred from these metrics.",
                     "Harmonic slope is measured only while each partial is above its own noise floor."]
    return result


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reference',type=Path,required=True)
    parser.add_argument('--synthetic',type=Path,required=True)
    parser.add_argument('--midi',type=int,required=True)
    parser.add_argument('--kind',choices=['electric','steel','nylon'],required=True)
    parser.add_argument('--out',type=Path,default=Path('PMG04_reference_comparison.json'))
    args=parser.parse_args()
    if not 40<=args.midi<=88:parser.error('MIDI note must be 40..88')
    f0=440*2**((args.midi-69)/12)
    ref,sr=read_audio(args.reference)
    synth,_=read_audio(args.synthetic,target_sr=sr)
    record={"model":args.kind,"midi_note":args.midi,"fundamental_hz":f0,
            "reference_path":str(args.reference),"synthetic_path":str(args.synthetic),
            "reference":features(ref,sr,f0),"synthetic":features(synth,sr,f0)}
    record['comparison']=compare(record['reference'],record['synthetic'])
    args.out.parent.mkdir(parents=True,exist_ok=True)
    args.out.write_text(json.dumps(record,indent=2,ensure_ascii=False),encoding='utf-8')
    print('Wrote',args.out)
    print('Attack centroid difference (synth - real):',round(record['comparison']['attack_centroid_hz_difference']), 'Hz')
    print('Early pitch offset (synth - real):',record['comparison']['initial_pitch_cents_difference'],'cents')
    for x in record['comparison']['harmonics'][:8]:
        print('H%02d: decay Δ%+.2f dB/sec'% (x['harmonic'],x['decay_slope_difference_db_per_sec']))


if __name__=='__main__':main()
