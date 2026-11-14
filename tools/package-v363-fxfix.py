"""Package the scoped v3.6.3 optics fix without changing historical deliveries."""
import hashlib,json,re,shutil,struct,subprocess,zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'work/v3.6.3-fxfix'
DIST=ROOT/'dist/v3.6.3-fxfix'
DLL=ROOT/'build/obs-spatial-fx.dll'
SETUP=ROOT/'build/OBS_Spatial_FX_V3.6.3_Setup.exe'
BASE=Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx')
checks=[]
def sha(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,d):p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def gate(name,ok):
 checks.append(dict(name=name,**{'pass':bool(ok)}));write(WORK/'package-audit.json',dict(checks=checks,failed=sum(not c['pass'] for c in checks)))
 print(('PASS ' if ok else 'FAIL ')+name,flush=True)
 if not ok:raise RuntimeError(name)
def passed(p,count):
 d=read(p);rows=d['checks'] if isinstance(d,dict) else d
 return len(rows)>=count and all(c.get('pass') is True for c in rows) and (not isinstance(d,dict) or d.get('failed',0)==0)
def provenance(folder):
 d=read(WORK/folder/'installed-path.json');log=(WORK/folder/'obs-runtime.log').read_text(encoding='utf-8-sig')
 return d['Major']==33 and d['InstalledHash'].lower()==sha(DLL) and len(d['LoadedModules'])==1 and d['LoadedModules'][0]['Sha256'].lower()==sha(DLL) and all(sha(ROOT/'data/effects'/x['Name'])==x['Sha256'].lower() for x in d['ShaderAssets']) and 'module 3.6.3 loaded' in log and 'module unloaded' in log and not re.search(r'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(?:failed|failure)',log)
def archive(path,members):
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for name,data in sorted(members.items()):z.writestr(name,data)
 with zipfile.ZipFile(path) as z:
  assert z.testzip() is None and set(z.namelist())==set(members)
  assert all(z.read(name)==data for name,data in members.items())

build=(WORK/'build-final.log').read_text(encoding='utf-8-sig')
gate('actual plugin and installer compilation',all(t in build for t in ['Linking CXX shared module obs-spatial-fx.dll','Linking CXX executable OBS_Spatial_FX_V3.6.3_Setup.exe']))
gate('production sources precede final DLL',all(p.stat().st_mtime_ns<=DLL.stat().st_mtime_ns for p in (ROOT/'src').rglob('*') if p.is_file() and p.suffix in {'.cpp','.hpp','.h','.inc'}))
test=(WORK/'test-final.log').read_text(encoding='utf-8-sig')
gate('two scoped CTest groups pass',bool(re.search(r'100% tests passed(?:, 0 tests failed)? out of 2',test)))
red=read(WORK/'red/optics-tests.json')
gate('original bugs reproduced on old installed payload',red['failed']==13 and sum(not x['pass'] for x in red['checks'])==13)
gate('70 actual optics and native UI checks',passed(WORK/'verified-optics/optics-tests.json',70))
gate('six global projection and geometry mask checks',passed(WORK/'verified-projection/projection-tests.json',6))
gate('five actual fresh-process persistence checks',passed(WORK/'verified-restart-results/restart-tests.json',5))
runtime=read(WORK/'verified-optics/runtime-info.json')
gate('actual OBS 33.0.1 at 1080p60',runtime['version']['obsVersion']=='33.0.1' and runtime['video']['baseWidth']==1920 and runtime['video']['baseHeight']==1080 and runtime['video']['fpsNumerator']==60*runtime['video']['fpsDenominator'])
perf=read(WORK/'verified-optics/performance.json')
gate('two thirty-second workloads have stable resources and no new skipped frames',len(perf)==2 and all(p['elapsedSeconds']>=30 and p['warm']['allocations']==p['after']['allocations'] and p['after']['failures']==0 and min(s['activeFps'] for s in p['samples'])>59 and p['samples'][-1]['renderSkippedFrames']==p['before']['renderSkippedFrames'] for p in perf))
gate('final installed module and shader provenance',provenance('verified-bridge') and provenance('verified-restart'))
gate('normal process exits',all(read(WORK/f/'exit-status.json')['NormalExit'] and read(WORK/f/'exit-status.json')['ExitCode']==0 for f in ['verified-bridge','verified-restart']))
gate('nine installation checks',passed(WORK/'verified-install/install-tests.json',9))
gate('installed DLL and maintenance payload equal build',sha(DLL)==sha(BASE/'obs-spatial-fx.dll') and sha(SETUP)==sha(BASE/'Uninstall.exe'))
gate('no bundled OBS Qt or QA DLL',not list(BASE.rglob('Qt*.dll')) and not list(BASE.rglob('obs.dll')) and not list(BASE.rglob('*qa-driver*')))
restored=read(WORK/'environment-restored.json')
gate('isolated settings restored and QA removed',restored['noObsProcesses'] and restored['noQA'] and restored['noListener'] and all(restored['restored'].values()))
for previous in ['v3.6.2','v3.6.2-shadowfix','v3.6.3']:
 folder=ROOT/'dist'/previous
 gate('historical delivery unchanged '+previous,all(sha(folder/name)==digest for digest,name in (line.split('  ',1) for line in (folder/'SHA256SUMS.txt').read_text(encoding='utf-8').splitlines())))
