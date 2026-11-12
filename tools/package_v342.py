"""Package after installed OBS33.0.1, GPU, UI and installer evidence passes."""
from pathlib import Path
import argparse,json,os,re,shutil,struct,subprocess,sys,uuid
import package_v34_colors as archive_tools

ROOT=Path(__file__).resolve().parents[1];WORK=ROOT/'work/v3.4.2';REL=WORK/'release';DIST=ROOT/'dist'
PLUGIN=ROOT/'build/obs-spatial-fx.dll';SETUP=ROOT/'build/OBS_Spatial_FX_V3.4.2_Setup.exe'
sha=archive_tools.sha;read=archive_tools.read_json;write=archive_tools.write_json
checks=[]
def gate(name,fn):
 try:ok=bool(fn());detail=''
 except Exception as e:ok=False;detail=str(e)
 checks.append({'name':name,'pass':ok,**({'detail':detail} if detail else {})});print(('PASS ' if ok else 'FAIL ')+name,flush=True)
def fresh(p):return p.is_file() and p.stat().st_mtime_ns>=PLUGIN.stat().st_mtime_ns
def passed(p,count):
 v=read(p);return fresh(p) and v['failed']==0 and len(v['checks'])==count and all(c.get('pass') is True for c in v['checks'])
def provenance(folder,major,pinned):
 p=REL/folder/'installed-path.json';v=read(p)
 suffix='bin/64bit/obs-spatial-fx.dll' if major==32 else 'portable33/obs-spatial-fx/obs-spatial-fx.dll'
 expected='c:/programdata/obs-studio/plugins/obs-spatial-fx/'+suffix
 return fresh(p) and v['Major']==major and v['InstalledHash'].lower()==pinned and len(v['LoadedModules'])==1 and v['LoadedModules'][0]['Path'].replace('\\','/').lower()==expected and v['LoadedModules'][0]['Sha256'].lower()==pinned and len(v['ShaderAssets'])==19 and all(sha(ROOT/'data/effects'/a['Name'])==a['Sha256'].lower() for a in v['ShaderAssets'])
def performance(path):
 v=read(path);before=v['before'];samples=v['samples']
 return fresh(path) and len(samples)>=30 and v['warmMetrics']['allocations']==v['afterMetrics']['allocations'] and v['afterMetrics']['failures']==0 and all(s['activeFps']>59 and s['renderSkippedFrames']==before['renderSkippedFrames'] for s in samples)
