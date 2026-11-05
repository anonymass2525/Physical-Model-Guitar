#!/usr/bin/env python3
"""Experimental scalar driving-point bridge mobility fit from REAL force/acceleration.
Data: R. Mores, Zenodo 4604577, 65 guitar corpus, 48kHz.
Input file is never bundled into artifacts. Sensor channels 2/3 are bass/treble
POSITION sensors on topplate, not orthogonal vector directions.
Nonnegative modal residues -> positive-real collocated scalar admittance.
"""
import argparse, json, hashlib, urllib.request
from pathlib import Path
import numpy as np
from scipy.io import loadmat
from scipy import signal, optimize
SOURCE='https://zenodo.org/records/4604577'
URL=SOURCE+'/files/qualified_selected_impulses.mat?download=1'
MD5='733cb10baf5ce36d8bf333610ffbb260'
FS=48000
M_V_TO_FORCE=(10000./92.90)*4.4482
M_V_TO_ACCEL=(10000./10.64)*9.80665


def get_measurements(path):
    if not path.exists():
        req=urllib.request.Request(URL,headers={'User-Agent':'PMG08-BridgeScience/1.0'})
        with urllib.request.urlopen(req,timeout=130) as source,path.open('wb') as out:
            while part:=source.read(1<<20):out.write(part)
    if hashlib.md5(path.read_bytes()).hexdigest()!=MD5:
        raise ValueError('Zenodo measurement checksum mismatch')
    a=loadmat(path,variable_names=['qualified_selected_impulses'])['qualified_selected_impulses']
    if a.shape!=(65,144000,6):raise ValueError(f'Unexpected measurement tensor {a.shape}')
    return a


def frf(raw,excitation_index,sensor_index):
    n=48000
    d=raw[excitation_index*n:(excitation_index+1)*n]
    force=np.asarray(d[:,0],dtype=float)*M_V_TO_FORCE
    accel=np.asarray(d[:,sensor_index],dtype=float)*M_V_TO_ACCEL
    # Same one-second cosine-tail taper as the dataset's plotting script.
    taper=.5+.5*np.cos(np.pi*np.arange(1,n+1)/n)
    f=np.fft.rfftfreq(n,1/FS)
    F=np.fft.rfft(force*taper)
    A=np.fft.rfft(accel*taper)
    power=np.abs(F)**2
    # Fourier acceleration -> velocity, then velocity/force. Avoid ω=0.
    regularizer=max(float(power.max())*1e-9,1e-20)
    omega=np.maximum(2*np.pi*f,1.)
    Y=(A*np.conj(F)/(power+regularizer))/(1j*omega)
    return f,Y,power


