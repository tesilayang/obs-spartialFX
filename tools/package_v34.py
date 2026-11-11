"""Package only after verification of the exact installed v3.4 binary."""
from pathlib import Path
import hashlib,json,shutil,struct,zipfile
r=Path(__file__).resolve().parents[1];w=r/'work/v3.4-development';d=r/'dist';d.mkdir(exist_ok=True)
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
checks=[]
def check(ok,name):
 checks.append({'name':name,'pass':bool(ok)})
 print(('PASS ' if ok else 'FAIL ')+name,flush=True)
 assert ok,name
binary=sha(r/'build/obs-spatial-fx.dll');setup=r/'build/OBS_Spatial_FX_V3.4_Setup.exe'
ui=['installed-ui32','installed-persist32','installed-ui33','installed-persist33']
for folder in ui:
 result=read(w/folder/'dock-smoke.json');paths=read(w/folder/'installed-path.json')
 check(result['failed']==0 and all(x['pass'] for x in result['checks']),folder+' real UI checks')
 check(paths['InstalledHash'].lower()==binary and len(paths['LoadedPaths'])==1 and 'ProgramData' in paths['LoadedPaths'][0],folder+' exact installed binary')
for folder in ['gpu32-final','gpu33-verified']:
 result=read(w/folder/'render-tests.json')
 check(result['failed']==0 and len(result['effects'])==92 and all(x['gpuPass'] and x['defaultVisible'] for x in result['effects']),folder+' all92 GPU effects and nonempty defaults')
 check(sum(x['variants'] for x in result['effects'])==224,folder+'224 default/mode variants')
 perf=read(w/folder/'performance.json');samples=perf['samples']
 check(perf['warmMetrics']['allocations']==perf['afterMetrics']['allocations'] and perf['afterMetrics']['failures']==0,folder+'30second resource plateau')
 check(min(x['activeFps'] for x in samples)>59 and samples[-1]['renderSkippedFrames']==perf['before']['renderSkippedFrames'],folder+'1080p60 steady run without newly skipped rendering frames')
for folder in ['gpu32-final','gpu33-final']:
 result=read(w/folder/'coordinates/coordinate-tests.json')
 check(result['failed']==0 and len(result['checks'])==8,folder+' coordinate regressions')
installer=read(w/'installer/installer-tests.json');check(len(installer)==18 and all(x['pass'] for x in installer),'upgrade rollback uninstall and configuration18 checks')
guard=read(w/'install-running-guard.json');check(guard['ExitCode']==10 and guard['HashUnchanged'],'running OBS blocks installation with no writes')
check('100% tests passed out of 7' in (w/'ctest-final.log').read_text(encoding='utf-8-sig'),'CTest7/7')
for name,value in read(w/'historical-hashes.json').items():check(sha(r/name)==value,'preserved '+name)
for name,value in read(r/'work/v3.3-ui-20261110/manual-hashes.json').items():check(sha(Path(name)).upper()==value.upper(),'unchanged old document '+Path(name).name)
mapping=read(r/'docs/v3.4/V34_IMPLEMENTATION_MAP.json')
check(len(mapping['effects'])==92 and len(set(i for x in mapping['effects'] for i in x['source_ids']))==113,'confirmed113 selections mapped to92 suboptions')
base=Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx')
for p in ['obs-spatial-fx.dll','bin/64bit/obs-spatial-fx.dll','portable33/obs-spatial-fx/obs-spatial-fx.dll']:check(sha(base/p)==binary,'installed layout '+p)
check(sha(base/'Uninstall.exe')==sha(setup),'final installed setup equals deliverable')
for major in [32,33]:
 cfg=read(r/f'runtime/obs{major}/config/obs-studio/plugin_config/obs-websocket/config.json')
 check(not cfg['server_enabled'],f'OBS{major} temporary test server disabled')
check(not list(base.rglob('Qt*.dll')) and not list(base.rglob('*qa-driver*')),'no Qt or QA DLL packaged')
data=setup.read_bytes();pe=struct.unpack_from('<I',data,0x3c)[0]
check(data[:2]==b'MZ' and struct.unpack_from('<H',data,pe+4)[0]==0x8664 and struct.unpack_from('<H',data,pe+24+68)[0]==2,'real Windows x64 GUI installer')
for name in ['OBS_Spatial_FX_V3.4_Setup.exe','OBS_Spatial_FX_Setup.exe']:shutil.copy2(setup,d/name)
def archive(name,files):
 with zipfile.ZipFile(d/name,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for p in sorted(set(files)):z.write(p,p.relative_to(r))
 with zipfile.ZipFile(d/name) as z:check(z.testzip() is None,name+' CRC')
 print(name,(d/name).stat().st_size,flush=True)
sources=[r/n for n in ['CMakeLists.txt','README.md','BUILDING.md','LICENSE','.gitignore']]
extensions={'.cpp','.hpp','.h','.inc','.body','.effect','.py','.md','.txt','.ps1','.cmd','.json','.in','.ini','.bat'}
for folder in ['src','data','installer','tests','docs']:
 sources.extend(p for p in (r/folder).rglob('*') if p.is_file() and p.suffix in extensions and '__pycache__' not in p.parts)
sources.extend(p for p in (r/'tools').iterdir() if p.is_file() and p.suffix in extensions)
archive('OBS_Spatial_FX_V3.4_Source.zip',sources)
with zipfile.ZipFile(d/'OBS_Spatial_FX_V3.4_Source.zip') as z:check(all(z.read(p.relative_to(r).as_posix())==p.read_bytes() for p in sources),'source archive equals workspace')
archive('OBS_Spatial_FX_V3.4_Symbols.zip',[r/'build/obs-spatial-fx.pdb',r/'build/OBS_Spatial_FX_V3.4_Setup.pdb'])
evidence=[p for p in w.iterdir() if p.is_file() and p.suffix in {'.json','.log','.md','.py'}]
for folder in ui+['gpu32-final','gpu33-verified','gpu33-final/coordinates','installer','shader-isolation']:
 evidence.extend(p for p in (w/folder).rglob('*') if p.is_file() and 'installed-before' not in p.parts and p.suffix in {'.json','.png','.log','.md'})
archive('OBS_Spatial_FX_V3.4_Evidence.zip',evidence)
artifacts={p.name:sha(p) for p in d.glob('OBS_Spatial_FX_V3.4_*') if p.is_file()}
manifest={'version':'3.4.0','date':'2026-11-10','pluginSha256':binary,'artifacts':artifacts,'groups':13,'suboptions':92,'sourceSelections':113,'manualsUnchanged':True,'guiChecks':sum(len(read(w/x/'dock-smoke.json')['checks']) for x in ui),'gpuVariantsPerOBSVersion':224,'installerChecks':18,'limitations':['No NVIDIA hardware run','No OBS34+ runtime available','30second representative FX chain performance run; not an hours-long or all92simultaneous stress test','Pixel effects are bounded by the source rectangle; motion blur uses known layer/camera motion, not video optical flow']}
(d/'V34_RELEASE.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
(w/'final-package-audit.json').write_text(json.dumps({'checks':checks,'failed':sum(not x['pass'] for x in checks),'pluginSha256':binary},ensure_ascii=False,indent=2),encoding='utf-8')
files=sorted(p for p in d.rglob('*') if p.is_file() and p.name!='SHA256SUMS.txt')
(d/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.relative_to(d).as_posix()+'\n' for p in files),encoding='utf-8')
print('Setup SHA256',sha(setup),flush=True)
