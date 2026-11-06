#!/usr/bin/env python3
"""Fit reciprocal positive-real 2x2 mechanical admittance for Mores guitar data.

Experiment / quality gate, NOT a validated implementation preset. The fit uses
rank-one PSD modal residues d_k d_k^T (guarantees positive-real Y if the
resonance has positive damping), while complex measurement transfer data can
be noisy, non-collocated and non-reciprocal after sensor/window corrections.

Reference: https://zenodo.org/records/4604577 ; no raw archive redistributed.
"""
from pathlib import Path
import argparse,json
import numpy as np
from scipy import signal,optimize
from FitZenodoBridge08 import get_measurements,frf,SOURCE

def fit_one(raw,number_modes):
    bands={name:frf(raw,drive,sense) for name,drive,sense in [
        ('bb',0,2),('tt',2,1),('bt',2,2),('tb',0,1)]}
    f=bands['bb'][0]; assert all(np.array_equal(f,x[0]) for x in bands.values())
    obs={k:v[1].copy() for k,v in bands.items()}
    # Sensor polarity on each driving-point pair is identified separately.
    polarities={}
    mask=(f>=70)&(f<=1600)
    for name in ('bb','tt'):
        y=obs[name][mask]
        polarity=1. if np.sum(y.real*np.maximum(1e-12,np.abs(y)))>=0 else -1.
        obs[name]*=polarity;polarities[name]=int(polarity)
    # Off-diagonal channels are corrected into one consistent coordinate frame.
    # If force/accelerometer signs disagree, report rather than hide it.
    rawBt=obs['bt'].copy();rawTb=obs['tb'].copy()
    if np.vdot(rawBt[mask],rawTb[mask]).real<0.:obs['tb']*=-1.
    cross=.5*(obs['bt']+obs['tb'])
    reciprocity=float(np.linalg.norm(obs['bt'][mask]-obs['tb'][mask])/
                      max(np.linalg.norm(obs['bt'][mask])+np.linalg.norm(obs['tb'][mask]),1e-15)*2)
    valid=(f>=70)&(f<=1600)&(bands['bb'][2]>bands['bb'][2].max()*1e-6)&(bands['tt'][2]>bands['tt'][2].max()*1e-6)
    valid &= (np.rint(f).astype(int)%2==0)
    fq=f[valid]; bb=obs['bb'][valid];tt=obs['tt'][valid];bt=cross[valid]
    if len(fq)<220:raise ValueError(f'not enough usable bins: {len(fq)}')
    # Common resonances across both sensor locations; more modes can be fitted
    # but this initial 10-mode model should remain comparable to v0.8.
    spectra=np.abs(bb)+np.abs(tt)
    smoothed=np.convolve(spectra,np.ones(5)/5,mode='same')
    peaks,_=signal.find_peaks(smoothed,distance=5,prominence=max(smoothed.max()*.01,1e-7))
    peaks=sorted(peaks,key=lambda k:smoothed[k],reverse=True)
    if len(peaks)<number_modes:raise ValueError(f'only {len(peaks)} modal candidates')
    peaks=sorted(peaks[:number_modes],key=lambda k:fq[k]); frequencies=[];qvalues=[]
    for i in peaks:
        cutoff=smoothed[i]/np.sqrt(2.);left=right=i
        while left>0 and smoothed[left]>cutoff:left-=1
        while right<len(fq)-1 and smoothed[right]>cutoff:right+=1
        bandwidth=max(4.,float(fq[right]-fq[left]))
        frequencies.append(float(fq[i]));qvalues.append(float(np.clip(fq[i]/bandwidth,1.5,65.)))
    omega=2*np.pi*fq
    basis=np.stack([1j*omega/((2*np.pi*fm)**2-omega*omega+1j*omega*(2*np.pi*fm)/q)
                    for fm,q in zip(frequencies,qvalues)],axis=1)
    floor=float(max(np.percentile(spectra,20),1e-7))
    weight=1./np.maximum(spectra,floor)
    train=(np.arange(len(fq))%5!=0);hold=~train
    # Fit strictly-positive diagonal mobility as initialization.
    X=np.concatenate([(basis[train]*weight[train,None]).real,
                      (basis[train]*weight[train,None]).imag])
    def diagfit(y):
        return optimize.lsq_linear(X,np.r_[(y[train]*weight[train]).real,
                                           (y[train]*weight[train]).imag],
                                   bounds=(0,np.inf),tol=1e-8,max_iter=120).x
    Cbb=diagfit(bb);Ctt=diagfit(tt)
    real_cross=np.real(np.conj(basis).T@(bt*weight))
    signed=np.where(real_cross<0.,-1.,1.)
    initial=np.stack((np.sqrt(np.maximum(Cbb,1e-10)),
                      signed*np.sqrt(np.maximum(Ctt,1e-10))),axis=1)
    # Optimize all modal direction vectors together so all 4 complex entries
    # are implied by one PSD matrix; no independent negative cross gains.
    def predict(vector):
        rr=vector.reshape(number_modes,2)
        return basis@(rr[:,0]**2),basis@(rr[:,1]**2),basis@(rr[:,0]*rr[:,1])
    def residual(vector):
        hbb,htt,hbt=predict(vector)
        parts=[]
        for modeled,observed,importance in ((hbb,bb,1.),(htt,tt,1.),(hbt,bt,.8)):
            z=(modeled[train]-observed[train])*weight[train]*importance
            parts.extend((z.real,z.imag))
        # Soft regularization reduces wild opposing modal residues.
        return np.r_[*parts,.0002*(vector-initial.reshape(-1))]
    fit=optimize.least_squares(residual,initial.reshape(-1),max_nfev=80,
                               ftol=1e-7,xtol=1e-7,gtol=1e-7)
    est=fit.x.reshape(number_modes,2)
    pbb,ptt,pbt=predict(fit.x)
    def error(modeled,observed,mask):
        r=(modeled[mask]-observed[mask])*weight[mask]
        return float(np.linalg.norm(r)/max(np.linalg.norm(observed[mask]*weight[mask]),1e-12))
    def errors(mask):return {key:error(v,observed,mask) for key,v,observed in
        [('bb',pbb,bb),('tt',ptt,tt),('bt_mean',pbt,bt)]}
    # Passivity check on dense 20-1800Hz grid: symmetric Hermitian real-part
    # PSD -> dissipates energy under sinusoidal two-port force excitation.
    testw=2*np.pi*np.linspace(20,1800,901)
    B=np.stack([1j*testw/((2*np.pi*fm)**2-testw*testw+1j*testw*(2*np.pi*fm)/q)
                for fm,q in zip(frequencies,qvalues)],axis=1)
    realbb=(B@(est[:,0]**2)).real;realtt=(B@(est[:,1]**2)).real
    realbt=(B@(est[:,0]*est[:,1])).real
    minEigen=float(np.min(.5*(realbb+realtt)-.5*np.sqrt((realbb-realtt)**2+4*realbt**2)))
    modes=[{'frequencyHz':round(fm,3),'Q':round(q,3),
            'inverseMass':round(float(est[k,0]**2+est[k,1]**2),8),
            'directionBass':round(float(est[k,0]/max(np.linalg.norm(est[k]),1e-20)),6),
            'directionTreble':round(float(est[k,1]/max(np.linalg.norm(est[k]),1e-20)),6)}
            for k,(fm,q) in enumerate(zip(frequencies,qvalues))]
    return dict(numberMeasuredFrequencyBins=len(fq),polarityCorrections=polarities,
        measuredReciprocityMismatch=reciprocity,trainingError=errors(train),
        holdoutError=errors(hold),denseRealHermitianMinimumEigen=minEigen,
        optimiserSuccessful=bool(fit.success),fittedModes=modes,
        note='Heldout = other frequency bins of the SAME hammer impacts, NOT independent instruments or impacts.')

