"""Package v3.3 UI refinement using evidence from this exact build."""
from pathlib import Path
import hashlib,json,shutil,zipfile
r=Path(__file__).resolve().parents[1];w=r/'work/v3.3-ui-20261110';d=r/'dist'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
folders=['ui33','ui32','regression33','regression32','upgrade-load33','upgrade-load32','install-guard33']
binary=sha(r/'build/obs-spatial-fx.dll')
for folder in folders:
 a=read(w/folder/'dock-smoke.json');paths=read(w/folder/'installed-path.json')
 assert a['failed']==0 and all(c['pass'] for c in a['checks']),folder
 assert paths['InstalledHash'].lower()==binary and len(paths['LoadedPaths'])==1,folder
for name,h in read(w/'manual-hashes.json').items():assert sha(Path(name)).upper()==h.upper(),name
for name,h in read(r/'work/v3.3/preserved-hashes.json').items():assert sha(r/name)==h,name
upgrade=read(w/'upgrade-result.json');assert upgrade['ExitCode']==0 and upgrade['BuildMatches'] and upgrade['ConfigUnchanged']
assert read(w/'install-guard.json')['ExitCode']==10 and read(w/'install-guard.json')['HashUnchanged']
assert '100% tests passed' in (w/'ctest.log').read_text(encoding='utf-8-sig')
setup=r/'build/OBS_Spatial_FX_V3.3_Setup.exe'
for name in ['OBS_Spatial_FX_V3.3_Setup.exe','OBS_Spatial_FX_Setup.exe']:shutil.copy2(setup,d/name)
def archive(name,files):
 with zipfile.ZipFile(d/name,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for p in sorted(set(files)):z.write(p,p.relative_to(r))
 with zipfile.ZipFile(d/name) as z:assert z.testzip() is None
 print(name,(d/name).stat().st_size,flush=True)
sources=[r/n for n in ['CMakeLists.txt','README.md','BUILDING.md','LICENSE','.gitignore']]
for folder in ['src','data','installer','tests','docs']:
 sources.extend(p for p in (r/folder).rglob('*') if p.is_file() and p.suffix in ['.cpp','.hpp','.h','.inc','.py','.md','.txt','.effect','.ini','.ps1','.in'])
for name in ['build.cmd','test.cmd','find-obs.ps1','bootstrap.py','dependencies.json','prepare_sdk.py','launch-test-obs.ps1','launch-v3-qa.ps1','package_v33.py','verify_v33.py','package_v33_fix.py','verify_v33_fix.py','package_v33_ui.py','verify_v33_ui.py','run_v33_installed_qa.ps1']:sources.append(r/'tools'/name)
archive('OBS_Spatial_FX_V3.3_Source.zip',sources)
archive('OBS_Spatial_FX_V3.3_Symbols.zip',[r/'build/obs-spatial-fx.pdb',r/'build/OBS_Spatial_FX_V3.3_Setup.pdb'])
evidence=[]
for folder in folders+['red33','pass33','final33','verified33']:
 evidence.extend(p for p in (w/folder).rglob('*') if p.is_file() and p.suffix in ['.json','.png','.log'])
evidence.extend(p for p in w.iterdir() if p.is_file() and p.suffix in ['.md','.log','.json','.ps1','.py'] and p.name!='final-package-audit.json')
assert (w/'TEST_REPORT.md') in evidence
archive('OBS_Spatial_FX_V3.3_Evidence.zip',evidence)
artifacts={p.name:sha(p) for p in d.glob('OBS_Spatial_FX_V3.3*') if p.is_file()}
manifest={'version':'3.3.0','releaseId':'3.3-ui-20261110','date':'2026-11-10','pluginSha256':binary,'artifacts':artifacts,'manualsUnchanged':True,'runtimeReports':folders,'guiChecks':sum(len(read(w/f/'dock-smoke.json')['checks']) for f in folders),'gpuTests':'NOT RUN (UI-only refinement; renderer unchanged)'}
(d/'V33_RELEASE.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=False),encoding='utf8')
files=sorted(p for p in d.rglob('*') if p.is_file() and p.name!='SHA256SUMS.txt')
(d/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.relative_to(d).as_posix()+'\n' for p in files),encoding='utf8')
print('Setup SHA256',sha(setup),flush=True)
