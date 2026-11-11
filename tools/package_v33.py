"""Package tested v3.3 without changing historical releases or manuals."""
from pathlib import Path
import hashlib,json,shutil,zipfile
root=Path(__file__).resolve().parents[1];dist=root/'dist';work=root/'work/v3.3'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
old=json.loads((work/'preserved-hashes.json').read_text(encoding='utf8'))
for name,h in old.items():assert sha(root/name)==h, 'Preserved artifact changed: '+name
required=['final33-compact','persist33-final','edge33-final','shutdown33-final','final32','persist32-final']
for folder in required:
 report=json.loads((work/folder/'dock-smoke.json').read_text(encoding='utf-8-sig'));assert report['failed']==0 and all(c['pass'] for c in report['checks']),folder
setup=root/'build/OBS_Spatial_FX_V3.3_Setup.exe'
for name in ['OBS_Spatial_FX_V3.3_Setup.exe','OBS_Spatial_FX_Setup.exe']:shutil.copy2(setup,dist/name)
def archive(name,files):
 with zipfile.ZipFile(dist/name,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for p in sorted(set(files)):z.write(p,p.relative_to(root))
 with zipfile.ZipFile(dist/name) as z:assert z.testzip() is None
 print(name,(dist/name).stat().st_size,flush=True)
sources=[root/n for n in ['CMakeLists.txt','README.md','BUILDING.md','LICENSE','.gitignore']]
for folder in ['src','data','installer','tests','docs']:
 sources.extend(p for p in (root/folder).rglob('*') if p.is_file() and p.suffix in ['.cpp','.hpp','.h','.inc','.py','.md','.txt','.effect','.ini','.ps1','.in'])
for name in ['build.cmd','test.cmd','find-obs.ps1','bootstrap.py','dependencies.json','prepare_sdk.py','launch-test-obs.ps1','launch-v3-qa.ps1','package_v33.py','verify_v33.py','run_v33_installed_qa.ps1']:sources.append(root/'tools'/name)
archive('OBS_Spatial_FX_V3.3_Source.zip',sources)
archive('OBS_Spatial_FX_V3.3_Symbols.zip',[root/'build/obs-spatial-fx.pdb',root/'build/OBS_Spatial_FX_V3.3_Setup.pdb'])
evidence=[]
for folder in required:
 evidence.extend(p for p in (work/folder).rglob('*') if p.is_file() and p.suffix in ['.json','.png','.log'])
for name in ['TEST_REPORT.md','progress.md','review.md','editor-report.md','final-build.log','ctest-final.log','upgrade-install.log','install-final.log','restore-v32-install.log','running-install-guard.log','upgrade-result.json','upgrade-config-baseline.json','install-guard.json','qa-harness-crash.txt','qa-cleanup.json','preserved-hashes.json']:
 evidence.append(work/name)
archive('OBS_Spatial_FX_V3.3_Evidence.zip',evidence)
manifest={p.name:sha(p) for p in dist.glob('OBS_Spatial_FX_V3.3*') if p.is_file()}
(dist/'V33_RELEASE.json').write_text(json.dumps({'version':'3.3.0','date':'2026-11-08','pluginSha256':sha(root/'build/obs-spatial-fx.dll'),'artifacts':manifest,'manualsUnchanged':True,'preserved':old},indent=2,ensure_ascii=False),encoding='utf8')
files=sorted(p for p in dist.rglob('*') if p.is_file() and p.name!='SHA256SUMS.txt')
(dist/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.relative_to(dist).as_posix()+'\n' for p in files),encoding='utf8')
print('Installer SHA256',sha(setup),flush=True)
