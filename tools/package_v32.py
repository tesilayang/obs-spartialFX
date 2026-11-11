"""Package the authorized same-version v3.2 patch; preserve V1 through V3.1."""
from pathlib import Path
import hashlib,json,shutil,zipfile
root=Path(__file__).resolve().parents[1]
dist=root/'dist';dist.mkdir(exist_ok=True)
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
old={p.name:digest(p) for p in dist.glob('OBS_Spatial_FX_V*') if p.is_file() and not p.name.startswith('OBS_Spatial_FX_V3.2')}
setup=root/'build/OBS_Spatial_FX_V3.2_Setup.exe'
for name in ['OBS_Spatial_FX_V3.2_Setup.exe','OBS_Spatial_FX_Setup.exe']:shutil.copy2(setup,dist/name)
for name in ['README.md','BUILDING.md','LICENSE']:shutil.copy2(root/name,dist/name)
for p in (root/'docs/v3.2').glob('*.md'):
    target=dist/'docs/v3.2'/p.name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,target)
source=[root/p for p in ['CMakeLists.txt','README.md','BUILDING.md','LICENSE','.gitignore']]
for directory in ['src','data','installer','tests','docs']:
    source.extend(p for p in (root/directory).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix in ['.cpp','.hpp','.h','.inc','.py','.md','.txt','.effect','.ini','.ps1','.in'])
for name in ['build.cmd','test.cmd','find-obs.ps1','bootstrap.py','dependencies.json','prepare_sdk.py','launch-test-obs.ps1','launch-v3-qa.ps1','package_v3.py','verify_v3.py','package_v31.py','verify_v31.py','package_v32.py','verify_v32.py','run_v32_installed_qa.ps1']:
    source.append(root/'tools'/name)
# Include fixture generators and independent review harnesses for reproducible audio evidence.
source.extend(p for p in (root/'work/v3.2/audio').glob('*') if p.is_file() and p.suffix in ['.py','.cmd','.cpp'])
def archive(name,files):
    with zipfile.ZipFile(dist/name,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for p in sorted(set(files)):
            if not p.is_file():raise FileNotFoundError(p)
            z.write(p,p.relative_to(root))
    with zipfile.ZipFile(dist/name) as z:assert z.testzip() is None
    print(name,len(set(files)),'files',(dist/name).stat().st_size,'bytes',flush=True)
archive('OBS_Spatial_FX_V3.2_Source.zip',source)
evidence=[]
for p in (root/'work/v3.2-fix').rglob('*'):
    if not p.is_file() or p.suffix not in ['.json','.log','.txt','.png','.md']:continue
    relative=p.relative_to(root/'work/v3.2-fix')
    if any(part in ['baseline','original-release','installed-original','obs33-localpayload','obs32-localpayload','cleanup-backup'] for part in relative.parts):continue
    if p.name in ['native-command.json','native-answer.json','fixture-pid.txt','user-config-before.json','websocket33-backup.json','websocket32-backup.json','tooltip-desktop.png','final-package-audit.json']:continue
    evidence.append(p)
archive('OBS_Spatial_FX_V3.2_Evidence.zip',evidence)
archive('OBS_Spatial_FX_V3.2_Symbols.zip',[root/'build/obs-spatial-fx.pdb',root/'build/OBS_Spatial_FX_V3.2_Setup.pdb'])
assert all(digest(dist/name)==value for name,value in old.items()),'Historical artifact modified'
manifest={p.name:digest(p) for p in dist.glob('OBS_Spatial_FX_V3.2*') if p.is_file()}
(dist/'V32_RELEASE.json').write_text(json.dumps({'version':'3.2.0','revision':'2026-11-01 timeline and curve fixes','pluginSha256':digest(root/'build/obs-spatial-fx.dll'),'artifacts':manifest,'historicalArtifactsPreserved':old},indent=2),encoding='utf-8')
files=sorted(p for p in dist.rglob('*') if p.is_file() and p.name!='SHA256SUMS.txt')
(dist/'SHA256SUMS.txt').write_text(chr(10).join(digest(p)+'  '+p.relative_to(dist).as_posix() for p in files)+chr(10),encoding='ascii')
print('Installer SHA256',digest(setup),flush=True)
