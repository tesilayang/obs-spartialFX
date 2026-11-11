"""Audits the exact release bytes and recorded real execution results."""
from pathlib import Path
import ctypes,hashlib,json,re,struct,sys
ROOT=Path(__file__).resolve().parents[1];checks=[]
def check(ok,name):
 checks.append({'name':name,'pass':bool(ok)});print(('PASS ' if ok else 'FAIL ')+name)
def read(p):return (ROOT/p).read_text(encoding='utf-8-sig')
def report(p):return json.loads(read(p))
source='\n'.join(p.read_text(encoding='utf-8-sig') for p in (ROOT/'src').glob('*') if p.suffix in ('.cpp','.hpp','.h','.inc'))
for api in ['obs_scene_sceneitem_from_source','obs_properties_add_button','obs_frontend_add_dock']:
 check(not re.search(r'\b'+api+r'\s*\(',source),'removed API absent: '+api)
check('/utf-8' in read('CMakeLists.txt'),'MSVC UTF-8 option present')
check('find_package(Qt6' in read('CMakeLists.txt') and 'Qt5' not in read('CMakeLists.txt'),'Qt6 build only')
for name in ['obs-spatial-fx.dll','OBS_Spatial_FX_V1.0_Setup.exe']:
 b=(ROOT/'build'/name).read_bytes();pe=struct.unpack_from('<I',b,0x3c)[0]
 check(b[:2]==b'MZ' and struct.unpack_from('<H',b,pe+4)[0]==0x8664 and struct.unpack_from('<H',b,pe+24)[0]==0x20b,'real x64 PE: '+name)
setup=ROOT/'build/OBS_Spatial_FX_V1.0_Setup.exe'
kernel=ctypes.WinDLL('kernel32',use_last_error=True)
kernel.LoadLibraryExW.argtypes=[ctypes.c_wchar_p,ctypes.c_void_p,ctypes.c_uint32];kernel.LoadLibraryExW.restype=ctypes.c_void_p
kernel.FindResourceW.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p];kernel.FindResourceW.restype=ctypes.c_void_p
kernel.LoadResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p];kernel.LoadResource.restype=ctypes.c_void_p
kernel.LockResource.argtypes=[ctypes.c_void_p];kernel.LockResource.restype=ctypes.c_void_p
kernel.SizeofResource.argtypes=[ctypes.c_void_p,ctypes.c_void_p];kernel.SizeofResource.restype=ctypes.c_uint32
kernel.FreeLibrary.argtypes=[ctypes.c_void_p]
module=kernel.LoadLibraryExW(str(setup),None,0x2)
if not module:raise ctypes.WinError(ctypes.get_last_error())
try:
 for number,path in [(101,'build/obs-spatial-fx.dll'),(102,'data/effects/spatial.effect'),(103,'README.md'),(104,'LICENSE')]:
  resource=kernel.FindResourceW(module,number,10);size=kernel.SizeofResource(module,resource);pointer=kernel.LockResource(kernel.LoadResource(module,resource));payload=ctypes.string_at(pointer,size)
  check(payload==(ROOT/path).read_bytes(),'embedded payload exactly matches '+path)
finally:kernel.FreeLibrary(module)
installed=Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx')
check((installed/'bin/64bit/obs-spatial-fx.dll').read_bytes()==(ROOT/'build/obs-spatial-fx.dll').read_bytes(),'installed DLL exactly equals release DLL')
check(not list(installed.rglob('Qt*.dll')),'installed package has zero Qt DLLs')
check(not list(installed.rglob('*qa-driver*')),'QA driver removed from installed package')
for p in ['logs/release-preset-apply/dock-smoke.json','logs/phase3/dock-smoke.json','logs/phase3-restart/dock-smoke.json','logs/phase4/features.json','logs/phase5-lifecycle-fixed/dock-smoke.json','logs/phase6-installed-normal/dock-smoke.json','logs/phase6-installed-normal/render-probe.json','logs/release-installed-performance/dock-smoke.json','logs/release-installed-performance/performance.json']:
 check(report(p)['failed']==0,'actual runtime assertions: '+p)
check(all(c['pass'] for c in report('logs/installer-tests.json')),'all 11 real installer assertions')
guard=report('logs/release-installer-guard.json');check(guard['installExit']==10 and guard['uninstallExit']==10 and guard['dllUnchanged'],'running OBS blocks install/uninstall with unchanged DLL')
discovery=report('logs/installer-running-discovery.json')['instances'];check(any(i['portable'] and i['supported'] for i in discovery),'automatic running portable discovery')
check(any(not i['supported'] and i['version'].startswith('29.') for i in discovery),'OBS29 rejected by version detection')
check(all(c['restored'] for c in report('logs/normal-config-restoration.json')),'normal OBS original configuration restored')
runtime=read('logs/release-installed-performance-obs-runtime.log')
check('Not all shader parameters were set' not in runtime and '[SpatialFX] effect compilation failed' not in runtime,'release runtime has no plugin shader errors')
check('module unloaded' in runtime and 'cached_texture_allocations=3' in runtime,'release clean shutdown and three cached surfaces')
tests=read('logs/release-unit-tests.log');check('100% tests passed' in tests,'real CTest run passed')
output={'checks':checks,'failed':sum(not c['pass'] for c in checks),'sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in ['build/obs-spatial-fx.dll','build/OBS_Spatial_FX_V1.0_Setup.exe']}}
(ROOT/'logs/release-audit.json').write_text(json.dumps(output,indent=2,ensure_ascii=False),encoding='utf-8')
sys.exit(bool(output['failed']))
