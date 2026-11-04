"""Read native float WAV renders; write RMS-matched auditions and spectral/pitch evidence."""
import argparse, json, math, struct
from pathlib import Path
import numpy as np

def read(path):
    blob=path.read_bytes();assert blob[:4]==b'RIFF' and blob[8:12]==b'WAVE' and struct.unpack_from('<I',blob,4)[0]+8==len(blob), 'RIFF file must contain one complete WAV'; pos=12;fmt=None;data=None
    while pos+8<=len(blob):
        name,size=struct.unpack_from('<4sI',blob,pos);chunk=blob[pos+8:pos+8+size];pos+=8+size+(size&1)
        if name==b'fmt ':fmt=chunk
        if name==b'data':data=chunk
    tag,ch,sr=struct.unpack_from('<HHI',fmt)
    bits=struct.unpack_from('<H',fmt,14)[0]
    assert (tag==3 or tag==0xfffe) and bits==32
    return np.frombuffer(data,dtype='<f4').reshape(-1,ch).copy(),sr

def write(path,x,sr):
    data=np.asarray(x,dtype='<f4').tobytes();ch=x.shape[1]
    fmt=struct.pack('<HHIIHH',3,ch,sr,sr*ch*4,ch*4,32)
    path.write_bytes(b'RIFF'+struct.pack('<I',36+len(data))+b'WAVEfmt '+struct.pack('<I',16)+fmt+b'data'+struct.pack('<I',len(data))+data)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('folder');args=parser.parse_args();root=Path(args.folder);dest=root/'Matched';dest.mkdir(exist_ok=True)
    files=sorted(root.glob('Model*.wav'));raw={f.name:read(f) for f in files};rows=[]
    for demo in ('Melody','Chord','HeldE3','Comparison'):
        selected=[(f,x,sr) for f,(x,sr) in raw.items() if f.endswith('_'+demo+'.wav')]
        # Common RMS within each phrase, with at least ~2 dB sample-peak headroom.
        target=min(10**(-24/20),min(.8*float(np.sqrt(np.mean(x.astype(np.float64)**2)))/float(np.max(np.abs(x))) for _,x,_ in selected))
        for filename,x,sr in selected:
            rms=float(np.sqrt(np.mean(x.astype(np.float64)**2)));gain=target/rms
            write(dest/filename,x*gain,sr)
            mono=x[:,0].astype(np.float64);segment=mono[:int(sr*2)];nfft=1<<int(np.ceil(np.log2(len(segment))))
            spectrum=np.abs(np.fft.rfft(segment*np.hanning(len(segment)),nfft))**2;freq=np.fft.rfftfreq(nfft,1/sr)
            bands={f'{lo}-{hi}':float(np.sum(spectrum[(freq>=lo)&(freq<hi)])/max(np.sum(spectrum),1e-30)) for lo,hi in ((20,200),(200,1000),(1000,5000),(5000,20000))}
            pitch=None
            if demo=='HeldE3':
                # Normalized short-time autocorrelation, restricted around the expected E3 fundamental.
                a=mono[int(sr*.05):int(sr*.25)];a=a-a.mean();expected=sr/(440*2**((52-69)/12))
                lags=np.arange(int(expected*.90),int(expected*1.1)+1);corr=np.array([np.dot(a[:-lag],a[lag:])/max(np.linalg.norm(a[:-lag])*np.linalg.norm(a[lag:]),1e-30) for lag in lags]);i=int(np.argmax(corr));lag=float(lags[i])
                if 0<i<len(corr)-1:
                    left,mid,right=corr[i-1:i+2];lag+=float(.5*(left-right)/(left-2*mid+right))
                hz=sr/lag;pitch={'frequency_hz':hz,'cents_error':1200*math.log2(hz/(440*2**((52-69)/12))),'autocorrelation':float(corr[i]),'method':'normalized autocorrelation 50-250ms near E3; approximate'}
            row={'file':filename,'rate':sr,'frames':len(x),'channels':x.shape[1],'rms':rms,'peak':float(np.max(np.abs(x))),'finite':bool(np.isfinite(x).all()),'matched_gain_db':20*math.log10(gain),'matched_rms':target,'spectral_power_fraction':bands,'pitch':pitch}
            assert row['finite'] and 0<row['peak']<1
            rows.append(row)
    (root/'AUDIO_METRICS.json').write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf-8')
    comparison=[r for r in rows if r['file'].endswith('_Comparison.wav')]
    comparison_spread=20*math.log10(max(r['rms'] for r in comparison)/min(r['rms'] for r in comparison))
    assert comparison_spread<.1, f'Preset RMS spread {comparison_spread} dB exceeds 0.1 dB'
    (root/'COMPARISON_PRESETS.json').write_text(json.dumps({'reference':'48kHz six-note melody, velocity 100, 8 seconds including tail','rms_spread_db':comparison_spread,'output_db':[-11.1,-7,-5.4,-3],'models':['Original','Electric','Steel Acoustic','Classical Nylon'],'note':'Only the Comparison button loads these values. Raw defaults and Original are unchanged.'},ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(rows,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
