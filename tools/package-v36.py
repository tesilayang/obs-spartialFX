"""Verify final installed v3.6 payload and publish a local, auditable bundle."""
from pathlib import Path
import hashlib, json, re, shutil, struct, subprocess, zipfile

ROOT=Path(__file__).resolve().parents[1]; WORK=ROOT/'work/v3.6'; DIST=ROOT/'dist/v3.6'
PLUGIN=ROOT/'build/obs-spatial-fx.dll'; SETUP=ROOT/'build/OBS_Spatial_FX_V3.6.0_Setup.exe'
BASE=Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx'); checks=[]
def sha(p):
    with p.open('rb') as f: return hashlib.file_digest(f,'sha256').hexdigest()
def read(p): return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,value): p.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def gate(name,ok):
    checks.append(dict(name=name,**{'pass':bool(ok)})); print(('PASS ' if ok else 'FAIL ')+name,flush=True)
    if not ok:
        write(WORK/'package-audit.json',dict(checks=checks,failed=1)); raise RuntimeError(name)
def passed(p,minimum=1):
    d=read(p); rows=d['checks'] if isinstance(d,dict) else d
    return len(rows)>=minimum and all(c.get('pass',c.get('status')=='PASS') is True for c in rows) and (not isinstance(d,dict) or d.get('failed',0)==0)
def provenance(folder):
    d=read(WORK/folder/'installed-path.json')
    return d['Major']==33 and d['InstalledHash'].lower()==sha(PLUGIN) and len(d['LoadedModules'])==1 and d['LoadedModules'][0]['Sha256'].lower()==sha(PLUGIN) and all(sha(ROOT/'data/effects'/a['Name'])==a['Sha256'].lower() for a in d['ShaderAssets'])
def clean(folder):
    s=(WORK/folder/'obs-runtime.log').read_text(encoding='utf-8-sig')
    return 'module unloaded' in s and not re.search(r'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(?:failed|failure)',s)
def zip_exact(path,members):
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for n,data in sorted(members.items()): z.writestr(n,data)
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None and set(z.namelist())==set(members)
        assert all(z.read(n)==data for n,data in members.items())

build=(WORK/'build-final.log').read_text(encoding='utf-8-sig')
gate('final plugin and setup actually linked',all(s in build for s in ['Linking CXX shared module obs-spatial-fx.dll','Linking CXX executable OBS_Spatial_FX_V3.6.0_Setup.exe']))
gate('12 CTest groups passed','100% tests passed out of 12' in (WORK/'test-final.log').read_text(encoding='utf-8-sig'))
gate('source compiled before final DLL',all(p.stat().st_mtime_ns<=PLUGIN.stat().st_mtime_ns for p in (ROOT/'src').rglob('*') if p.is_file() and p.suffix in {'.cpp','.hpp','.h','.inc'}))
for folder,count in [('verified-ui',500),('verified-restart',14)]:
    gate(folder+' real native checks',passed(WORK/folder/'dock-smoke.json',count))
    gate(folder+' final installed provenance and normal exit',provenance(folder) and clean(folder))
