"""Package verified v3.6.4 workspace sources and installed-payload evidence."""
import ctypes,hashlib,json,re,shutil,struct,subprocess,zipfile
from pathlib import Path

root=Path(__file__).resolve().parents[1]
task=root/'work/v3.6.4';dist=root/'dist/v3.6.4';dist.mkdir(parents=True,exist_ok=True)
def sha(data):return hashlib.sha256(data).hexdigest().upper()
def digest(p):return sha(p.read_bytes())
def dump(p,data):p.write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def require(condition,message):
 if not condition:raise RuntimeError(message)
def zip_items(path,items,extra=None):
 entries={}
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for label,p in sorted(items):
   data=p.read_bytes();z.writestr(label,data);entries[label]=dict(size=len(data),sha256=sha(data))
  if extra:z.writestr(extra,json.dumps(entries,ensure_ascii=False,indent=2).encode())
 with zipfile.ZipFile(path) as z:
  require(z.testzip() is None,'ZIP CRC mismatch: '+str(path))
  for name,item in entries.items():require(sha(z.read(name))==item['sha256'],'ZIP payload mismatch: '+name)
 return entries

installer=root/'build/OBS_Spatial_FX_V3.6.4_Setup.exe';dll=root/'build/obs-spatial-fx.dll'
exe=installer.read_bytes();offset=struct.unpack_from('<I',exe,0x3c)[0]
require(exe[offset:offset+4]==b'PE\0\0' and struct.unpack_from('<H',exe,offset+4)[0]==0x8664,'Installer must be x64 PE')
require(struct.unpack_from('<H',exe,offset+24+68)[0]==2,'Installer must use Windows GUI subsystem')
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.LoadLibraryExW.argtypes=[ctypes.c_wchar_p,ctypes.c_void_p,ctypes.c_uint];kernel.LoadLibraryExW.restype=ctypes.c_void_p
kernel.FindResourceW.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p];kernel.FindResourceW.restype=ctypes.c_void_p
kernel.SizeofResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p];kernel.SizeofResource.restype=ctypes.c_uint
kernel.LoadResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p];kernel.LoadResource.restype=ctypes.c_void_p
kernel.LockResource.argtypes=[ctypes.c_void_p];kernel.LockResource.restype=ctypes.c_void_p
kernel.FreeLibrary.argtypes=[ctypes.c_void_p]
handle=kernel.LoadLibraryExW(str(installer),None,2);require(handle,'Could not read installer resources')
resources=[]
try:
 for rid,path in re.findall(r'(\d+) RCDATA "([^"]+)"',(root/'build/generated/setup.rc').read_text(encoding='utf-8')):
  resource=kernel.FindResourceW(handle,int(rid),10);require(resource,'Missing resource '+rid)
  data=ctypes.string_at(kernel.LockResource(kernel.LoadResource(handle,resource)),kernel.SizeofResource(handle,resource))
  p=Path(path);require(data==p.read_bytes(),'Resource differs from build source: '+path)
  resources.append(dict(id=int(rid),file=p.name,size=len(data),sha256=sha(data)))
finally:kernel.FreeLibrary(handle)
require(next(r['sha256'] for r in resources if r['id']==101)==digest(dll),'Embedded DLL differs')
require(not any('qt' in r['file'].lower() or 'qa-driver' in r['file'].lower() or r['file'].lower()=='obs.dll' for r in resources),'Unexpected runtime payload')
provenance=read(task/'final2-bridge/installed-path.json');require(provenance['InstalledHash']==digest(dll),'Installed load hash mismatch')
require(len(provenance['LoadedModules'])==1 and provenance['LoadedModules'][0]['Sha256']==digest(dll),'Duplicate or mismatched loaded plugin')
require(read(task/'final2-acceptance/acceptance.json')['failed']==0,'Native acceptance failed')
require(all(c['status']=='PASS' for c in read(task/'final2-clock/results.json')),'Clock GPU test failed')
require(all(c['status']=='PASS' for c in read(task/'final2-animation/results.json')),'Animation GPU test failed')
require(all(c['status']=='PASS' for c in read(task/'final2-restart/results.json')),'Final restart failed')
require(all(c['status']=='PASS' for c in read(task/'final2-performance/performance.json')['workloads']),'Performance checks failed')
require(read(task/'cleanup.json')['status']=='PASS','Runtime cleanup incomplete')
ctest=(task/'test-final2.log').read_text(encoding='utf-8-sig')
require(('100% tests passed out of 10' in ctest or '100% tests passed, 0 tests failed out of 10' in ctest) and '***Failed' not in ctest and 'Exception:' not in ctest,'Scoped CTest failure')

