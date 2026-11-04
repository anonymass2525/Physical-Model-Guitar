"""Package only this product; verify native architecture, evidence, CRC and per-entry SHA-256."""
import hashlib,json,re,struct,subprocess,sys,zipfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def sha(data):return hashlib.sha256(data).hexdigest()
def load_module_info(p):return json.loads(re.sub(r',\s*([}\]])',r'\1',p.read_text(encoding='utf-8-sig')))
def main():
    destination=Path(sys.argv[1]);assert not destination.exists(),'Keep previous packages; choose a new filename'
    for name in ('ctest-final','render-final','audio-analysis-final','pluginval-final','final-build-PhysicalModelGuitar_VST3','final-tools-PMGPluginTests','final-tools-PMGRender'):
        assert (root/'logs'/f'{name}.exit.txt').read_text().strip()=='0',f'{name} did not succeed'
    assert 'SUCCESS' in (root/'logs'/'pluginval-final.stdout.log').read_text(encoding='utf-8-sig',errors='replace')
    assert not subprocess.check_output(['git','status','--porcelain'],cwd=root).strip(),'Commit the verified source before packaging'
    bundle=root/'build-upgrade'/'PhysicalModelGuitar_artefacts'/'Release'/'VST3'/'Physical Model Guitar.vst3'
    module=bundle/'Contents'/'x86_64-win'/'Physical Model Guitar.vst3';data=module.read_bytes();offset=struct.unpack_from('<I',data,60)[0]
    assert data[:2]==b'MZ' and data[offset:offset+4]==b'PE\0\0' and struct.unpack_from('<H',data,offset+4)[0]==0x8664
    info=load_module_info(bundle/'Contents'/'Resources'/'moduleinfo.json')
    old=load_module_info(root/'build-baseline'/'PhysicalModelGuitar_artefacts'/'Release'/'VST3'/'Physical Model Guitar.vst3'/'Contents'/'Resources'/'moduleinfo.json')
    assert info['Version']=='0.2.0' and [c['CID'] for c in info['Classes']]==[c['CID'] for c in old['Classes']]
    audio=root/'artifacts'/'audio-final';rows=json.loads((audio/'AUDIO_METRICS.json').read_text(encoding='utf-8'))
    assert len(rows)==16 and all(r['finite'] and r['frames']==384000 and r['channels']==2 and r['rate']==48000 and 0<r['peak']<1 for r in rows)
    # Matched copies use a single documented gain, no limiter, normalization dynamics or shift.
    sys.path.insert(0,str(root/'tools'));from AnalyseAudio import read
    import numpy as np
    for row in rows:
        a,sr=read(audio/row['file']);b,br=read(audio/'Matched'/row['file']);gain=row['matched_rms']/row['rms']
        assert sr==br and a.shape==b.shape and np.array_equal(b,a*gain) and np.isfinite(b).all() and np.max(np.abs(b))<=.800001
    branch=subprocess.check_output(['git','branch','--show-current'],cwd=root,text=True).strip()
    commit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip()
    files={}
    def add(path,name):files[name]=path.read_bytes()
    for item in subprocess.check_output(['git','ls-files','-z'],cwd=root).decode().split('\0'):
        if item:add(root/item,'Source/'+item)
    for file in bundle.rglob('*'):
        if file.is_file():add(file,'VST3/Physical Model Guitar.vst3/'+file.relative_to(bundle).as_posix())
    for file in audio.rglob('*'):
        if file.is_file():add(file,'Audio/'+file.relative_to(audio).as_posix())
    for file in (root/'artifacts'/'gui').glob('*.png'):add(file,'GUI/'+file.name)
    for file in (root/'logs').glob('*'):
        if file.is_file() and not file.name.startswith('package'):add(file,'Logs/'+file.name)
    for name in ('PMGDSPTests.exe','PMGPluginTests.exe','PMGRender.exe'):add(root/'build-upgrade'/'Release'/name,'TestTools/'+name)
    for file in root.glob('*JA.md'):add(file,'Reports/'+file.name)
    for file in (root/'third_party').glob('*'):add(file,'ThirdParty/'+file.name)
    bundle_file=root/'artifacts'/'PhysicalModelGuitar.git.bundle'
    subprocess.run(['git','bundle','create',str(bundle_file),'--all'],cwd=root,check=True)
    add(bundle_file,'SourceHistory/PhysicalModelGuitar.git.bundle')
    build={'product':'Physical Model Guitar','version':'0.2.0','branch':branch,'commit':commit,'architecture':'AMD64 / Windows x64','juce':'9.0.4','module_sha256':sha(data),'class_ids':[c['CID'] for c in info['Classes']],'ctest':'7/7 PASS','pluginval':'strictness 5, GUI included, exit 0 SUCCESS','studio_one':'NOT VERIFIED','human_listening':'NOT VERIFIED','original_reference':'All six measured conditions are bit-identical to captured source prototype'}
    files['BUILD_INFO.json']=json.dumps(build,ensure_ascii=False,indent=2).encode()
    files['SHA256SUMS.txt']=''.join(f'{sha(value)}  {name}\n' for name,value in sorted(files.items())).encode()
    destination.parent.mkdir(parents=True,exist_ok=True)
    with zipfile.ZipFile(destination,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as archive:
        for name,value in sorted(files.items()):archive.writestr(name,value)
    with zipfile.ZipFile(destination) as archive:
        assert archive.testzip() is None
        for name,value in files.items():assert sha(archive.read(name))==sha(value),name
    receipt={**build,'zip':str(destination),'bytes':destination.stat().st_size,'entries':len(files),'zip_sha256':sha(destination.read_bytes()),'crc':'PASS','entry_sha256':'PASS'}
    Path(str(destination)+'.sha256.txt').write_text(receipt['zip_sha256']+'  '+destination.name+'\n',encoding='utf-8')
    Path(str(destination)+'.validation.json').write_text(json.dumps(receipt,ensure_ascii=False,indent=2),encoding='utf-8')
    print(json.dumps(receipt,ensure_ascii=False,indent=2))
if __name__=='__main__':main()
