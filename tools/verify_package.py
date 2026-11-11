from pathlib import Path
import hashlib,json,struct,zipfile
root=Path('.');dist=root/'dist'
for line in (dist/'SHA256SUMS.txt').read_text().splitlines():
 expected,file=line.split('  ',1);assert hashlib.sha256((dist/file).read_bytes()).hexdigest()==expected,file
setup=(dist/'OBS_Spatial_FX_V1.0_Setup.exe').read_bytes();pe=struct.unpack_from('<I',setup,0x3c)[0];assert struct.unpack_from('<H',setup,pe+24+68)[0]==2
assert setup==(dist/'OBS_Spatial_FX_Setup.exe').read_bytes()
assert not Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx/bin/64bit/spatial-qa-driver.dll').exists()
with zipfile.ZipFile(dist/'OBS_Spatial_FX_V1.0_Source.zip') as z:
 names=z.namelist();assert 'src/obsconfig.h.in' in names and 'installer/setup.cpp' in names and 'tools/bootstrap.py' in names and 'tools/package_release.py' in names
 assert not any(n.startswith(('third_party/','msvc/')) or n.endswith('.dll') for n in names)
 assert z.testzip() is None
with zipfile.ZipFile(dist/'OBS_Spatial_FX_V1.0_Evidence.zip') as z:
 assert not any('normal-config-backup' in p for p in z.namelist());assert z.testzip() is None
audit=json.loads((root/'logs/release-audit.json').read_text(encoding='utf-8'));assert audit['failed']==0
assert json.loads((root/'logs/test-cleanup.json').read_text())['testPort4457Closed']
print('PASS final GUI EXE, both aliases, SHA256 manifest, source/evidence CRC, no bundled DLL dependencies or QA, test cleanup')
print('Release audit:',len(audit['checks']),'checks passed')
for p in sorted(dist.glob('*')):
 if p.is_file():print(p.name,p.stat().st_size,'bytes')
