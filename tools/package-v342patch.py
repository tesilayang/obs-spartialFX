"""Audit final installed payload and create a separate local same-version patch bundle."""
from pathlib import Path
import hashlib,json,re,shutil,struct,subprocess,zipfile

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'work/v3.4.2-fxpatch-20261112'
DIST=ROOT/'dist/v3.4.2-fxpatch-20261112'
PLUGIN=ROOT/'build/obs-spatial-fx.dll'
SETUP=ROOT/'build/OBS_Spatial_FX_V3.4.2_Setup.exe'
BASE=Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
checks=[]
def gate(name,condition):
 ok=bool(condition);checks.append({'name':name,'pass':ok});print(('PASS ' if ok else 'FAIL ')+name,flush=True)
 if not ok:write(WORK/'package-audit.json',{'checks':checks,'failed':1});raise RuntimeError(name)
def passed(p):
 d=read(p);rows=d['checks'] if isinstance(d,dict) else d
 return bool(rows) and all(c.get('pass',c.get('pass_')) is True for c in rows) and (not isinstance(d,dict) or d.get('failed',0)==0)
def provenance(folder):
 d=read(WORK/folder/'installed-path.json');assets=d['ShaderAssets']
 return d['Major']==33 and d['InstalledHash'].lower()==sha(PLUGIN) and len(d['LoadedModules'])==1 and d['LoadedModules'][0]['Sha256'].lower()==sha(PLUGIN) and len(assets)==19 and all(sha(ROOT/'data/effects'/a['Name'])==a['Sha256'].lower() for a in assets)
def clean(folder):
 s=(WORK/folder/'obs-runtime.log').read_text(encoding='utf-8-sig')
 return 'module unloaded' in s and not re.search(r'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(?:failed|failure)',s)
