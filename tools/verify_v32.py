"""Audit the final same-version v3.2 patch against its actual installed evidence."""
from pathlib import Path
import hashlib,json,struct,zipfile
root=Path(__file__).resolve().parents[1];dist=root/'dist';work=root/'work/v3.2-fix';checks=[]
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def check(ok,name):
 checks.append({'name':name,'pass':bool(ok)});print('PASS' if ok else 'FAIL',name)
manifest=read(dist/'V32_RELEASE.json');binary=digest(root/'build/obs-spatial-fx.dll')
check(manifest['version']=='3.2.0' and manifest['pluginSha256']==binary,'same-version3.2.0 patch manifest')
for name,value in manifest['artifacts'].items():check(digest(dist/name)==value,'release SHA256 '+name)
for name,value in manifest['historicalArtifactsPreserved'].items():check(digest(dist/name)==value,'historical V1 through V3.1 unchanged '+name)
for entry in read(root/'work/v3.2/historical-hashes.json'):check(digest(Path(entry['Path'])).upper()==entry['Hash'].upper(),'original historical baseline '+Path(entry['Path']).name)
for name,value in read(work/'original-release/V32_RELEASE.json')['artifacts'].items():check(digest(work/'original-release'/name)==value,'original v3.2 rollback archive '+name)
for line in (dist/'SHA256SUMS.txt').read_text().splitlines():
 value,name=line.split('  ',1);assert digest(dist/name)==value,name
check(True,'whole dist SHA256 manifest')
setup=(dist/'OBS_Spatial_FX_V3.2_Setup.exe').read_bytes();pe=struct.unpack_from('<I',setup,0x3c)[0]
check(struct.unpack_from('<H',setup,pe+4)[0]==0x8664,'real Windows x64 PE installer')
check(struct.unpack_from('<H',setup,pe+24+68)[0]==2,'GUI installer subsystem')
check(setup==(dist/'OBS_Spatial_FX_Setup.exe').read_bytes()==(root/'build/OBS_Spatial_FX_V3.2_Setup.exe').read_bytes(),'both published installers equal tested build')
with zipfile.ZipFile(dist/'OBS_Spatial_FX_V3.2_Source.zip') as z:
 check(z.testzip() is None,'source CRC');names=z.namelist()
 check(all(n in names for n in ['src/hover_help.hpp','src/editor_curve_widget.cpp','src/editor_curves.cpp','src/editor_help.cpp','tests/v32fix_qa.inc','docs/v3.2/PATCH_2026-11-01.md','installer/setup.cpp']),'complete patch source')
 check(all((root/n).read_bytes()==z.read(n) for n in names),'archived source equals final workspace')
 check(not any(n.lower().endswith('.dll') for n in names),'source excludes OBS and Qt DLLs')
for kind in ['Evidence','Symbols']:
 with zipfile.ZipFile(dist/('OBS_Spatial_FX_V3.2_'+kind+'.zip')) as z:check(z.testzip() is None,kind+' CRC')
base=Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx')
for name in ['obs-spatial-fx.dll','bin/64bit/obs-spatial-fx.dll','portable33/obs-spatial-fx/obs-spatial-fx.dll']:
 check(digest(base/name)==binary,'installed exact DLL '+name)
check(set(p.name for p in base.rglob('*.dll'))=={'obs-spatial-fx.dll'},'installation contains only own DLL; no Qt/OBS/QA')
check(digest(base/'README.md')==digest(root/'README.md'),'installed current README')
upgrade=read(work/'upgrade-result.json');guard=read(work/'install-guard.json')
check(upgrade['ExitCode']==0 and upgrade['Version']=='3.2.0' and upgrade['NewHash'].lower()==binary and upgrade['OldHash']!=upgrade['NewHash'],'actual original-v3.2 overwrite and registry version')
check(guard['ExitCode']==10 and guard['HashUnchanged'],'actual running OBS install guard')
for major in [32,33]:
 module=read(work/f'installed{major}-final/installed-path.json')
 check(len(module['LoadedPaths'])==1 and module['InstalledHash'].lower()==binary and digest(Path(module['LoadedPaths'][0]))==binary,'OBS'+str(major)+' unique installed exact module')
for folder in ['installed32-final','installed33-final','general33-final','advanced33-final','audio33-final','dense33-final','persist33-final']:
 result=read(work/folder/'dock-smoke.json')
 check(result['failed']==0 and all(c['pass'] for c in result['checks']),'actual native QA '+folder+' '+str(len(result['checks']))+' checks')
for name in ['camera-render-tests.json','stability.json']:
 result=read(work/'gpu33-final'/name);check(result['failed']==0 and all(c['pass'] for c in result['checks']),'actual final GPU '+name)
persist=read(work/'persistence-compare.json');check(persist['passed'],'actual schema4 restart persistence')
check('100% tests passed' in (work/'final-test.log').read_text(encoding='utf-8-sig'),'actual5CTest executables')
cleanup=read(work/'cleanup.json')
check(not cleanup['writesDuringPatch'] and cleanup['historicalBaselineStatus'].startswith('NOT PROVEN'),'daily config has no write timestamps during patch; historical mismatch explicitly recorded')
check(cleanup['websocketRestored'] and not cleanup['obsProcesses'] and not cleanup['qaDlls'] and not cleanup['listeners4457'],'isolated QA process, DLL and websocket cleanup')
report={'checks':checks,'failed':sum(not c['pass'] for c in checks),'installerSha256':digest(dist/'OBS_Spatial_FX_V3.2_Setup.exe'),'pluginSha256':binary}
(work/'final-package-audit.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
raise SystemExit(bool(report['failed']))
