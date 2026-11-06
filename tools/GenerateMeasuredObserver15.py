#!/usr/bin/env python3
"""Generate research observer coefficients from actual published complex measurements.

Electric: measured loaded / unloaded pickup network-analyzer complex transfer,
  NOT a magnetic flux density field map or absolute induced string signal.
Nylon: a two-input/two-output complex near-field pressure/velocity map from the
  2021 Mores impulse measurements, NOT a far-field directivity sphere.
Raw third-party datasets are neither vendored nor distributed. A held-out *frequency
bin* check estimates transfer approximation only; not independent instruments.
"""
import argparse, csv, hashlib, json, urllib.request
from pathlib import Path
from io import StringIO
import numpy as np
from scipy.io import loadmat

PICKUP_URL='https://raw.githubusercontent.com/strayr/pickupAnalyseR-data/master/Guitar/DiMarzio/AirNorton/AirNorton-{}.csv'
ZENODO_URL='https://zenodo.org/records/4604577/files/qualified_selected_impulses.mat?download=1'
ZENODO_MD5='733cb10baf5ce36d8bf333610ffbb260'
FS=48000
NFIR=96

def get_text(url):
    req=urllib.request.Request(url,headers={'User-Agent':'PMG15-measured-observers/0.1'})
    with urllib.request.urlopen(req,timeout=90) as r:return r.read().decode('utf-8-sig')

def get_file(url,path):
    if path.exists():return
    path.parent.mkdir(parents=True,exist_ok=True)
    req=urllib.request.Request(url,headers={'User-Agent':'PMG15-measured-observers/0.1'})
    with urllib.request.urlopen(req,timeout=150) as r, path.open('wb') as out:
        while chunk:=r.read(1<<20):out.write(chunk)

def pickup_data():
    readings={}
    for suffix in ('UL','LD'):
        text=get_text(PICKUP_URL.format(suffix))
        table=list(csv.DictReader(StringIO(text)))
        f=np.array([float(r['Frequency [Hz]']) for r in table])
        mag=np.array([float(r['Magnitude [dB]']) for r in table])
        phase=np.array([float(r['Phase [deg]']) for r in table])
        qual=np.array([int(r['Quality']) for r in table])
        readings[suffix]=(f,10**(mag/20)*np.exp(1j*np.deg2rad(phase)),qual)
    f,ld,q=readings['LD']
    f0,ul,q0=readings['UL']
    if not np.allclose(f,f0):
        ul=np.interp(f,f0,ul.real)+1j*np.interp(f,f0,ul.imag)
    ratio=ld/(ul+1e-12)
    reference=np.interp(1000,f,np.abs(ratio))
    ratio/=max(reference,1e-6)
    mask=(f>=75)&(f<=12000)&(q>0)&(q0>0)
    return f[mask],ratio[mask],dict(kind='network_analyzer',measurement='AirNorton, LD=200 kOhm + 470 pF / UL=unloaded',reference_hz=1000,raw_url=PICKUP_URL.format('LD'))