def targeted(major):return WORK/'boundary-gpu33' if major==33 else REL/'fx34232'
def audit(pinned):
 checks.clear();gate('pinned final DLL',lambda:sha(PLUGIN)==pinned)
 gate('real build produced current installer',lambda:fresh(SETUP) and 'Linking CXX executable OBS_Spatial_FX_V3.4.2_Setup.exe' in (WORK/'logs/build-3.log').read_text(encoding='utf-8-sig'))
 def pe():
  b=SETUP.read_bytes();at=struct.unpack_from('<I',b,0x3c)[0]
  return b[:2]==b'MZ' and b[at:at+4]==b'PE\0\0' and struct.unpack_from('<H',b,at+4)[0]==0x8664 and struct.unpack_from('<H',b,at+24+68)[0]==2
 gate('installer is an actual x64 Windows GUI executable',pe)
 gate('8 fresh CTest groups pass',lambda:fresh(WORK/'logs/test-final.log') and '100% tests passed out of 8' in (WORK/'logs/test-final.log').read_text(encoding='utf-8-sig'))
 gate('original bug was reproduced before fixes',lambda:read(WORK/'baseline-gpu33/fx342-tests.json')['failed']>=30)
 for major in (33,):
  gate(f'OBS{major} installed DLL and shader provenance',lambda major=major:provenance(f'bridge{major}final',major,pinned))
  for stem,count in [('ui',79),('persist',18),('ui341',35),('persist341',16),('ui342',47),('persist342',15)]:
   gate(f'{stem}{major} native Qt and restart checks',lambda stem=stem,count=count,major=major:passed(REL/f'{stem}{major}/dock-smoke.json',count) and provenance(f'{stem}{major}',major,pinned))
  gate(f'fx342{major}101 targeted checks including combined extreme grade',lambda major=major:passed(targeted(major)/'fx342-tests.json',101))
  for stem,file,count in [('quality','quality-tests.json',66),('encode','encode-tests.json',7),('colors','live-colors.json',13),('siblings','blur-siblings.json',6),('blurquality','blur-quality.json',14),('edge','blur-edgecases.json',10),('light','lighting-controls.json',17),('patch','patch-tests.json',36)]:
   gate(f'{stem}{major} actual GPU regression',lambda stem=stem,file=file,count=count,major=major:passed(REL/f'{stem}{major}'/file,count))
  gate(f'gpu{major}95 effects/202 checks',lambda major=major:passed(REL/f'gpu{major}/render-tests.json',202) and len(read(REL/f'gpu{major}/render-tests.json')['effects'])==95)
  gate(f'coordinates{major}8 checks',lambda major=major:passed(REL/f'gpu{major}/coordinates/coordinate-tests.json',8))
  gate(f'OBS{major} maximum-radius glow30second1080p60 resource plateau',lambda major=major:performance(targeted(major)/'glow-performance.json'))
  gate(f'OBS{major} existing mixed-chain30second1080p60 plateau',lambda major=major:performance(REL/f'gpu{major}/performance.json'))
  gate(f'OBS{major}17 runner exit codes zero',lambda major=major:len(read(REL/f'exits{major}.json'))==17 and all(type(v)is int and v==0 for v in read(REL/f'exits{major}.json').values()))
  for folder in [f'bridge{major}final',f'ui{major}',f'persist{major}',f'ui341{major}',f'persist341{major}',f'ui342{major}',f'persist342{major}']:
   def clean(folder=folder):
    p=REL/folder/'obs-runtime.log';s=p.read_text(encoding='utf-8-sig')
    return fresh(p) and 'module unloaded' in s and not re.search(r'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(?:failed|failure)',s)
   gate(folder+' clean rendering and normal shutdown',clean)
 gate('18 real3.4.1 upgrade/rollback/uninstall checks',lambda:len(read(WORK/'installer/installer-tests.json'))==18 and all(c['pass'] is True for c in read(WORK/'installer/installer-tests.json')))
 gate('OBS33 extended-boundary run uses final installed binary and shaders',lambda:provenance('../boundary-bridge33',33,pinned) and read(WORK/'boundary-exit.json')['ExitCode']==0 and 'module unloaded' in (WORK/'boundary-bridge33/obs-runtime.log').read_text(encoding='utf-8-sig'))
 gate('running OBS upgrade guard returns10 and preserves DLL',lambda:read(REL/'install-running-guard.json')=={'ExitCode':10,'HashUnchanged':True})
 base=Path(os.environ['ProgramData'])/'obs-studio/plugins/obs-spatial-fx'
 gate('installed payload equals local deliverables',lambda:sha(base/'obs-spatial-fx.dll')==pinned and sha(base/'Uninstall.exe')==sha(SETUP))
 gate('no Qt or QA driver in install payload',lambda:not list(base.rglob('Qt*.dll')) and not list(base.rglob('*qa-driver*')))
 gate('previous installers/source/manuals preserved',lambda:all(sha(ROOT/k)==v for k,v in read(WORK/'baseline/preserved-hashes.json').items()))
 gate('isolated environment restored and OBS closed',lambda:read(WORK/'environment-restored.json')['noObsProcesses'] is True and all(read(WORK/'environment-restored.json')['restored'].values()))
 gate('developer report exists',lambda:(ROOT/'docs/v3.4.2/V342_REPORT.md').is_file())
 gate('OBS33.0.1 runtime baseline',lambda:'OBS 33.0.1' in (REL/'bridge33final/obs-runtime.log').read_text(encoding='utf-8-sig'))
 gate('project standards and execution entry exist',lambda:all((ROOT/n).is_file() for n in ('DEVELOPMENT_STANDARDS.md','AGENTS.md')))
 return all(c['pass'] for c in checks)
