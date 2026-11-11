"""Verify concrete v3.3 build, archives, preservation and installed OBS evidence."""
from pathlib import Path
import hashlib,json,struct,zipfile
r=Path(__file__).resolve().parents[1];w=r/'work/v3.3';d=r/'dist';checks=[]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
def check(ok,name):
 checks.append({'name':name,'pass':bool(ok)});print('PASS' if ok else 'FAIL',name)
m=read(d/'V33_RELEASE.json');binary=sha(r/'build/obs-spatial-fx.dll');check(m['version']=='3.3.0' and m['pluginSha256']==binary,'v3.3.0 exact build manifest')
for name,h in m['artifacts'].items():check(sha(d/name)==h,'SHA256 '+name)
check(all(sha(r/name)==h for name,h in m['preserved'].items()),'all previous versioned releases and docs unchanged')
exe=(d/'OBS_Spatial_FX_V3.3_Setup.exe').read_bytes();pe=struct.unpack_from('<I',exe,0x3c)[0]
check(struct.unpack_from('<H',exe,pe+4)[0]==0x8664 and struct.unpack_from('<H',exe,pe+24+68)[0]==2,'real Windows x64 GUI installer')
check(exe==(d/'OBS_Spatial_FX_Setup.exe').read_bytes()==(r/'build/OBS_Spatial_FX_V3.3_Setup.exe').read_bytes(),'published installers equal tested build')
for kind in ['Source','Evidence','Symbols']:
 with zipfile.ZipFile(d/f'OBS_Spatial_FX_V3.3_{kind}.zip') as z:
  check(z.testzip() is None,kind+' archive CRC')
  if kind=='Source':check(all((r/n).read_bytes()==z.read(n) for n in z.namelist()),'source archive equals workspace')
base=Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx')
check(all(sha(base/n)==binary for n in ['obs-spatial-fx.dll','bin/64bit/obs-spatial-fx.dll','portable33/obs-spatial-fx/obs-spatial-fx.dll']),'three installed OBS32/33 payload copies match final DLL')
check({p.name for p in base.rglob('*.dll')}=={'obs-spatial-fx.dll'},'installer contains own plugin only; no Qt/OBS/QA libraries')
for folder in ['final33-compact','persist33-final','edge33-final','shutdown33-final','final32','persist32-final']:
 a=read(w/folder/'dock-smoke.json');check(a['failed']==0 and all(c['pass'] for c in a['checks']),f'actual OBS QA {folder}: {len(a["checks"])} checks')
for major,folder in [(33,'final33-compact'),(32,'final32')]:
 a=read(w/folder/'installed-path.json');check(len(a['LoadedPaths'])==1 and a['InstalledHash'].lower()==binary and sha(Path(a['LoadedPaths'][0]))==binary,f'OBS{major} loaded unique installed final plugin')
a=read(w/'upgrade-result.json');check(a['ExitCode']==0 and a['Version']=='3.3.0' and a['OldHash']!=binary and a['NewHash']==binary and a['ConfigUnchanged'],'real v3.2 upgrade preserves OBS configurations')
a=read(w/'install-guard.json');check(a['ExitCode']==10 and a['HashUnchanged'],'installer refuses overwrite while OBS running')
check('100% tests passed' in (w/'ctest-final.log').read_text(encoding='utf-8-sig'),'five core CTest groups pass')
for line in (d/'SHA256SUMS.txt').read_text(encoding='utf8').splitlines():
 h,n=line.split('  ',1);assert sha(d/n)==h,n
check(True,'whole dist SHA256 manifest matches')
(w/'final-package-audit.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['pass'] for c in checks)},indent=2),encoding='utf8')
raise SystemExit(any(not c['pass'] for c in checks))
