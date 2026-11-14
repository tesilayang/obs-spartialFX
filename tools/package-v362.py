"""Package v3.6.2 only after final installed-payload evidence has passed."""
import hashlib,json,re,shutil,struct,subprocess,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];WORK=ROOT/'work/v3.6.2';DIST=ROOT/'dist/v3.6.2'
SETUP=ROOT/'build/OBS_Spatial_FX_V3.6.2_Setup.exe';DLL=ROOT/'build/obs-spatial-fx.dll';BASE=Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx')
checks=[]
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,d):p.write_text(json.dumps(d,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def gate(name,ok):
    checks.append(dict(name=name,**{'pass':bool(ok)}));print(('PASS ' if ok else 'FAIL ')+name,flush=True)
    write(WORK/'package-audit.json',dict(checks=checks,failed=sum(not c['pass'] for c in checks)))
    if not ok:raise RuntimeError(name)
def passed(p,count):
    d=read(p);rows=d['checks'] if isinstance(d,dict) else d
    return len(rows)>=count and all(c.get('pass',c.get('status')=='PASS') is True for c in rows) and (not isinstance(d,dict) or d.get('failed',0)==0)
def provenance(folder):
    d=read(WORK/folder/'installed-path.json');log=(WORK/folder/'obs-runtime.log').read_text(encoding='utf-8-sig')
    return d['Major']==33 and d['InstalledHash'].lower()==sha(DLL) and len(d['LoadedModules'])==1 and d['LoadedModules'][0]['Sha256'].lower()==sha(DLL) and all(sha(ROOT/'data/effects'/a['Name'])==a['Sha256'].lower() for a in d['ShaderAssets']) and 'module unloaded' in log and not re.search(r'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(?:failed|failure)',log)
def archive(path,members):
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for name,data in sorted(members.items()):z.writestr(name,data)
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None and set(z.namelist())==set(members)
        assert all(z.read(name)==data for name,data in members.items())
build=(WORK/'build-final.log').read_text(encoding='utf-8-sig')
gate('real final plugin and installer compilation',all(t in build for t in ['Linking CXX shared module obs-spatial-fx.dll','Linking CXX executable OBS_Spatial_FX_V3.6.2_Setup.exe']))
gate('all13 CTest groups pass','100% tests passed out of 13' in (WORK/'test-final.log').read_text(encoding='utf-8-sig'))
gate('final installer collision fix compiled','Linking CXX executable OBS_Spatial_FX_V3.6.2_Setup.exe' in (WORK/'build-installer-final.log').read_text(encoding='utf-8-sig'))
gate('all production sources precede final plugin or setup',all(p.stat().st_mtime_ns<=DLL.stat().st_mtime_ns for p in (ROOT/'src').rglob('*') if p.is_file() and p.suffix in {'.cpp','.hpp','.h','.inc'}) and (ROOT/'installer/setup.cpp').stat().st_mtime_ns<=SETUP.stat().st_mtime_ns)
gate('actual OBS33.0.1 runtime',read(WORK/'verified-light/runtime-info.json')['version']['obsVersion']=='33.0.1')
gate('actual native light controls and GPU output',passed(WORK/'verified-light/light-tests.json',30))
gate('fresh process light persistence and stable render',passed(WORK/'verified-restart/restart-tests.json',14))
gate('actual installed shader DLL provenance and normal exits',provenance('verified-bridge') and provenance('verified-restart'))
gate('95 existing FX and202 real GPU checks',passed(WORK/'verified-fx/render-tests.json',202) and len(read(WORK/'verified-fx/render-tests.json')['effects'])==95)
gate('existing geometry24 GPU checks',passed(WORK/'verified-gpu/geometry-tests.json',24))
gate('existing coordinates8 GPU checks',passed(WORK/'verified-fx/coordinates/coordinate-tests.json',8))
gate('high-saturation unshadowed lighting GPU regression',passed(WORK/'verified-grade/grade-tests.json',1))
gate('independent FX environment and backlight regressions',passed(WORK/'verified-fx-lighting/patch-tests.json',25))
perf=read(WORK/'verified-light/light-tests.json')['workloads']
gate('three30second1080p60 lighting workloads',len(perf)==3 and all(p['elapsedSeconds']>=30 and p['meanFPS']>59 and p['addedSkippedFrames']==0 for p in perf))
gate('cached resources and real continuous rendering',all(p['warm']['allocations']==p['after']['allocations'] and p['after']['failures']==p['warm']['failures']==0 and p['after']['renders']-p['warm']['renders']>=1500 for p in perf))
gate('installer upgrade rollback configuration uninstall reinstall',passed(WORK/'installer-final/installer-tests.json',18))
gate('running OBS installation blocked unchanged',read(WORK/'verified-running-guard.json')==dict(ExitCode=10,HashUnchanged=True))
gate('installed payload matches final build',sha(DLL)==sha(BASE/'obs-spatial-fx.dll') and sha(SETUP)==sha(BASE/'Uninstall.exe'))
gate('no bundled Qt OBS or QA binaries',not list(BASE.rglob('Qt*.dll')) and not list(BASE.rglob('*qa-driver*')) and not list(BASE.rglob('obs.dll')))
restored=read(WORK/'environment-restored.json')
gate('OBS exited QA removed and isolated settings restored',restored['noObsProcesses'] and restored['noQA'] and restored['noListener'] and all(restored['restored'].values()))
gate('earlier3.6.1 installer unchanged',sha(ROOT/'dist/v3.6.1/OBS_Spatial_FX_V3.6.1_Setup.exe')=='57bfe4a795ad2102d5e9cbf00130896b843c96a9c547a749c4d6563feb84f765')
gate('all previous delivery assets unchanged',passed(WORK/'previous-delivery-unchanged.json',6))
b=SETUP.read_bytes();pe=struct.unpack_from('<I',b,0x3c)[0]
gate('x64 Windows GUI installer',b[:2]==b'MZ' and b[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',b,pe+4)[0]==0x8664 and struct.unpack_from('<H',b,pe+92)[0]==2)
gate('version3.6.2 metadata','VERSION 3.6.2' in (ROOT/'CMakeLists.txt').read_text(encoding='utf-8') and '3.6.2' in (ROOT/'installer/setup.cpp').read_text(encoding='utf-8'))
gate('new delivery directory',not DIST.exists());DIST.mkdir()
shutil.copy2(SETUP,DIST/SETUP.name);shutil.copy2(ROOT/'docs/v3.6/V362_REPORT.md',DIST/'V362_REPORT.md')
names=set()
for args in [['git','ls-files','-z'],['git','ls-files','--others','--exclude-standard','-z']]:names.update(n for n in subprocess.check_output(args,cwd=ROOT).decode('utf-8').split('\0') if n)
sources={n:(ROOT/n).read_bytes() for n in names if (ROOT/n).is_file() and (n.startswith(('src/','data/','tests/','tools/','installer/','docs/')) or n in {'CMakeLists.txt','README.md','BUILDING.md','LICENSE','.gitignore','AGENTS.md','DEVELOPMENT_STANDARDS.md'}) and n!='docs/v3.4.1/OBS_Spatial_FX_V3.4.1_Manual.html'}
gate('source contains exact lighting implementation and reproduction',all(n in sources for n in ['src/scene_evaluator.cpp','data/effects/spatial.effect','tests/v362_light_probe.py','tests/v362_restart_probe.py','docs/v3.6/V362_REPORT.md']))
archive(DIST/'OBS_Spatial_FX_V3.6.2_Source.zip',sources)
archive(DIST/'OBS_Spatial_FX_V3.6.2_Symbols.zip',{p.name:p.read_bytes() for p in [ROOT/'build/obs-spatial-fx.pdb',ROOT/'build/OBS_Spatial_FX_V3.6.2_Setup.pdb']})
evidence={}
for p in WORK.rglob('*'):
    if not p.is_file():continue
    rel=p.relative_to(WORK)
    include=(rel.parts[0].startswith('verified-') or rel.parts[0] in {'installer-final','red-repro','installer-first-failure'}) and p.suffix in {'.log','.json','.png','.jpg'} and not any(s in {'installed-before','recovery-staging-23688'} for s in rel.parts)
    include|=len(rel.parts)==1 and p.suffix in {'.log','.md','.json','.diff'} and not p.name.startswith('baseline-')
    if include:
        data=p.read_bytes()
        if p.suffix=='.log':data=re.sub(r'(Possible connect address: )[^\r\n]+',r'\1[local QA]',data.decode('utf-8-sig',errors='replace')).encode('utf-8')
        evidence[rel.as_posix()]=data
archive(DIST/'OBS_Spatial_FX_V3.6.2_Evidence.zip',evidence)
plan_key='docs/v3.6/V362_IMPLEMENTATION.md';plan=ROOT/plan_key
plan.write_text(plan.read_text(encoding='utf-8').replace('- [ ] 正常退出、配置恢复、打包及 SHA256 校验。','- [x] 正常退出、配置恢复、打包及 SHA256 校验。'),encoding='utf-8')
sources[plan_key]=plan.read_bytes();archive(DIST/'OBS_Spatial_FX_V3.6.2_Source.zip',sources)
write(DIST/'DELIVERY_MANIFEST.json',dict(version='3.6.2',requiredOBS=['33.0.1'],sourceBaseCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),sourceIncludesWorkingChanges=True,pluginSha256=sha(DLL),shaderSha256={p.name:sha(p) for p in sorted((ROOT/'data/effects').iterdir()) if p.is_file()},sourceMembers=len(sources),evidenceMembers=len(evidence),checks=checks,artifacts={p.name:sha(p) for p in DIST.iterdir() if p.is_file()},manualUpdated=False,githubPublished=False,limitations=['Windows11 NVIDIA and integrated-GPU NOT RUN','Long streaming sessions and physical power measurements NOT RUN','User scene geometry inspected read-only; synthetic fixtures used for render acceptance']))
(DIST/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in sorted(DIST.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
write(WORK/'package-audit.json',dict(checks=checks,failed=0,output=str(DIST),artifacts={p.name:sha(p) for p in DIST.iterdir() if p.is_file()}))
print('Packaged '+str(DIST),flush=True)