def zip_exact(target,members):
 with zipfile.ZipFile(target,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for name,data in sorted(members.items()):z.writestr(name,data)
 with zipfile.ZipFile(target) as z:
  assert z.testzip() is None and set(z.namelist())==set(members)
  assert all(z.read(n)==b for n,b in members.items())

gate('final C++ build and setup linked',all(s in (WORK/'build-final2.log').read_text(encoding='utf-8-sig') for s in ('Linking CXX shared module obs-spatial-fx.dll','Linking CXX executable OBS_Spatial_FX_V3.4.2_Setup.exe')))
gate('8 final CTest groups passed','100% tests passed out of 8' in (WORK/'test-final2.log').read_text(encoding='utf-8-sig'))
gate('no C++ source changed after final compilation',all(p.stat().st_mtime_ns<=PLUGIN.stat().st_mtime_ns for p in (ROOT/'src').rglob('*') if p.is_file() and p.suffix in {'.cpp','.hpp','.inc'}))
jobs={'patch':'patch-tests.json','fx342':'fx342-tests.json','quality':'quality-tests.json','colors':'live-colors.json','siblings':'blur-siblings.json','blurquality':'blur-quality.json','edge':'blur-edgecases.json','light':'lighting-controls.json','composition':'patch-tests.json','gpu':'render-tests.json','encode':'encode-tests.json','boundaries':'boundary-tests.json'}
for name,file in jobs.items():gate('final '+name+' GPU evidence',passed(WORK/('final-'+name)/file))
gate('all95 effects rendered',len(read(WORK/'final-gpu/render-tests.json')['effects'])==95)
gate('coordinate GPU regression',passed(WORK/'final-gpu/coordinates/coordinate-tests.json'))
gate('final installed GPU bridge provenance',provenance('final-bridge') and clean('final-bridge'))
ui=['ui34','persist34','ui341','persist341','ui342','persist342','ui-patch','persist-patch']
for name in ui:gate('native UI/restart '+name,passed(WORK/('final-'+name)/'dock-smoke.json') and provenance('final-'+name) and clean('final-'+name))
gate('reinstalled final payload loads and restores settings',passed(WORK/'final-reinstall/dock-smoke.json') and provenance('final-reinstall') and clean('final-reinstall'))
codes=read(WORK/'final-exits.json');gate('all20 final runners exited zero',len(codes)==20 and all(type(v)is int and v==0 for v in codes.values()))
for name in ['final-patch','final-boundaries','final-fx342','final-gpu']:
 file='glow-performance.json' if name=='final-fx342' else 'performance.json'
 d=read(WORK/name/file);gate(name+' actual30second1080p60 plateau',len(d['samples'])>=30 and d['warmMetrics']['allocations']==d['afterMetrics']['allocations'] and d['afterMetrics']['failures']==0 and all(s['activeFps']>59 and s['renderSkippedFrames']==d['before']['renderSkippedFrames'] for s in d['samples']))
gate('18 installer upgrade rollback uninstall tests',passed(WORK/'installer/installer-tests.json') and len(read(WORK/'installer/installer-tests.json'))==18)
gate('running OBS installation blocked without changes',read(WORK/'final-running-guard.json')=={'ExitCode':10,'HashUnchanged':True})
gate('installed DLL and setup are final deliverables',sha(BASE/'obs-spatial-fx.dll')==sha(PLUGIN) and sha(BASE/'Uninstall.exe')==sha(SETUP))
gate('payload excludes Qt and QA drivers',not list(BASE.rglob('Qt*.dll')) and not list(BASE.rglob('*qa-driver*')))
state=read(WORK/'environment-restored.json');gate('test OBS closed and settings restored',state['noObsProcesses'] and state['noQA'] and state['noListener'] and all(state['restored'].values()))
baseline=read(WORK/'baseline-hashes.json');gate('published Release and all manual files unchanged',all(sha(ROOT/k)==v for k,v in baseline.items() if k.startswith(('dist/','docs/'))))
gate('original issue and review boundaries reproduced',read(WORK/'red-gpu/patch-tests.json')['failed']>0 and read(WORK/'red-boundaries/boundary-tests.json')['failed']==3)
gate('developer report exists',(ROOT/'docs/v3.4.2/PATCH_20261112.md').is_file())
b=SETUP.read_bytes();pe=struct.unpack_from('<I',b,0x3c)[0];gate('actual x64 GUI Windows installer',b[:2]==b'MZ' and b[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',b,pe+4)[0]==0x8664 and struct.unpack_from('<H',b,pe+92)[0]==2)
gate('distinct patch destination unused',not DIST.exists())
DIST.mkdir()
shutil.copy2(SETUP,DIST/SETUP.name)
shutil.copy2(ROOT/'docs/v3.4.2/PATCH_20261112.md',DIST/'PATCH_20261112.md')
tracked={n for n in subprocess.check_output(['git','ls-files','-z'],cwd=ROOT).decode('utf-8').split('\0') if n}
new={'tests/v342patch_probe.py','tests/v342patch_boundaries.py','tests/v342patch_qa.inc','tools/run-v342patch-regressions.ps1','tools/test-installer-v342patch.ps1','tools/package-v342patch.py','docs/v3.4.2/PATCH_20261112.md'}
sources={n:(ROOT/n).read_bytes() for n in tracked|new if (ROOT/n).is_file() and (n.startswith(('src/','data/','tests/','tools/','installer/','docs/')) or n in {'CMakeLists.txt','README.md','BUILDING.md','LICENSE','.gitignore','AGENTS.md','DEVELOPMENT_STANDARDS.md'})}
zip_exact(DIST/'OBS_Spatial_FX_V3.4.2_Source.zip',sources)
zip_exact(DIST/'OBS_Spatial_FX_V3.4.2_Symbols.zip',{p.name:p.read_bytes() for p in [ROOT/'build/obs-spatial-fx.pdb',ROOT/'build/OBS_Spatial_FX_V3.4.2_Setup.pdb']})
evidence={}
allowed_json={'installed-path.json','dock-smoke.json','patch-tests.json','performance.json','glow-performance.json','fx342-tests.json','quality-tests.json','live-colors.json','blur-siblings.json','blur-quality.json','blur-edgecases.json','lighting-controls.json','render-tests.json','coordinate-tests.json','encode-tests.json','boundary-tests.json'}
for path in WORK.rglob('*'):
 if not path.is_file():continue
 rel=path.relative_to(WORK);parts=rel.parts
 include=(parts[0].startswith('final-') and (path.name in allowed_json or path.suffix=='.log' or (path.suffix=='.png' and 'fixture' not in path.name))) or (len(parts)==1 and path.name in {'build-final2.log','test-final2.log','light-ray.log','red-gpu.log','red-ui.log','red-boundaries.log','gpu-before-after.png','environment-restored.json','final-running-guard.json'}) or (parts[0]=='installer' and path.suffix in {'.json','.log'} and len(parts)==2)
 if include and not any('native-command' in p or 'native-answer' in p or 'config' in p or p=='installed-before' for p in parts):
  data=path.read_bytes()
  if path.suffix=='.log':
   s=data.decode('utf-8-sig',errors='replace');s=re.sub(r'(Possible connect address: )[^\r\n]+',r'\1[local QA]',s);data=s.encode('utf-8')
  evidence[rel.as_posix()]=data
zip_exact(DIST/'OBS_Spatial_FX_V3.4.2_Evidence.zip',evidence)
manifest={'version':'3.4.2','revision':'fxpatch-20261112','requiredOBS':['33.0.1'],'sourceBaseCommit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'sourceIncludesWorkingChanges':True,'pluginSha256':sha(PLUGIN),'shaderSha256':{p.name:sha(p) for p in sorted((ROOT/'data/effects').iterdir()) if p.is_file()},'sourceMembers':len(sources),'evidenceMembers':len(evidence),'artifacts':{p.name:sha(p) for p in DIST.iterdir() if p.is_file()},'manualUnchanged':True,'publishedReleaseUnchanged':True,'githubPublished':False,'checks':checks,'limitations':['NVIDIA/integrated GPU/Windows11 NOT RUN','30second steady samples; hours-long soak and network streaming NOT RUN','No physical power measurement']}
write(DIST/'PATCH_MANIFEST.json',manifest)
(DIST/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in sorted(DIST.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
write(WORK/'package-audit.json',{'checks':checks,'failed':0,'output':str(DIST),'artifacts':{p.name:sha(p) for p in DIST.iterdir() if p.is_file()}})
print('Packaged '+str(DIST),flush=True)
