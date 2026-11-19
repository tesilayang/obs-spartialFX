"""Package verified v3.6.6-b workspace sources and installed-payload evidence."""
import ctypes,hashlib,json,re,shutil,struct,subprocess,zipfile
from pathlib import Path

root=Path(__file__).resolve().parents[1]
task=root/'work/v3.6.6-b';dist=root/'dist/v3.6.6-b';dist.mkdir(parents=True,exist_ok=True)
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

installer=root/'build/OBS_Spatial_FX_V3.6.6-b_Setup.exe';dll=root/'build/obs-spatial-fx.dll'
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
for folder in ('final-bridge', 'restart-bridge'):
    run = read(task / folder / 'installed-path.json')
    require(run['InstalledHash'] == digest(dll), 'Installed DLL differs')
    require([m['Sha256'] for m in run['LoadedModules']] == [digest(dll)], 'Loaded DLL differs')
    require(read(task / folder / 'exit-status.json')['exitCode'] == 0, 'OBS exit failed')
    log = (task / folder / 'obs-runtime.log').read_text(encoding='utf-8-sig')
    lines = [line for line in log.splitlines() if re.search(r'OBS 33\.0\.1 \(64-bit|Loading up D3D11 on adapter|\[SpatialFX\] (module |native Qt6 dock registered)', line)]
    require(any('module 3.6.6-b loaded, OBS=33.0.1' in line for line in lines), 'Wrong OBS runtime')
    (task / folder / 'load-excerpt.txt').write_text('\n'.join(lines) + '\n', encoding='utf-8')
for folder, count in [('qa2', 12), ('restart', 2), ('legacy', 3)]:
    results = read(task / folder / 'checks.json')
    require(len(results) == count and all(r['status'] == 'PASS' for r in results), folder + ' incomplete')
require('100% tests passed out of 5' in (task/'test-final.log').read_text(encoding='utf-8-sig'), 'CTest failed')
cleanup = read(task/'cleanup.json')
require(cleanup['qaRemoved'] and cleanup['status'] == 'PASS' and cleanup['registeredVersion'] == '3.6.6-b', 'Cleanup incomplete')
perf = read(task / 'qa/performance.json')
require([r['name'] for r in perf] == ['off', 'motion', 'all'] and all(r['frames'] > 1700 and r['skipped'] == 0 for r in perf), 'Performance results differ')
metrics = read(task/'style-metrics.json')
require(len({(r['style'], r['seed']) for r in metrics}) == 24 and all(min(r['midChange'],r['entryToMid'],r['exitToMid']) > 0 for r in metrics), 'Missing style samples')
files = subprocess.check_output(['git','ls-files','-z','--cached','--others','--exclude-standard'], cwd=root).decode().split('\0')
roots = ('src/', 'data/', 'tests/', 'tools/', 'installer/', 'cmake/', '.github/', 'docs/v3.6.6/')
fixed = {'CMakeLists.txt','README.md','BUILDING.md','LICENSE','AGENTS.md','DEVELOPMENT_STANDARDS.md','.gitignore','docs/architecture/SPATIAL_COORDINATE_CONTRACT.md','docs/v3.6.5/WINDOW_CAPTURE_FIX.md'}
source = []
for name in sorted(set(files)):
    path = root/name
    if path.is_file() and (name.startswith(roots) or name in fixed):
        require(path.suffix.lower() not in {'.dll','.exe','.pdb','.pyc','.key','.pem','.pfx'}, 'Unexpected source file: ' + name)
        source.append((name, path))
required = {'src/import_depth.hpp', 'src/presets/stage_motion.inc','src/presets/style_art.hpp','tests/composer_motion366.inc','tools/package_v366.py'} | {p.relative_to(root).as_posix() for p in (root/'tests').glob('v366_*.py')}
require(required <= {name for name, path in source}, 'Incomplete source archive')
prefix = 'OBS_Spatial_FX_V3.6.6-b_'
zip_items(dist/(prefix+'Source.zip'), source, 'SOURCE_MANIFEST.json')
shutil.copy2(installer, dist / installer.name)
shutil.copy2(root / 'docs/v3.6.6/TEST_REPORT.md',dist / 'TEST_REPORT.md')
gallery = ['装饰实拍对照.html', '八种风格实拍对照.jpg']
dump(dist/'PAYLOAD_MANIFEST.json', dict(version='3.6.6-b', baseCommit=subprocess.check_output(['git','rev-parse','HEAD'],cwd=root).decode().strip(), source='workspace snapshot', sourceFiles=len(source), installer=digest(installer), dll=digest(dll), resources=resources, gallery={n:digest(dist/n) for n in gallery}))
paths = [task/name for name in ['qa2/checks.json','qa/checks.json','qa/performance.json','legacy/checks.json','restart/checks.json','build-final.log','test-final.log','hardware.json','style-metrics.json','cleanup.json','install-final.log']]
for folder in ('baseline', 'final', 'rich', 'qa2', 'restart', 'legacy'):
    paths.extend((task/folder).glob('*.png'))
for folder in ('final-bridge','restart-bridge'):
    paths.extend(task/folder/name for name in ('exit-status.json','load-excerpt.txt','installed-path.json'))
evidence = [(p.relative_to(task).as_posix(),p) for p in paths] + [(n,dist/n) for n in ('TEST_REPORT.md','PAYLOAD_MANIFEST.json')]
zip_items(dist/(prefix+'Evidence.zip'), evidence, 'EVIDENCE_MANIFEST.json')
sums = dist/'SHA256SUMS.txt'
sums.write_text(''.join(digest(p)+'  '+p.name+'\n' for p in sorted(dist.iterdir()) if p.is_file() and p != sums), encoding='utf-8')
print(f'PASS: {len(source)} source files, {len(evidence)} evidence files, {len(resources)} installer resources')
