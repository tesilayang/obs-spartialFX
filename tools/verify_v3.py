from pathlib import Path
import hashlib,json,struct,zipfile
root=Path(__file__).resolve().parents[1];dist=root/'dist'
checks=[]
def check(ok,name):
    checks.append({'name':name,'pass':bool(ok)});print('PASS' if ok else 'FAIL',name)
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
manifest=json.loads((dist/'V3_RELEASE.json').read_text())
for name,value in manifest['artifacts'].items():check(digest(dist/name)==value,'release SHA256 '+name)
for name,value in manifest['historicalArtifactsPreserved'].items():check(digest(dist/name)==value,'V1/V2 preserved '+name)
for line in (dist/'SHA256SUMS.txt').read_text().splitlines():
    value,name=line.split('  ',1);assert digest(dist/name)==value,name
check(True,'whole dist SHA256 manifest')
setup=(dist/'OBS_Spatial_FX_V3.0_Setup.exe').read_bytes();pe=struct.unpack_from('<I',setup,0x3c)[0]
check(struct.unpack_from('<H',setup,pe+4)[0]==0x8664,'real Windows x64 PE')
check(struct.unpack_from('<H',setup,pe+24+68)[0]==2,'GUI installer subsystem')
check(setup==(dist/'OBS_Spatial_FX_Setup.exe').read_bytes(),'latest unversioned alias equals v3')
check(setup==(root/'build/OBS_Spatial_FX_V3.0_Setup.exe').read_bytes(),'published installer equals verified build')
with zipfile.ZipFile(dist/'OBS_Spatial_FX_V3.0_Source.zip') as z:
    names=z.namelist();check(z.testzip() is None,'source CRC');check(all(n in names for n in ['src/widgets.cpp','src/dock_v3.cpp','tests/v3_qa.inc','installer/setup.cpp','BUILDING.md']),'full source deliverable');check(not any(n.lower().endswith('.dll') for n in names),'source has no bundled OBS/Qt DLL')
for kind in ['Evidence','Symbols']:
    with zipfile.ZipFile(dist/('OBS_Spatial_FX_V3.0_'+kind+'.zip')) as z:check(z.testzip() is None,kind+' archive CRC')
base=Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx')
check((base/'obs-spatial-fx.dll').read_bytes()==(root/'build/obs-spatial-fx.dll').read_bytes(),'installed final DLL equals build')
names=[p.name for p in base.rglob('*.dll')]
check(set(names)=={'obs-spatial-fx.dll'},'installed payload contains only our DLL; no Qt/OBS/QA DLL')
check(all(not c.exists() for c in [base/'spatial-qa-driver.dll',base/'bin/64bit/spatial-qa-driver.dll']),'QA driver excluded from system install')
report={'checks':checks,'failed':sum(not c['pass'] for c in checks),'installerSha256':digest(dist/'OBS_Spatial_FX_V3.0_Setup.exe')}
(root/'work/v3/final-package-audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
raise SystemExit(bool(report['failed']))