gate('native geometry output checks',passed(WORK/'verified-gpu/geometry-tests.json',22))
gate('UI-created template pixels and five30sec workloads',passed(WORK/'verified-templates/template-tests.json',55))
perf=read(WORK/'verified-templates/template-tests.json')['workloads']
gate('five workloads at1080p60, cached resources and zero added render skips',len(perf)==5 and all(p['elapsedSeconds']>=30 and p['meanFPS']>59 and p['addedSkippedFrames']==0 and p['warm']['allocations']==p['after']['allocations'] for p in perf))
gate('existing95FX202GPUchecks',passed(WORK/'verified-fx/render-tests.json',202) and len(read(WORK/'verified-fx/render-tests.json')['effects'])==95)
gate('existing coordinate GPU checks',passed(WORK/'verified-fx/coordinates/coordinate-tests.json',8))
gate('graphics final payload and normal OBS exit',provenance('verified-bridge') and clean('verified-bridge'))
gate('installer upgrade rollback uninstall and reinstall',passed(WORK/'installer-final/installer-tests.json',18))
gate('running OBS blocked with identical payload',read(WORK/'verified-running-guard.json')==dict(ExitCode=10,HashUnchanged=True))
gate('installed DLL and uninstaller equal deliverables',sha(PLUGIN)==sha(BASE/'obs-spatial-fx.dll') and sha(SETUP)==sha(BASE/'Uninstall.exe'))
gate('installed payload excludes Qt and QA',not list(BASE.rglob('Qt*.dll')) and not list(BASE.rglob('*qa-driver*')))
restored=read(WORK/'environment-restored.json')
gate('isolated config restored, OBS exited, QA removed',restored['noObsProcesses'] and restored['noQA'] and restored['noListener'] and all(restored['restored'].values()))
b=SETUP.read_bytes(); pe=struct.unpack_from('<I',b,0x3c)[0]
gate('x64 Windows GUI setup',b[:2]==b'MZ' and b[pe:pe+4]==b'PE\0\0' and struct.unpack_from('<H',b,pe+4)[0]==0x8664 and struct.unpack_from('<H',b,pe+92)[0]==2)
gate('version3.6.0 metadata','VERSION 3.6.0' in (ROOT/'CMakeLists.txt').read_text(encoding='utf-8') and '3.6.0' in (ROOT/'installer/setup.cpp').read_text(encoding='utf-8'))
gate('new v3.6 delivery destination',not DIST.exists()); DIST.mkdir()
shutil.copy2(SETUP,DIST/SETUP.name); shutil.copy2(ROOT/'docs/v3.6/V36_REPORT.md',DIST/'V36_REPORT.md')
names={n for n in subprocess.check_output(['git','ls-files','-z'],cwd=ROOT).decode('utf-8').split('\0') if n}
names.update(n for n in subprocess.check_output(['git','ls-files','--others','--exclude-standard','-z'],cwd=ROOT).decode('utf-8').split('\0') if n)
sources={n:(ROOT/n).read_bytes() for n in names if (ROOT/n).is_file() and (n.startswith(('src/','data/','tests/','tools/','installer/','docs/')) or n in {'CMakeLists.txt','README.md','BUILDING.md','LICENSE','.gitignore','AGENTS.md','DEVELOPMENT_STANDARDS.md'}) and n!='docs/v3.4.1/OBS_Spatial_FX_V3.4.1_Manual.html'}
assert 'src/presets/graphics.cpp' in sources and 'tests/v36_qa.inc' in sources
zip_exact(DIST/'OBS_Spatial_FX_V3.6.0_Source.zip',sources)
zip_exact(DIST/'OBS_Spatial_FX_V3.6.0_Symbols.zip',{p.name:p.read_bytes() for p in [ROOT/'build/obs-spatial-fx.pdb',ROOT/'build/OBS_Spatial_FX_V3.6.0_Setup.pdb']})
evidence={}
for p in WORK.rglob('*'):
    if not p.is_file(): continue
    rel=p.relative_to(WORK); final=rel.parts[0].startswith('verified-') or rel.parts[0]=='installer-final'
    include=final and p.suffix in {'.log','.json','.png'} and not any(s.startswith('v36-extracted-') or s=='installed-before' for s in rel.parts)
    include|=len(rel.parts)==1 and (p.suffix=='.log' or p.name in {'document-check.json','environment-restored.json','verified-running-guard.json'})
    include|=len(rel.parts)==2 and rel.parts[0].startswith('ui-') and p.name in {'dock-smoke.json','obs-runtime.log','obs-crash.txt'}
    if include:
        data=p.read_bytes()
        if p.suffix=='.log': data=re.sub(r'(Possible connect address: )[^\r\n]+',r'\1[local QA]',data.decode('utf-8-sig',errors='replace')).encode('utf-8')
        evidence[rel.as_posix()]=data
zip_exact(DIST/'OBS_Spatial_FX_V3.6.0_Evidence.zip',evidence)
manifest=dict(version='3.6.0',requiredOBS=['33.0.1'],sourceBaseCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),sourceIncludesWorkingChanges=True,
              sourceBranch=subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip(),pluginSha256=sha(PLUGIN),shaderSha256={p.name:sha(p) for p in sorted((ROOT/'data/effects').iterdir()) if p.is_file()},
              sourceMembers=len(sources),evidenceMembers=len(evidence),artifacts={p.name:sha(p) for p in DIST.iterdir() if p.is_file()},checks=checks,manualUpdated=False,githubPublished=False,
              limitations=['Windows11 / NVIDIA / integrated GPU NOT RUN','Physical second-computer transfer NOT RUN; actual local archive extraction and independent scene restore PASS','5x30second1080p60 measurements; long streaming, hardware GPU timestamp and physical power measurement NOT RUN','Nonuniform parent scale with tilted child can introduce shear beyond current plane representation; baked curves exact at sample times'])
write(DIST/'DELIVERY_MANIFEST.json',manifest)
(DIST/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in sorted(DIST.iterdir()) if p.is_file() and p.name!='SHA256SUMS.txt'),encoding='utf-8')
write(WORK/'package-audit.json',dict(checks=checks,failed=0,output=str(DIST),artifacts={p.name:sha(p) for p in DIST.iterdir() if p.is_file()}))
print('Packaged '+str(DIST),flush=True)
