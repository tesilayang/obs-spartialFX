"""Audit final v3.3 UI release against build, installed payload and runtime logs."""
from pathlib import Path
import hashlib,json,struct,zipfile
r=Path(__file__).resolve().parents[1];w=r/'work/v3.3-ui-20261110';d=r/'dist';checks=[]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
def check(ok,name):checks.append({'name':name,'pass':bool(ok)});print('PASS' if ok else 'FAIL',name)
m=read(d/'V33_RELEASE.json');binary=sha(r/'build/obs-spatial-fx.dll')
check(m['version']=='3.3.0' and m['releaseId']=='3.3-ui-20261110' and m['pluginSha256']==binary,'manifest identifies tested v3.3 build')
for name,h in m['artifacts'].items():check(sha(d/name)==h,'SHA256 '+name)
check(all(sha(Path(n)).upper()==h.upper() for n,h in read(w/'manual-hashes.json').items()),'user manuals unchanged')
check(all(sha(r/n)==h for n,h in read(r/'work/v3.3/preserved-hashes.json').items()),'historical docs and releases unchanged')
exe=(d/'OBS_Spatial_FX_V3.3_Setup.exe').read_bytes();pe=struct.unpack_from('<I',exe,0x3c)[0]
check(struct.unpack_from('<H',exe,pe+4)[0]==0x8664 and struct.unpack_from('<H',exe,pe+24+68)[0]==2,'real Windows x64 GUI installer')
check(exe==(d/'OBS_Spatial_FX_Setup.exe').read_bytes()==(r/'build/OBS_Spatial_FX_V3.3_Setup.exe').read_bytes(),'both published installers equal tested build')
for kind in ['Source','Evidence','Symbols']:
 with zipfile.ZipFile(d/f'OBS_Spatial_FX_V3.3_{kind}.zip') as z:
  check(z.testzip() is None,kind+' archive CRC')
  if kind=='Source':check(all((r/n).read_bytes()==z.read(n) for n in z.namelist()),'source archive matches workspace')
base=Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx')
check(all(sha(base/n)==binary for n in ['obs-spatial-fx.dll','bin/64bit/obs-spatial-fx.dll','portable33/obs-spatial-fx/obs-spatial-fx.dll']),'installed OBS32/33 payloads match final DLL')
check({p.name for p in base.rglob('*.dll')}=={'obs-spatial-fx.dll'},'payload contains no bundled Qt OBS or QA DLLs')
for folder in m['runtimeReports']:
 a=read(w/folder/'dock-smoke.json');paths=read(w/folder/'installed-path.json')
 check(a['failed']==0 and all(c['pass'] for c in a['checks']),f'OBS {folder}: {len(a["checks"])} checks')
 check(paths['InstalledHash'].lower()==binary and len(paths['LoadedPaths'])==1 and sha(Path(paths['LoadedPaths'][0]))==binary,folder+' uniquely loaded final installed DLL')
a=read(w/'upgrade-result.json');check(a['ExitCode']==0 and a['Version']=='3.3.0' and a['OldHash'].lower()!=binary and a['NewHash'].lower()==binary and a['ConfigUnchanged'],'v3.3 upgrade preserves configuration')
a=read(w/'install-guard.json');check(a['ExitCode']==10 and a['HashUnchanged'],'running OBS prevents file replacement')
check('100% tests passed' in (w/'ctest.log').read_text(encoding='utf-8-sig'),'five CTest groups pass')
check(all(c['Removed'] for c in read(w/'qa-cleanup.json')),'isolated QA drivers removed')
check((w/'previous-release/OBS_Spatial_FX_V3.3_Setup.exe').is_file(),'previous v3.3 installer retained')
for line in (d/'SHA256SUMS.txt').read_text(encoding='utf8').splitlines():
 h,n=line.split('  ',1);assert sha(d/n)==h,n
check(True,'dist checksums match')
(w/'final-package-audit.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['pass'] for c in checks)},indent=2),encoding='utf8')
raise SystemExit(any(not c['pass'] for c in checks))