def bridge_matrix(cache,guitar=1):
    get_file(ZENODO_URL,cache)
    if hashlib.md5(cache.read_bytes()).hexdigest()!=ZENODO_MD5:raise ValueError('Zenodo source checksum mismatch')
    a=loadmat(cache,variable_names=['qualified_selected_impulses'])['qualified_selected_impulses']
    if a.shape!=(65,144000,6):raise ValueError(f'Wrong shape {a.shape}')
    d=np.asarray(a[guitar-1,:,:],dtype=np.float64)
    n=48000
    # Two independent hammer locations: first & third impacts. 2 accelerometers,
    # 3 nearfield mics. We use the first two mics as sparse spatial observations.
    # Source calibration: hammer 92.90mV/lbf, acceleration 10.64mV/g,
    # microphone 50mV/Pa; +/-10V ADC represented as +/-1.0.
    force_scale=(10000./92.90)*4.4482
    accel_scale=(10000./10.64)*9.80665
    mic_scale=(10000./50.)
    time=np.arange(n,dtype=float)
    # suppress late room response, but 100 ms is an approximation and room
    # reflections may still remain. Same gate for all channels preserves phase.
    gate=np.ones(n)
    gate[2400:6000]=.5+.5*np.cos(np.pi*np.arange(3600)/3599.)
    gate[6000:]=0.
    nfft=65536
    fs=np.fft.rfftfreq(nfft,1./FS)
    vv=np.zeros((len(fs),2,2),dtype=complex)
    pp=np.zeros((len(fs),2,2),dtype=complex)
    powers=[]
    for col,impact in enumerate((0,2)):
        x=d[impact*n:(impact+1)*n]
        F=np.fft.rfft(x[:,0]*force_scale*gate,nfft)
        p=np.abs(F)**2;reg=max(float(np.max(p))*1e-7,1e-18)
        powers.append(p)
        for s,ch in enumerate((2,1)): # bass then treble accelerometer
            A=np.fft.rfft(x[:,ch]*accel_scale*gate,nfft)
            vv[:,s,col]=(A*np.conj(F)/(p+reg))/(1j*np.maximum(2*np.pi*fs,1.))
        for c,ch in enumerate((3,4)):
            P=np.fft.rfft(x[:,ch]*mic_scale*gate,nfft)
            pp[:,c,col]=P*np.conj(F)/(p+reg)
    response=np.zeros((len(fs),2,2),dtype=complex)
    condition=[]
    for k in range(1,len(fs)):
        v=vv[k];p=pp[k]
        A=v@v.conj().T
        reg=0.035*float(np.trace(A).real)+1e-16
        response[k]=p@v.conj().T@np.linalg.inv(A+np.eye(2)*reg)
        condition.append(float(np.linalg.cond(v)))
    mask=(fs>=200)&(fs<=6500)&(np.minimum(powers[0],powers[1])>max(np.max(powers[0]),np.max(powers[1]))*1e-8)
    freq=fs[mask]
    resp=response[mask]
    med=float(np.median(np.abs(resp)))
    if not np.isfinite(med) or med<1e-10:raise ValueError('Complex transfer has insufficient support')
    resp/=med
    return freq,resp,dict(guitar=guitar,normalization_pa_per_ms=med,from_channels='mic 4,5 & bridge accelerometers 3,2 (1-indexed)',gating='0-50ms full; 50-125ms cosine fade; room contamination possible',phase_preserved=True,spatial_positions='two sparse nearfield microphone locations, not far-field spherical directivity')

