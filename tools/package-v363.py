"""Package v3.6.3 from fresh, scoped preset evidence and the verified installed payload."""
import hashlib
import json
import re
import shutil
import struct
import subprocess
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'work/v3.6.3'
DIST = ROOT / 'dist/v3.6.3'
SETUP = ROOT / 'build/OBS_Spatial_FX_V3.6.3_Setup.exe'
DLL = ROOT / 'build/obs-spatial-fx.dll'
BASE = Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx')
checks = []


def sha(path):
    with path.open('rb') as handle:
        return hashlib.file_digest(handle, 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def write(path, data):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def gate(name, ok):
    checks.append(dict(name=name, **{'pass': bool(ok)}))
    print(('PASS ' if ok else 'FAIL ') + name, flush=True)
    write(WORK / 'package-audit.json', dict(checks=checks, failed=sum(not c['pass'] for c in checks)))
    if not ok:
        raise RuntimeError(name)


def passed(path, minimum):
    data = read(path)
    rows = data['checks'] if isinstance(data, dict) else data
    return len(rows) >= minimum and all(c.get('pass') is True for c in rows) and (
        not isinstance(data, dict) or data.get('failed', 0) == 0)


def provenance(folder):
    data = read(WORK / folder / 'installed-path.json')
    log = (WORK / folder / 'obs-runtime.log').read_text(encoding='utf-8-sig')
    return (data['Major'] == 33 and data['InstalledHash'].lower() == sha(DLL)
            and len(data['LoadedModules']) == 1
            and data['LoadedModules'][0]['Sha256'].lower() == sha(DLL)
            and all(sha(ROOT / 'data/effects' / asset['Name']) == asset['Sha256'].lower()
                    for asset in data['ShaderAssets'])
            and 'module 3.6.3 loaded' in log and 'module unloaded' in log
            and not re.search(r'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(?:failed|failure)', log))


def archive(path, members):
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as handle:
        for name, data in sorted(members.items()):
            handle.writestr(name, data)
    with zipfile.ZipFile(path) as handle:
        assert handle.testzip() is None and set(handle.namelist()) == set(members)
        assert all(handle.read(name) == data for name, data in members.items())


build = (WORK / 'build-final.log').read_text(encoding='utf-8-sig')
gate('real final plugin and installer compilation', all(token in build for token in [
    'Linking CXX shared module obs-spatial-fx.dll',
    'Linking CXX executable OBS_Spatial_FX_V3.6.3_Setup.exe']))
test_log = (WORK / 'test-final.log').read_text(encoding='utf-8-sig')
gate('three scoped preset CTest groups pass', bool(re.search(
    r'100% tests passed(?:, 0 tests failed)? out of 3', test_log))
    and all(re.search(r'spatial-' + name + r'\s+\.+\s+Passed', test_log)
            for name in ('presets', 'composer', 'preset-panel')))
gate('original composition and stale-preview defects reproduced',
     'FAIL: automatic style combines' in (WORK / 'test-composer-red.log').read_text(encoding='utf-8-sig')
     and 'FAIL: applying stale preview preserves' in (WORK / 'test-panel-red.log').read_text(encoding='utf-8-sig'))
gate('current production source precedes final plugin and installer',
     all(path.stat().st_mtime_ns <= DLL.stat().st_mtime_ns for path in (ROOT / 'src').rglob('*')
         if path.is_file() and path.suffix in {'.cpp', '.hpp', '.h', '.inc'})
     and (ROOT / 'installer/setup.cpp').stat().st_mtime_ns <= SETUP.stat().st_mtime_ns)
gate('targeted installed OBS preset acceptance', passed(WORK / 'verified-presets/preset-tests.json', 25))
gate('QA commands execute once while response file is locked',
     passed(WORK / 'verified-bridge-lock/bridge-tests.json', 5))
runtime = read(WORK / 'verified-presets/runtime-info.json')
gate('actual OBS 33.0.1 at 1080p60', runtime['OBSversion'] == '33.0.1'
     and runtime['video']['baseWidth'] == 1920 and runtime['video']['baseHeight'] == 1080
     and runtime['video']['fpsNumerator'] == 60 * runtime['video']['fpsDenominator'])
workloads = read(WORK / 'verified-presets/preset-tests.json')['workloads']
gate('baseline and two rich presets sustain thirty-second workloads', len(workloads) == 3
     and all(row['elapsedSeconds'] >= 30 and row['meanFPS'] > 59
             and row['meanRenderMs'] < 1000 / 60 and row['addedSkippedFrames'] == 0
             for row in workloads))
gate('preset graphics resources stay cached', all(
    row['warm']['allocations'] == row['after']['allocations']
    and row['after']['failures'] == row['warm']['failures'] == 0
    and row['after']['renders'] - row['warm']['renders'] >= 1500 for row in workloads))
gate('preset save and fresh-process restoration', passed(WORK / 'verified-restart/restart-tests.json', 5))
gate('installed payload provenance and normal exits', provenance('verified-bridge') and provenance('verified-restart'))
gate('installer deployment and configuration preservation', passed(WORK / 'verified-install/install-tests.json', 5))
gate('installed payload equals final build', sha(DLL) == sha(BASE / 'obs-spatial-fx.dll')
     and sha(SETUP) == sha(BASE / 'Uninstall.exe'))
gate('no bundled Qt OBS or QA binaries', not list(BASE.rglob('Qt*.dll'))
     and not list(BASE.rglob('*qa-driver*')) and not list(BASE.rglob('obs.dll')))
restored = read(WORK / 'environment-restored.json')
gate('OBS stopped QA removed and isolated settings restored', restored['noObsProcesses']
     and restored['noQA'] and restored['noListener'] and all(restored['restored'].values()))
for previous in ('v3.6.2', 'v3.6.2-shadowfix'):
    folder = ROOT / 'dist' / previous
    gate('earlier ' + previous + ' delivery unchanged', all(sha(folder / name) == digest
         for digest, name in (line.split('  ', 1) for line in (
             folder / 'SHA256SUMS.txt').read_text(encoding='utf-8').splitlines())))
binary = SETUP.read_bytes()
pe = struct.unpack_from('<I', binary, 0x3c)[0]
gate('x64 Windows GUI installer', binary[:2] == b'MZ' and binary[pe:pe + 4] == b'PE\0\0'
     and struct.unpack_from('<H', binary, pe + 4)[0] == 0x8664
     and struct.unpack_from('<H', binary, pe + 92)[0] == 2)
gate('version 3.6.3 metadata', 'VERSION 3.6.3' in (
    ROOT / 'CMakeLists.txt').read_text(encoding='utf-8') and '3,6,3,0' in (
        ROOT / 'installer/setup.rc.in').read_text(encoding='utf-8'))
gate('new delivery directory', not DIST.exists())
DIST.mkdir()
shutil.copy2(SETUP, DIST / SETUP.name)
shutil.copy2(ROOT / 'docs/v3.6/V363_REPORT.md', DIST / 'V363_REPORT.md')
names = set()
for args in (['git', 'ls-files', '-z'], ['git', 'ls-files', '--others', '--exclude-standard', '-z']):
    names.update(name for name in subprocess.check_output(args, cwd=ROOT).decode('utf-8').split('\0') if name)
sources = {name: (ROOT / name).read_bytes() for name in names if (ROOT / name).is_file() and (
    name.startswith(('src/', 'data/', 'tests/', 'tools/', 'installer/', 'docs/')) or name in {
        'CMakeLists.txt', 'README.md', 'BUILDING.md', 'LICENSE', '.gitignore', 'AGENTS.md',
        'DEVELOPMENT_STANDARDS.md'}) and name != 'docs/v3.4.1/OBS_Spatial_FX_V3.4.1_Manual.html'}
gate('exact v3.6.3 source and tests included', all(name in sources for name in [
    'src/presets/composer.cpp', 'src/presets/panel.cpp', 'tests/composer_tests.cpp',
    'tests/preset_panel_tests.cpp', 'tests/v363_probe.py', 'tests/v363_restart_probe.py',
    'docs/v3.6/V363_REPORT.md', 'docs/v3.6/V363_IMPLEMENTATION.md']))
archive(DIST / 'OBS_Spatial_FX_V3.6.3_Source.zip', sources)
archive(DIST / 'OBS_Spatial_FX_V3.6.3_Symbols.zip', {path.name: path.read_bytes() for path in [
    ROOT / 'build/obs-spatial-fx.pdb', ROOT / 'build/OBS_Spatial_FX_V3.6.3_Setup.pdb']})
evidence = {}
for path in WORK.rglob('*'):
    if not path.is_file():
        continue
    rel = path.relative_to(WORK)
    include = rel.parts[0].startswith('verified-') and path.suffix in {'.log', '.json', '.png', '.jpg'}
    include |= len(rel.parts) == 1 and path.suffix in {'.log', '.md', '.json', '.diff'} and not path.name.startswith('baseline-')
    if include:
        data = path.read_bytes()
        if path.suffix == '.log':
            data = re.sub(r'(Possible connect address: )[^\r\n]+', r'\1[local QA]',
                          data.decode('utf-8-sig', errors='replace')).encode('utf-8')
        evidence[rel.as_posix()] = data
for name in ('install-final.ps1', 'finish-obs.ps1', 'restore-environment.ps1', 'test-bridge-lock.py'):
    evidence[name] = (WORK / name).read_bytes()
archive(DIST / 'OBS_Spatial_FX_V3.6.3_Evidence.zip', evidence)
write(DIST / 'DELIVERY_MANIFEST.json', dict(
    version='3.6.3', requiredOBS=['33.0.1'],
    sourceBaseCommit=subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
    sourceIncludesWorkingChanges=True, pluginSha256=sha(DLL),
    shaderSha256={path.name: sha(path) for path in sorted((ROOT / 'data/effects').iterdir()) if path.is_file()},
    sourceMembers=len(sources), evidenceMembers=len(evidence), checks=checks,
    artifacts={path.name: sha(path) for path in DIST.iterdir() if path.is_file()},
    manualUpdated=False, githubPublished=False,
    testScope='Preset composition, UI, append, persistence and directly affected rendering/performance',
    limitations=['Windows11 NVIDIA and integrated-GPU NOT RUN',
                 'Long streaming sessions and physical power measurement NOT RUN',
                 'Actual OBS rendering acceptance uses synthetic fixtures']))
(DIST / 'SHA256SUMS.txt').write_text(''.join(sha(path) + '  ' + path.name + '\n' for path in sorted(
    DIST.iterdir()) if path.is_file() and path.name != 'SHA256SUMS.txt'), encoding='utf-8')
write(WORK / 'package-audit.json', dict(checks=checks, failed=0, output=str(DIST),
      artifacts={path.name: sha(path) for path in DIST.iterdir() if path.is_file()}))
print('Packaged ' + str(DIST), flush=True)
