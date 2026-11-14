"""Verify and package the v3.6.3 shadow/rim fix; preserve prior deliveries."""
import hashlib,json,re,shutil,struct,subprocess,zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'work/v3.6.3-shadow-isolation'
DIST=ROOT/'dist/v3.6.3-shadow-isolation'
BASE=Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx')
DLL=ROOT/'build/obs-spatial-fx.dll'
SETUP=ROOT/'build/OBS_Spatial_FX_V3.6.3_Setup.exe'
checks=[]
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def gate(name,ok):
    checks.append(dict(name=name,**{'pass':bool(ok)}));write(WORK/'package-audit.json',dict(checks=checks,failed=sum(not x['pass'] for x in checks)))
    print(('PASS ' if ok else 'FAIL ')+name,flush=True)
    if not ok:raise RuntimeError(name)
def passed(p,count):
    d=read(p);rows=d['checks'] if isinstance(d,dict) else d
    return len(rows)==count and all(x['pass'] is True for x in rows)
def provenance(folder):
    d=read(WORK/folder/'installed-path.json');s=(WORK/folder/'obs-runtime.log').read_text(encoding='utf-8-sig')
    return (d['Major']==33 and d['InstalledHash'].lower()==sha(DLL)
        and len(d['LoadedModules'])==1 and d['LoadedModules'][0]['Sha256'].lower()==sha(DLL)
        and all(sha(ROOT/'data/effects'/x['Name'])==x['Sha256'].lower() for x in d['ShaderAssets'])
        and 'module 3.6.3 loaded' in s and 'module unloaded' in s
        and not re.search(r'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(?:failed|failure)',s))
