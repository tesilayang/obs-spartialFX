"""Fail-closed v3.4.1 packaging. Historical releases/manuals remain byte-identical."""
from pathlib import Path
import argparse,hashlib,json,os,re,shutil,struct,sys,uuid,zipfile
import package_v34_colors as archive_tools
ROOT=Path(__file__).resolve().parents[1];WORK=ROOT/'work/v3.4.1';REL=WORK/'release';DIST=ROOT/'dist'
PLUGIN=ROOT/'build/obs-spatial-fx.dll';SETUP=ROOT/'build/OBS_Spatial_FX_V3.4.1_Setup.exe'
sha=archive_tools.sha;read=archive_tools.read_json;write=archive_tools.write_json
checks=[]
def gate(name,fn):
 try:ok=bool(fn());detail=''
 except Exception as e:ok=False;detail=str(e)
 checks.append({'name':name,'pass':ok,**({'detail':detail} if detail else {})});print(('PASS ' if ok else 'FAIL ')+name,flush=True)
def fresh(p):return p.is_file() and p.stat().st_mtime_ns>=PLUGIN.stat().st_mtime_ns
def passed(p,count):
 v=read(p)
 return fresh(p) and v['failed']==0 and len(v['checks'])==count and all(c.get('pass') is True for c in v['checks'])
def provenance(folder,major,pinned):
 p=REL/folder/'installed-path.json';v=read(p);suffix='bin/64bit/obs-spatial-fx.dll' if major==32 else 'portable33/obs-spatial-fx/obs-spatial-fx.dll'
 expected='c:/programdata/obs-studio/plugins/obs-spatial-fx/'+suffix
 return fresh(p) and v['Major']==major and v['InstalledHash'].lower()==pinned and len(v['LoadedModules'])==1 and v['LoadedModules'][0]['Path'].replace('\\','/').lower()==expected and v['LoadedModules'][0]['Sha256'].lower()==pinned and len(v['ShaderAssets'])==19 and all(sha(ROOT/'data/effects'/a['Name'])==a['Sha256'].lower() for a in v['ShaderAssets'])
def performance(path):
 v=read(path);before=v['before'];samples=v['samples']
 return fresh(path) and len(samples)>=30 and v['warmMetrics']['allocations']==v['afterMetrics']['allocations'] and v['afterMetrics']['failures']==0 and all(s['activeFps']>59 and s['renderSkippedFrames']==before['renderSkippedFrames'] for s in samples)
