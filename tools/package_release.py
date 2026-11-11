"""Package v2 while preserving the exact v1 release artifacts."""
from pathlib import Path
import hashlib,shutil,zipfile
root=Path(__file__).resolve().parents[1];dist=root/'dist';dist.mkdir(exist_ok=True)
v1={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in dist.glob('OBS_Spatial_FX_V1.0*')}
for name in ['OBS_Spatial_FX_V2.0_Setup.exe','OBS_Spatial_FX_Setup.exe']:shutil.copy2(root/'build/OBS_Spatial_FX_V2.0_Setup.exe',dist/name)
for name in ['README.md','LICENSE']:shutil.copy2(root/name,dist/name)
for p in (root/'docs').rglob('*.md'):
 target=dist/p.relative_to(root);target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target)
source=[root/p for p in ['CMakeLists.txt','README.md','LICENSE','.gitignore']]
for directory in ['src','data','tests','docs','installer']:
 source += [p for p in (root/directory).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix in ['.cpp','.hpp','.h','.inc','.py','.md','.txt','.effect','.ini','.ps1','.in']]
toolnames=['build.cmd','test.cmd','find-obs.ps1','bootstrap.py','dependencies.json','prepare_sdk.py','launch-test-obs.ps1','test-upgrade-v2.ps1','test-installer-v2.ps1','test-installed-normal-v2.ps1','test-installed-portable-v2.ps1','test-installed-migration-v2.ps1','prepare_v1_fixture.py','write_v2_report.py','inspect-binaries.cmd','cleanup_tests.py','package_release.py','native-debugger.cpp','build-debugger.cmd']
source += [root/'tools'/n for n in toolnames]
def archive(name,files):
 with zipfile.ZipFile(dist/name,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for p in sorted(set(files)):
   if not p.is_file():raise FileNotFoundError(p)
   z.write(p,p.relative_to(root))
 with zipfile.ZipFile(dist/name) as z:assert z.testzip() is None
 print(name,len(files),'files',(dist/name).stat().st_size,'bytes')
archive('OBS_Spatial_FX_V2.0_Source.zip',source)
keep=['native-private-final','restart-private-final','stress-private-final','association-private-final','gpu-private-retry','gpu-private-final','installed-migration-first','installed-migration-restart','phase6-installed-normal','release-installed-performance','baseline-stress','debug-stress','debug2-stress','stress-final','upgrade-attempt1','area-direct-red']
evidence=[]
for name in keep:
 evidence += [p for p in (root/'logs/v2'/name).rglob('*') if p.is_file() and p.suffix in ['.json','.log','.txt','.png']]
# Exclude collection/profile backups and binaries; do not distribute personal OBS configuration.
for p in (root/'logs/v2').glob('*'):
 if p.is_file() and p.suffix in ['.log','.json','.txt'] and not any(x in p.name for x in ['collection','fixture','config-backup']):evidence.append(p)
archive('OBS_Spatial_FX_V2.0_Evidence.zip',evidence)
archive('OBS_Spatial_FX_V2.0_Symbols.zip',[root/'build/obs-spatial-fx.pdb',root/'build/OBS_Spatial_FX_V2.0_Setup.pdb'])
for name,digest in v1.items():assert hashlib.sha256((dist/name).read_bytes()).hexdigest()==digest,'v1 artifact changed: '+name
files=[p for p in dist.rglob('*') if p.is_file() and p.name!='SHA256SUMS.txt']
(dist/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(dist).as_posix()+'\n' for p in sorted(files)),encoding='ascii')
assert (dist/'OBS_Spatial_FX_V2.0_Setup.exe').read_bytes()==(root/'build/OBS_Spatial_FX_V2.0_Setup.exe').read_bytes()
print('v1 preserved',v1)
print('v2 installer SHA256',hashlib.sha256((dist/'OBS_Spatial_FX_V2.0_Setup.exe').read_bytes()).hexdigest())