def main():
    p=argparse.ArgumentParser();p.add_argument('--output',type=Path,default=Path('bridge-fit09'))
    p.add_argument('--dataset',type=Path,default=Path('bridge-fit09/qualified_selected_impulses.mat'))
    p.add_argument('--guitars',type=int,nargs='+',default=[1,3,10]);p.add_argument('--modes',type=int,default=10)
    a=p.parse_args();a.output.mkdir(parents=True,exist_ok=True);a.dataset.parent.mkdir(parents=True,exist_ok=True)
    raw=get_measurements(a.dataset)
    out={'source':SOURCE,'doi':'10.5281/zenodo.4604577','rawSamplesIncluded':False,
        'architecture':'2-point positive-real reciprocal rank-1-PSD modal residues','guitars':{}}
    for guitar in a.guitars:
        try:r=fit_one(raw[guitar-1],a.modes)
        except Exception as e:r={'error':str(e)}
        out['guitars'][str(guitar)]=r
        print('GUITAR',guitar,'RESULT',json.dumps({k:v for k,v in r.items() if k!='fittedModes'}),flush=True)
    target=a.output/'TWO_PORT_09_MEASURED_MATRIX_FITS.json'
    target.write_text(json.dumps(out,indent=2),encoding='utf-8')
    print('REPORT',target)
if __name__=='__main__':main()