def scalar_fit(f,H,power,maxModes=10):
    valid=(f>=65)&(f<=1800)&(power>power.max()*1e-5)
    freq=f[valid];y=H[valid]
    # One spectral sample per 3Hz; residual train / heldout bins separated.
    take=(np.rint(freq).astype(int)%3==0)
    freq=freq[take];y=y[take]
    if len(freq)<250:raise ValueError('Insufficient reliable force-excited frequency bins')
    # Sensor wiring may reverse displacement sign. Report, rather than hiding it.
    rawRealFraction=float(np.mean(y.real>=-1e-8))
    polarity=1 if np.sum(y.real*np.abs(y))>=0 else -1
    y*=polarity
    amp=np.abs(y)
    smooth=np.convolve(amp,np.ones(5)/5,mode='same')
    peak,minprom=signal.find_peaks(smooth,prominence=max(float(smooth.max())*.012,1e-7),distance=5)
    indexes=sorted(peak,key=lambda i:smooth[i],reverse=True)[:maxModes]
    indexes=sorted(indexes,key=lambda i:freq[i])
    if len(indexes)<4:raise ValueError('Insufficient resonances for usable modal fit')
    freqModes=[];qModes=[]
    for idx in indexes:
        p=smooth[idx];cut=p/np.sqrt(2)
        lo=idx;hi=idx
        while lo>0 and smooth[lo]>cut:lo-=1
        while hi<len(smooth)-1 and smooth[hi]>cut:hi+=1
        bw=max(3.,float(freq[hi]-freq[lo]))
        freqModes.append(float(freq[idx]));qModes.append(float(np.clip(freq[idx]/bw,1.1,55.)))
    # Positive scalar modal admittance: Y = Σ a jω/(ω_j² - ω²+jωω_j/Q_j), a=1/m>0.
    # Only positive coefficients are allowed; no unstable negative damping.
    w=2*np.pi*freq
    design=np.stack([(1j*w)/((2*np.pi*fm)**2-w*w + 1j*w*(2*np.pi*fm)/q)
                     for fm,q in zip(freqModes,qModes)],axis=1)
    train=np.arange(len(freq))%5!=0
    heldout=~train
    # Noise floor prevents a few quiet bins from dominating the objective.
    floor=float(np.percentile(amp,35))
    weight=1/np.maximum(amp,floor)
    def pack(a,msk):
        aa=a[msk]*weight[msk,None] if a.ndim==2 else a[msk]*weight[msk]
        return np.concatenate((aa.real,aa.imag),axis=0)
    A=pack(design,train);b=pack(y,train)
    res=optimize.lsq_linear(A,b,bounds=(0,np.inf),max_iter=200,tol=1e-9)
    coefs=np.maximum(res.x,0)
    predicted=design@coefs
    def normerr(m):
        d=(predicted[m]-y[m])*weight[m]
        return float(np.linalg.norm(d)/max(np.linalg.norm(y[m]*weight[m]),1e-12))
    measured_phase_degree=float(np.median(np.angle(y[heldout],deg=True)))
    positive_real_fraction=float(np.mean(predicted.real>=-1e-10))
    # Fitted scalar model is evaluated on unseen frequency bins, not independent
    # hammer impacts; do not call this generalisation to other instruments.
    modes=[dict(frequency_hz=round(fm,3),Q=round(q,3),inverse_effective_mass=round(float(a),8))
           for fm,q,a in zip(freqModes,qModes,coefs)]
    return dict(number_modes=len(modes),modes=modes,
                heldout_relative_complex_error=normerr(heldout),
                training_relative_complex_error=normerr(train),
                positive_real_fraction=positive_real_fraction,
                measured_real_nonnegative_fraction=rawRealFraction,
                sensor_polarity_assumption=polarity,
                heldout_phase_median_deg=measured_phase_degree,
                good_frequency_bins=int(len(freq)),
                floor_35pct_m_per_N_s=floor)


def main():
    ap=argparse.ArgumentParser();ap.add_argument('--dataset',type=Path,default=Path('bridge-probe/qualified_selected_impulses.mat'))
    ap.add_argument('--output',type=Path,default=Path('bridge-results'))
    ap.add_argument('--guitars',type=int,nargs='+',default=[1,3,10])
    a=ap.parse_args();a.output.mkdir(exist_ok=True,parents=True);a.dataset.parent.mkdir(exist_ok=True,parents=True)
    corpus=get_measurements(a.dataset)
    out={'source':SOURCE,'doi':'10.5281/zenodo.4604577','license_notice':'dataset authors request attribution',
         'raw_measured_samples_included':False,
         'units':{'hammer':'N','sensor':'m/s²','mobility':'m/(N*s)'},
         'measurement_note':'two bridge locations (bass/treble), NOT two perpendicular wave polarizations',
         'guitars':{}}
    for guitar in a.guitars:
        assert 1<=guitar<=65
        rec=corpus[guitar-1,:,:]
        results={}
        for label,excite,sensor in [('bass_collocated',0,2),('treble_collocated',2,1),
                                    ('bass_to_treble_transfer',0,1),('treble_to_bass_transfer',2,2)]:
            try:
                f,y,power=frf(rec,excite,sensor)
                results[label]=scalar_fit(f,y,power)
            except Exception as e:results[label]={'error':str(e)}
        # Reciprocal cross-admittance is a cross-check for matching geometry.
        out['guitars'][str(guitar)]=results
        print('GUITAR',guitar,[(k,round(v.get('heldout_relative_complex_error',99),3),v.get('number_modes',0)) for k,v in results.items()],flush=True)
    (a.output/'GUITAR_BRIDGE_MODAL_FITS.json').write_text(json.dumps(out,indent=2),encoding='utf-8')
    print('REPORT',a.output/'GUITAR_BRIDGE_MODAL_FITS.json',flush=True)

if __name__=='__main__':main()