binary=SETUP.read_bytes();pe=struct.unpack_from('<I',binary,0x3c)[0]
gate('x64 GUI installer and version 3.6.3',binary[:2]==b'MZ' and struct.unpack_from('<H',binary,pe+4)[0]==0x8664 and struct.unpack_from('<H',binary,pe+92)[0]==2 and 'VERSION 3.6.3' in (ROOT/'CMakeLists.txt').read_text(encoding='utf-8'))
gate('new independent delivery directory',not DIST.exists());DIST.mkdir()
shutil.copy2(SETUP,DIST/'OBS_Spatial_FX_V3.6.3_FXFix_Setup.exe')
for name in ['V363_FXFIX_REPORT.md','V363_FXFIX_IMPLEMENTATION.md']:shutil.copy2(ROOT/'docs/v3.6'/name,DIST/name)
names=set()
for args in [['git','ls-files','-z'],['git','ls-files','--others','--exclude-standard','-z']]:names.update(n for n in subprocess.check_output(args,cwd=ROOT).decode('utf-8').split('\0') if n)
sources={n:(ROOT/n).read_bytes() for n in names if (ROOT/n).is_file() and (n.startswith(('src/','data/','tests/','tools/','installer/','docs/')) or n in {'CMakeLists.txt','README.md','BUILDING.md','LICENSE','.gitignore','AGENTS.md','DEVELOPMENT_STANDARDS.md'}) and n!='docs/v3.4.1/OBS_Spatial_FX_V3.4.1_Manual.html'}
gate('fixed source tests records and packaging script included',all(n in sources for n in ['src/fx_optics.hpp','src/fx_renderer.cpp','tests/v363_fx_optics_probe.py','tests/v363_optics_global_probe.py','tests/v363_optics_restart_probe.py','tools/package-v363-fxfix.py','docs/v3.6/V363_FXFIX_REPORT.md']))
archive(DIST/'OBS_Spatial_FX_V3.6.3_FXFix_Source.zip',sources)
archive(DIST/'OBS_Spatial_FX_V3.6.3_FXFix_Symbols.zip',{p.name:p.read_bytes() for p in [ROOT/'build/obs-spatial-fx.pdb',ROOT/'build/OBS_Spatial_FX_V3.6.3_Setup.pdb']})
evidence={}
for p in WORK.rglob('*'):
 if not p.is_file():continue
 rel=p.relative_to(WORK);head=rel.parts[0]
 include=(head.startswith('verified-') or head in {'red','baseline-bridge'}) and p.suffix in {'.json','.png','.jpg','.log'} and not p.name.startswith('native-')
 include|=len(rel.parts)==1 and p.suffix in {'.log','.md','.json','.diff','.ps1'} and not p.name.startswith('baseline-')
 if include:
  b=p.read_bytes()
  if p.suffix=='.log':b=re.sub(r'(Possible connect address: )[^\r\n]+',r'\1[local QA]',b.decode('utf-8-sig',errors='replace')).encode('utf-8')
  evidence[rel.as_posix()]=b
archive(DIST/'OBS_Spatial_FX_V3.6.3_FXFix_Evidence.zip',evidence)
write(DIST/'DELIVERY_MANIFEST.json',dict(version='3.6.3',build='local-optics-fix',requiredOBS=['33.0.1'],pluginSha256=sha(DLL),shaderSha256={p.name:sha(p) for p in sorted((ROOT/'data/effects').iterdir()) if p.is_file()},sourceBaseCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),sourceIncludesWorkingChanges=True,sourceMembers=len(sources),evidenceMembers=len(evidence),checks=checks,artifacts={p.name:sha(p) for p in DIST.iterdir() if p.is_file()},manualUpdated=False,githubPublished=False,testScope='local inner/outer shadow, glow, color-selected star, UV/alpha/blur, crop/mask/global projection, native colors, persistence and performance',notRun=['Windows11 NVIDIA integrated GPU','long streaming sessions','physical power measurements','unrelated full regression']))
(DIST/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in sorted(DIST.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
write(WORK/'package-audit.json',dict(checks=checks,failed=0,output=str(DIST),artifacts={p.name:sha(p) for p in DIST.iterdir() if p.is_file()}))
print('Packaged '+str(DIST),flush=True)