def archive(p,members):
    with zipfile.ZipFile(p,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for name,data in sorted(members.items()):z.writestr(name,data)
    with zipfile.ZipFile(p) as z:
        assert z.testzip() is None and set(z.namelist())==set(members)
        assert all(z.read(name)==data for name,data in members.items())

build=(WORK/'build-final.log').read_text(encoding='utf-8-sig')
gate('actual MSVC plugin and setup compilation',all(n in build for n in ['Linking CXX shared module obs-spatial-fx.dll','Linking CXX executable OBS_Spatial_FX_V3.6.3_Setup.exe']) and 'FAILED:' not in build)
gate('production sources precede tested DLL',all(p.stat().st_mtime_ns<=DLL.stat().st_mtime_ns for p in (ROOT/'src').rglob('*') if p.is_file() and p.suffix in {'.cpp','.h','.hpp','.inc'}))
gate('two related CTest groups passed',bool(re.search(r'100% tests passed(?:, 0 tests failed)? out of 2',(WORK/'test-delivery.log').read_text(encoding='utf-8-sig'))))
red=read(WORK/'red/isolation-tests.json')
gate('old installed version reproduced reported bugs',red['failed']==7 and len(red['checks'])==13)
for name,file,count in [('shadow isolation and UI','final-isolation/isolation-tests.json',33),('related glow and star smoke','final-optics/optics-tests.json',14),('related global projections','final-projection/projection-tests.json',6),('actual process restart','final-restart-results/restart-tests.json',5)]:
    gate(name,passed(WORK/file,count))
runtime=read(WORK/'final-isolation/runtime-info.json')
gate('OBS 33.0.1 real 1080p60 runtime',runtime['version']['obsVersion']=='33.0.1' and runtime['video']['baseWidth']==1920 and runtime['video']['baseHeight']==1080 and runtime['video']['fpsNumerator']==60*runtime['video']['fpsDenominator'])
perf=read(WORK/'final-isolation/performance.json')
gate('two 30-second workloads retain cached resources and 60 fps',len(perf)==2 and all(x['elapsedSeconds']>=30 and x['warm']['allocations']==x['after']['allocations'] and x['after']['failures']==0 and min(s['activeFps'] for s in x['samples'])>59 and x['samples'][-1]['renderSkippedFrames']==x['before']['renderSkippedFrames'] for x in perf))
gate('final installed DLL and shader provenance',provenance('final-bridge') and provenance('final-restart'))
gate('both isolated OBS sessions exited normally',all(read(WORK/x/'exit-status.json')['NormalExit'] and read(WORK/x/'exit-status.json')['ExitCode']==0 for x in ['final-bridge','final-restart']))
gate('installation payload checks',passed(WORK/'final-install/install-tests.json',9))
gate('installed DLL and maintenance setup match build',sha(DLL)==sha(BASE/'obs-spatial-fx.dll') and sha(SETUP)==sha(BASE/'Uninstall.exe'))
restored=read(WORK/'environment-restored.json')
gate('QA removed and isolated settings exactly restored',restored['noObsProcesses'] and restored['noQA'] and restored['noListener'] and all(restored['restored'].values()))
gate('code review recorded',(WORK/'code-review.md').exists())
for old in sorted((ROOT/'dist').glob('*/SHA256SUMS.txt')):
    gate('historical delivery unchanged '+old.parent.name,all(sha(old.parent/name)==digest for digest,name in (l.split('  ',1) for l in old.read_text(encoding='utf-8').splitlines())))
binary=SETUP.read_bytes();pe=struct.unpack_from('<I',binary,0x3c)[0]
gate('x64 GUI setup and version 3.6.3',struct.unpack_from('<H',binary,pe+4)[0]==0x8664 and struct.unpack_from('<H',binary,pe+92)[0]==2 and 'VERSION 3.6.3' in (ROOT/'CMakeLists.txt').read_text(encoding='utf-8'))
gate('new delivery directory',not DIST.exists());DIST.mkdir()
shutil.copy2(SETUP,DIST/'OBS_Spatial_FX_V3.6.3_ShadowFix_Setup.exe')
for name in ['V363_SHADOW_ISOLATION_REPORT.md','V363_SHADOW_ISOLATION_IMPLEMENTATION.md']:shutil.copy2(ROOT/'docs/v3.6'/name,DIST/name)
names=set()
for args in [['git','ls-files','-z'],['git','ls-files','--others','--exclude-standard','-z']]:names.update(n for n in subprocess.check_output(args,cwd=ROOT).decode('utf-8').split('\0') if n)
sources={n:(ROOT/n).read_bytes() for n in names if (ROOT/n).is_file() and (n.startswith(('src/','data/','tests/','tools/','installer/','docs/')) or n in {'CMakeLists.txt','README.md','BUILDING.md','LICENSE','.gitignore','AGENTS.md','DEVELOPMENT_STANDARDS.md'}) and n!='docs/v3.4.1/OBS_Spatial_FX_V3.4.1_Manual.html'}
gate('new implementation and regression probes included',all(n in sources for n in ['tests/v363_shadow_isolation_probe.py','tests/v363_shadow_isolation_restart.py','tools/package-v363-shadow-isolation.py','docs/v3.6/V363_SHADOW_ISOLATION_REPORT.md']))
archive(DIST/'OBS_Spatial_FX_V3.6.3_ShadowFix_Source.zip',sources)
archive(DIST/'OBS_Spatial_FX_V3.6.3_ShadowFix_Symbols.zip',{p.name:p.read_bytes() for p in [ROOT/'build/obs-spatial-fx.pdb',ROOT/'build/OBS_Spatial_FX_V3.6.3_Setup.pdb']})
evidence={}
for p in WORK.rglob('*'):
    if not p.is_file():continue
    rel=p.relative_to(WORK);head=rel.parts[0]
    include=(head.startswith(('verified-','final-')) or head in {'red','baseline-bridge','crop-red','crop-red-bridge'}) and p.suffix in {'.json','.png','.log'} and not p.name.startswith('native-')
    include|=len(rel.parts)==1 and p.suffix in {'.log','.md','.json','.diff','.ps1'} and not p.name.startswith('baseline-')
    if include:
        b=p.read_bytes()
        if p.suffix=='.log':b=re.sub(r'(Possible connect address: )[^\r\n]+',r'\1[local QA]',b.decode('utf-8-sig',errors='replace')).encode('utf-8')
        evidence[rel.as_posix()]=b
archive(DIST/'OBS_Spatial_FX_V3.6.3_ShadowFix_Evidence.zip',evidence)
write(DIST/'DELIVERY_MANIFEST.json',dict(version='3.6.3',build='shadow-rim-isolation',requiredOBS=['33.0.1'],pluginSha256=sha(DLL),setupSha256=sha(SETUP),shaderSha256={p.name:sha(p) for p in sorted((ROOT/'data/effects').iterdir()) if p.is_file()},sourceBaseCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),sourceIncludesWorkingChanges=True,checks=checks,sourceMembers=len(sources),evidenceMembers=len(evidence),manualUpdated=False,githubPublished=False,testScope='local shadow auxiliary removal, original-surface stage rim, original UV and masks, related optics/global projection, native colors, persistence and 1080p60 performance',notRun=['unrelated full regression','other platforms and GPUs','physical power measurement','long streaming session'],artifacts={p.name:sha(p) for p in DIST.iterdir() if p.is_file()}))
(DIST/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in sorted(DIST.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
write(WORK/'package-audit.json',dict(checks=checks,failed=0,output=str(DIST),artifacts={p.name:sha(p) for p in DIST.iterdir() if p.is_file()}))
print('Packaged '+str(DIST),flush=True)