def audit(pinned):
 checks.clear()
 gate('explicitly pinned final DLL',lambda:sha(PLUGIN)==pinned)
 gate('actual current build generated3.4.1 installer',lambda:SETUP.is_file() and fresh(SETUP) and 'OBS_Spatial_FX_V3.4.1_Setup.exe' in (WORK/'logs/build-3.log').read_text(encoding='utf-8-sig') and not re.search(r'FAILED:|fatal error|error C\d{4}',(WORK/'logs/build-3.log').read_text(encoding='utf-8-sig')))
 gate('fresh8/8 CTest result',lambda:fresh(WORK/'logs/test-final.log') and '100% tests passed out of 8' in (WORK/'logs/test-final.log').read_text(encoding='utf-8-sig'))
 def pe():
  b=SETUP.read_bytes();offset=struct.unpack_from('<I',b,0x3c)[0]
  return b[:2]==b'MZ' and b[offset:offset+4]==b'PE\0\0' and struct.unpack_from('<H',b,offset+4)[0]==0x8664 and struct.unpack_from('<H',b,offset+24+68)[0]==2
 gate('x64 native Windows GUI installer PE',pe)
 for major in (32,33):
  gate(f'OBS{major} exact installed DLL/shader provenance',lambda major=major:provenance(f'bridge{major}final',major,pinned))
  for stem,count in [('ui',79),('persist',18),('ui341',35),('persist341',16)]:
   gate(f'{stem}{major} native UI/persistence checks',lambda stem=stem,count=count,major=major:passed(REL/f'{stem}{major}'/'dock-smoke.json',count) and provenance(f'{stem}{major}',major,pinned))
  gate(f'quality{major}66 real pixel/pass/resource checks',lambda major=major:passed(REL/f'quality{major}'/'quality-tests.json',66))
  gate(f'encode{major}7 actual x264 output checks',lambda major=major:passed(REL/f'encode{major}'/'encode-tests.json',7))
  for stem,file,count in [('colors','live-colors.json',13),('siblings','blur-siblings.json',6),('blurquality','blur-quality.json',14),('edge','blur-edgecases.json',10),('light','lighting-controls.json',17),('patch','patch-tests.json',36)]:
   gate(f'{stem}{major} regression',lambda stem=stem,file=file,count=count,major=major:passed(REL/f'{stem}{major}'/file,count))
  def gpu(major=major):
   p=REL/f'gpu{major}'/'render-tests.json';v=read(p);ids={e['id'] for e in read(ROOT/'build/fx-runtime-catalog.json')['effects']}
   return passed(p,202) and len(v['effects'])==95 and {e['id'] for e in v['effects']}==ids and all(e['gpuPass'] and e['defaultVisible'] for e in v['effects'])
  gate(f'gpu{major}95 effects actual shader/output sweep',gpu)
  gate(f'gpu{major}30second1080p60 stable timing plateau',lambda major=major:performance(REL/f'gpu{major}'/'performance.json'))
  gate(f'coordinates{major} existing8checks',lambda major=major:passed(REL/f'gpu{major}'/'coordinates/coordinate-tests.json',8))
  def exits(major=major):
   codes=read(REL/f'exits{major}.json');return len(codes)==14 and all(type(v)is int and v==0 for v in codes.values())
  gate(f'OBS{major} all14 actual runner exit codes zero',exits)
  for folder in [f'bridge{major}final',f'ui{major}',f'persist{major}',f'ui341{major}',f'persist341{major}']:
   def clean(folder=folder):
    p=REL/folder/'obs-runtime.log';s=p.read_text(encoding='utf-8-sig');return fresh(p) and 'module unloaded' in s and not re.search(r'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(?:failed|failure)',s)
   gate(folder+' clean render and shutdown log',clean)
 def installer():
  p=WORK/'installer/installer-tests.json';v=read(p);return fresh(p) and len(v)==18 and all(x['pass'] is True for x in v)
 gate('18 installer upgrade/rollback/uninstall tests',installer)
 gate('running OBS guard rejects replacement with exit10',lambda:read(REL/'install-running-guard.json')=={'ExitCode':10,'HashUnchanged':True})
 base=Path(os.environ['ProgramData'])/'obs-studio/plugins/obs-spatial-fx'
 gate('installed plugin and uninstaller equal deliverables',lambda:sha(base/'obs-spatial-fx.dll')==pinned and sha(base/'Uninstall.exe')==sha(SETUP))
 gate('no Qt DLL or QA driver packaged',lambda:not list(base.rglob('Qt*.dll')) and not list(base.rglob('*qa-driver*')))
 gate('manual byte hashes preserved',lambda:all(sha(ROOT/k)==v for k,v in read(WORK/'baseline/manual-hashes.json').items()))
 gate('historical v3.4 setup preserved',lambda:sha(DIST/'OBS_Spatial_FX_V3.4_Setup.exe')==sha(WORK/'baseline/OBS_Spatial_FX_V3.4_Setup.exe'))
 gate('OBS closed before publishing final release',lambda:read(WORK/'environment-restored.json')['noObsProcesses'] is True)
 gate('isolated WebSocket/profile test settings restored byte-for-byte',lambda:all(v is True for v in read(WORK/'environment-restored.json')['restored'].values()))
 gate('developer report and deferred Native AA scope exist',lambda:(ROOT/'docs/v3.4.1/V341_REPORT.md').is_file() and (ROOT/'docs/v3.4.1/NATIVE_AA_FUTURE.md').is_file())
 return not any(c['pass'] is not True for c in checks)
def evidence_files():
 names={'quality-tests.json','encode-tests.json','render-tests.json','performance.json','dock-smoke.json','installed-path.json','native-ready.json','live-colors.json','blur-siblings.json','blur-quality.json','blur-edgecases.json','lighting-controls.json','patch-tests.json','coordinate-tests.json','installer-tests.json','install-running-guard.json','exits32.json','exits33.json','environment-restored.json'}
 for p in WORK.rglob('*'):
  if not p.is_file() or set(p.relative_to(WORK).parts)&{'baseline','installed-before','references','package-transactions','__pycache__'}:continue
  if p.suffix in ('.log','.png') or p.name in names or p==WORK/'environment.json':yield p
 yield WORK/'references/sources.json'