files=subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'],cwd=root).decode('utf-8').split('\0')
roots=('src/','data/','tests/','tools/','installer/','cmake/','.github/')
fixed={'CMakeLists.txt','README.md','BUILDING.md','LICENSE','AGENTS.md','DEVELOPMENT_STANDARDS.md','.gitignore','docs/architecture/SPATIAL_COORDINATE_CONTRACT.md','docs/superpowers/plans/2026-11-15-v3.6.4-timeline-and-alpha.md'}
items=[]
for name in sorted(set(files)):
 p=root/name
 if p.is_file() and (name in fixed or name.startswith(roots) or name.startswith('docs/v3.6.4/')):
  require(p.suffix.lower() not in {'.exe','.dll','.pdb','.pem','.key','.pfx','.pyc'},'Unexpected source artifact: '+name)
  items.append((name,p))
snapshot=zip_items(dist/'OBS_Spatial_FX_V3.6.4_Source.zip',items,'SOURCE_MANIFEST.json')
dump(task/'source-manifest.json',snapshot)
shutil.copy2(installer,dist/installer.name)
for name in ['IMPLEMENTATION.md','TEST_REPORT.md']:shutil.copy2(root/'docs/v3.6.4'/name,dist/name)
dump(dist/'PAYLOAD_MANIFEST.json',dict(version='3.6.4',source='current workspace snapshot; no new Git commit or tag',head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root).decode().strip(),dll=digest(dll),installer=digest(installer),resources=resources,sourceFiles=len(snapshot),liveNDI='BLOCKED: no sender'))
evidence=[]
folders=['final2-acceptance','final2-alpha','final2-animation','final2-clock','final2-spout','final2-performance','final2-restart','perf363']
for name in folders:
 for p in (task/name).rglob('*'):
  if p.is_file() and p.suffix.lower() in {'.json','.png','.log','.txt'}:evidence.append((str(p.relative_to(task)).replace('\\','/'),p))
for folder in ['final2-bridge','final2-restart-bridge']:
 for p in (task/folder).glob('*'):
  if p.is_file() and (p.suffix in {'.png','.log'} or p.name in ['installed-path.json','native-ready.json']):evidence.append((folder+'/'+p.name,p))
for p in task.glob('*'):
 if p.is_file() and p.suffix in {'.log','.md'}:evidence.append((p.name,p))
for name in ['hardware-cpu.json','hardware-gpu.json','cleanup.json','final-sender-discovery.json','source-manifest.json']:
 p=task/name
 if p.exists():evidence.append((name,p))
evidence.append(('payload-manifest.json',dist/'PAYLOAD_MANIFEST.json'))
zip_items(dist/'OBS_Spatial_FX_V3.6.4_Evidence.zip',evidence,'EVIDENCE_MANIFEST.json')
sumfile=dist/'SHA256SUMS.txt'
sumfile.write_text(''.join(digest(p)+'  '+p.name+'\n' for p in sorted(dist.iterdir()) if p.is_file() and p!=sumfile),encoding='utf-8')
print(json.dumps(dict(status='PASS',sourceFiles=len(snapshot),resources=len(resources),files=[dict(name=p.name,size=p.stat().st_size,sha256=digest(p)) for p in sorted(dist.iterdir()) if p.is_file()]),indent=2))