def fit_real_fir(f,h,taps=NFIR,phase_grid=None):
    # Weighted real causal LS fit: its coefficients are real, but complex
    # response is fitted including measured phase, not just a magnitude EQ.
    f=np.asarray(f);h=np.asarray(h)
    freq_step=max(len(f)//850,1)
    f=f[::freq_step];h=h[::freq_step]
    k=np.arange(taps)
    M=np.exp(-2j*np.pi*np.outer(f,k)/FS)
    valid=np.isfinite(h.real)&np.isfinite(h.imag)
    M=M[valid];h=h[valid]
    if len(h)<taps*2:raise ValueError('Too few measured frequency points')
    train=np.arange(len(h))%5!=0;test=~train
    # Condition regularization so unmeasured/poorly excited bins do not create
    # extreme FIR taps. Tikhonov amplitude penalty + first difference penalty.
    Am=np.vstack([M[train].real,M[train].imag])
    b=np.concatenate([h[train].real,h[train].imag])
    ridge=0.01*np.sqrt(len(Am)/taps)
    dif=np.diff(np.eye(taps),axis=0)
    A=np.vstack([Am,ridge*np.eye(taps),ridge*.5*dif])
    target=np.concatenate([b,np.zeros(taps),np.zeros(taps-1)])
    coeff=np.linalg.lstsq(A,target,rcond=None)[0]
    # Keep measurable output stable even for extraordinarily ill-conditioned runs.
    max_abs=np.max(np.abs(coeff))
    if max_abs>6:coeff*=6/max_abs
    pred=M@coeff
    def err(mask):return float(np.linalg.norm(pred[mask]-h[mask])/max(np.linalg.norm(h[mask]),1e-12))
    # A complex frequency residual is phase-sensitive, unlike magnitude-only EQ.
    phase=np.angle(pred[test]*np.conj(h[test]))
    return coeff,dict(train_relative_complex_error=err(train),heldout_relative_complex_error=err(test),median_abs_phase_error_degrees=float(np.rad2deg(np.median(np.abs(phase)))),heldout_bins=int(np.count_nonzero(test)),train_bins=int(np.count_nonzero(train)),taps=int(taps))

def write_header(where,coil,paths):
    def array(a):return '{'+','.join(f'{float(v):.12g}' for v in a)+'}'
    lines=['// Generated by tools/GenerateMeasuredObserver15.py. Real transfer data from',
       '// CC0 pickupAnalyseR (loaded/unloaded ratio) and Mores Zenodo 4604577.',
       '// Relative phase from nearfield samples is preserved in fitted FIR.',
       '// NO measured spatial magnetic flux map or full 3D farfield directivity.',
       '#pragma once','#include <array>','namespace pmg {',f'inline constexpr int kMeasuredFIR15={len(coil)};',
       f'inline constexpr std::array<double,kMeasuredFIR15> kMeasuredCoil15={array(coil)};',
       'inline constexpr std::array<std::array<double,kMeasuredFIR15>,4> kMeasuredNylon15{{']
    for a in paths:lines.append('    '+array(a)+',')
    lines+=['}};','} // namespace pmg']
    where.parent.mkdir(parents=True,exist_ok=True)
    where.write_text('\n'.join(lines)+'\n')

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--cache',type=Path,default=Path('research/qualified_selected_impulses.mat'))
    ap.add_argument('--header',type=Path,default=Path('DSP/MeasuredObserver15Coeffs.h'))
    ap.add_argument('--report',type=Path,default=Path('research/observer15_measurement_fit.json'))
    ap.add_argument('--guitar',type=int,default=1)
    args=ap.parse_args()
    f,c,cm=pickup_data(); coil,errc=fit_real_fir(f,c)
    freq,hm,meta=bridge_matrix(args.cache,args.guitar)
    # 2 microphone ports x 2 bridge ports. All four complex responses fit.
    h=[];errors={}
    for m in range(2):
        for p in range(2):
            v,e=fit_real_fir(freq,hm[:,m,p]);h.append(v)
            errors[f'microphone{m+1}_bridge{p+1}']=e
    write_header(args.header,coil,h)
    report={'electric':dict(meta=cm,fit=errc,measurement_vs_physical_flux='A measured LOAD TRANSFER, not a pole field map'),
            'nylon':dict(meta=meta,fits=errors,measurement_vs_farfield='Sparse two-mic nearfield, NOT a fully measured angular farfield transfer'),
            'sources':['https://github.com/strayr/pickupAnalyseR-data', 'https://zenodo.org/records/4604577'],
            'fit_note':'heldout frequency bins from same instrument and capture; neither independent instruments nor blind-listening evidence',
            'guitar_parameters_measured':False,'phase_included':True,'audio_samples_distributed':False}
    args.report.parent.mkdir(parents=True,exist_ok=True)
    args.report.write_text(json.dumps(report,indent=2,ensure_ascii=False))
    print('COIL',json.dumps(errc),flush=True)
    for k,v in errors.items():print('NYLON',k,json.dumps(v),flush=True)
    print('GENERATED',args.header,args.report,flush=True)
if __name__=='__main__':main()