def package(pinned):
 staging=WORK/'package-transactions'/uuid.uuid4().hex;staging.mkdir(parents=True)
 names=['OBS_Spatial_FX_V3.4.1_Setup.exe','OBS_Spatial_FX_Setup.exe','OBS_Spatial_FX_V3.4.1_Source.zip','OBS_Spatial_FX_V3.4.1_Symbols.zip','OBS_Spatial_FX_V3.4.1_Evidence.zip']
 for name in names[:2]:shutil.copy2(SETUP,staging/name)
 sources=archive_tools.source_files();symbols=[ROOT/'build/obs-spatial-fx.pdb',ROOT/'build/OBS_Spatial_FX_V3.4.1_Setup.pdb']
 evidence=list(evidence_files())+list((ROOT/'docs/v3.4.1').glob('*.md'))
 snapshot=archive_tools.snapshot_inputs(sources+symbols+evidence+[PLUGIN,SETUP]);write(WORK/'package-input-hashes.json',snapshot)
 source_map=archive_tools.archive(staging/names[2],sources);archive_tools.archive(staging/names[3],symbols)
 archive_tools.archive(staging/names[4],evidence,sanitize=True)
 manifest={'version':'3.4.1','date':'2026-11-11','pluginSha256':pinned,'artifacts':{name:sha(staging/name) for name in names},'groups':13,'effects':95,'newEffects':['SMAA 1x Low/Medium','FXAA','AMD RCAS'],'nativeAA':'DEFERRED and hidden per user','ctest':8,'nativeChecksPerOBS':148,'qualityChecksPerOBS':66,'renderChecksPerOBS':202,'encodeChecksPerOBS':7,'installerChecks':18,'manualUnchanged':True,'limitations':['Integrated GPU and NVIDIA hardware NOT RUN; actual device is Radeon RX6900XT.','No network streaming test; real local1080p60 x264 encoding tested.','Spatial layer AA cannot fix new aliases introduced later by stage perspective or recover missing video detail.','SMAA/FXAA are mutually exclusive; RCAS can be used after either.','No hours-long soak test.'],'report':'docs/v3.4.1/V341_REPORT.md'}
 write(staging/'V341_RELEASE.json',manifest);names.append('V341_RELEASE.json')
 sums={p.name:sha(p) for p in DIST.iterdir() if p.is_file() and p.name!='SHA256SUMS.txt'};sums.update({name:sha(staging/name) for name in names})
 (staging/'SHA256SUMS.txt').write_text(''.join(v+'  '+k+'\n' for k,v in sorted(sums.items())),encoding='utf-8');names.append('SHA256SUMS.txt')
 archive_tools.assert_inputs(snapshot)
 if not audit(pinned):raise RuntimeError('final audit changed; no publication')
 archive_tools.WORK=WORK
 archive_tools.publish(staging,names,{name:sha(staging/name) for name in names},type('Audit',(),{'checks':checks})())
 write(WORK/'final-package-audit.json',{'checks':checks,'failed':0,'pluginSha256':pinned,'sourceMembers':len(source_map),'artifacts':{name:sha(DIST/name) for name in names},'sourceExactBytes':True,'symbolsExactBytes':True})
 print('Published v3.4.1 '+manifest['artifacts'][names[0]],flush=True)
def main():
 p=argparse.ArgumentParser();p.add_argument('--plugin-sha256',required=True);p.add_argument('--package',action='store_true');args=p.parse_args();pinned=args.plugin_sha256.lower()
 if not re.fullmatch('[0-9a-f]{64}',pinned):raise ValueError('pin a SHA256 hash')
 ok=audit(pinned);write(WORK/'package-checks.json',{'checks':checks,'failed':sum(not c['pass'] for c in checks)})
 if not ok:return 1
 if args.package:package(pinned)
 return 0
if __name__=='__main__':sys.exit(main())