def package(pinned):
 staging=WORK/'package-transactions'/uuid.uuid4().hex;staging.mkdir(parents=True)
 tracked=set(subprocess.check_output(['git','ls-files','-z'],cwd=ROOT).decode('utf-8').split('\0'))
 allowed={'docs/v3.4.2/PLAN.md','docs/v3.4.2/V342_REPORT.md','tests/v342_fx_probe.py','tests/v342_qa.inc','tools/run_v342_regressions.ps1','tools/test-installer-v342.ps1','tools/package_v342.py','DEVELOPMENT_STANDARDS.md','AGENTS.md'}
 sources=[p for p in archive_tools.source_files() if p.relative_to(ROOT).as_posix() in tracked|allowed]
 sources.extend(ROOT/n for n in ('DEVELOPMENT_STANDARDS.md','AGENTS.md'))
 names=['OBS_Spatial_FX_V3.4.2_Setup.exe','OBS_Spatial_FX_Setup.exe','OBS_Spatial_FX_V3.4.2_Source.zip','OBS_Spatial_FX_V3.4.2_Symbols.zip','OBS_Spatial_FX_V3.4.2_Evidence.zip']
 for name in names[:2]:shutil.copy2(SETUP,staging/name)
 source_map=archive_tools.archive(staging/names[2],sources)
 archive_tools.archive(staging/names[3],[ROOT/'build/obs-spatial-fx.pdb',ROOT/'build/OBS_Spatial_FX_V3.4.2_Setup.pdb'])
 evidence=[p for p in WORK.rglob('*') if p.is_file() and not (set(p.relative_to(WORK).parts)&{'baseline','installed-before','attempt1','attempt2','package-transactions','__pycache__'}) and (p.suffix in ('.log','.png','.json') and p.name not in ('native-command.json','native-answer.json','package-input-hashes.json','package-checks.json','final-package-audit.json'))]
 archive_tools.archive(staging/names[4],evidence+[ROOT/'docs/v3.4.2/V342_REPORT.md'],sanitize=True)
 manifest={'version':'3.4.2','date':'2026-11-12','sourceCommit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'requiredOBS':['33.0.1'],'pluginSha256':pinned,'artifacts':{n:sha(staging/n) for n in names},'groups':13,'effects':95,'ctest':8,'targetedChecksPerOBS':101,'gpuChecksPerOBS':202,'nativeChecksPerOBS':210,'installerChecks':18,'manualUnchanged':True,'report':'docs/v3.4.2/V342_REPORT.md','limitations':['Integrated GPU/NVIDIA/Windows11 NOT RUN; physical GPU RX6900XT.','30second warm1080p60 samples; no hours-long soak or Internet streaming.','Floating point intermediate textures use more memory; no equal-power promise on other devices.','Source highlights can clip at final SDR output.']}
 write(staging/'V342_RELEASE.json',manifest);names.append('V342_RELEASE.json')
 sums={p.name:sha(p) for p in DIST.iterdir() if p.is_file() and p.name!='SHA256SUMS.txt'};sums.update({name:sha(staging/name) for name in names})
 (staging/'SHA256SUMS.txt').write_text(''.join(v+'  '+k+'\n' for k,v in sorted(sums.items())),encoding='utf-8');names.append('SHA256SUMS.txt')
 if not audit(pinned):raise RuntimeError('Evidence changed before publication')
 archive_tools.WORK=WORK;archive_tools.publish(staging,names,{name:sha(staging/name) for name in names},type('Audit',(),{'checks':checks})())
 write(WORK/'final-package-audit.json',{'checks':checks,'failed':0,'pluginSha256':pinned,'sourceMembers':len(source_map),'artifacts':{name:sha(DIST/name) for name in names},'exactZipBytes':True})
def main():
 p=argparse.ArgumentParser();p.add_argument('--plugin-sha256',required=True);p.add_argument('--package',action='store_true');a=p.parse_args();pinned=a.plugin_sha256.lower()
 if not re.fullmatch('[0-9a-f]{64}',pinned):raise ValueError('Pin the final DLL SHA256')
 ok=audit(pinned);write(WORK/'package-checks.json',{'checks':checks,'failed':sum(not c['pass'] for c in checks)})
 if not ok:return 1
 if a.package:package(pinned)
 return 0
if __name__=='__main__':sys.exit(main())
