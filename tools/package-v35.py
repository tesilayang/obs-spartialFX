"""Gate final v3.5 evidence and write a reproducible local delivery bundle."""
from pathlib import Path
import hashlib
import json
import re
import shutil
import struct
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'work/v3.5'
DIST = ROOT / 'dist/v3.5'
PLUGIN = ROOT / 'build/obs-spatial-fx.dll'
SETUP = ROOT / 'build/OBS_Spatial_FX_V3.5.0_Setup.exe'
BASE = Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx')
checks = []


def sha(path):
    return hashlib.file_digest(path.open('rb'), 'sha256').hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def write(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def gate(name, condition):
    ok = bool(condition)
    checks.append({'name': name, 'pass': ok})
    print(('PASS ' if ok else 'FAIL ') + name, flush=True)
    if not ok:
        write(WORK / 'package-audit.json', {'checks': checks, 'failed': 1})
        raise RuntimeError(name)


def passed(path, count=None):
    d = read(path)
    rows = d['checks'] if isinstance(d, dict) else d
    return bool(rows) and (count is None or len(rows) == count) and all(r['pass'] is True for r in rows) and (not isinstance(d, dict) or d.get('failed', 0) == 0)


def provenance(folder):
    d = read(WORK / folder / 'installed-path.json')
    return d['Major'] == 33 and d['InstalledHash'].lower() == sha(PLUGIN) and len(d['LoadedModules']) == 1 and d['LoadedModules'][0]['Sha256'].lower() == sha(PLUGIN) and len(d['ShaderAssets']) == 19 and all(sha(ROOT / 'data/effects' / a['Name']) == a['Sha256'].lower() for a in d['ShaderAssets'])


def clean(folder):
    text = (WORK / folder / 'obs-runtime.log').read_text(encoding='utf-8-sig')
    return 'module unloaded' in text and not re.search(r'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(?:failed|failure)', text)


def zip_exact(path, members):
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for name, data in sorted(members.items()):
            z.writestr(name, data)
    with zipfile.ZipFile(path) as z:
        assert z.testzip() is None and set(z.namelist()) == set(members)
        assert all(z.read(name) == data for name, data in members.items())


gate('final plugin and installer linked', all(s in (WORK / 'build-final.log').read_text(encoding='utf-8-sig') for s in ['Linking CXX shared module obs-spatial-fx.dll', 'Linking CXX executable OBS_Spatial_FX_V3.5.0_Setup.exe']))
gate('10 CTest groups passed', '100% tests passed out of 10' in (WORK / 'test-final.log').read_text(encoding='utf-8-sig'))
gate('compiled sources precede final DLL', all(p.stat().st_mtime_ns <= PLUGIN.stat().st_mtime_ns for p in (ROOT / 'src').rglob('*') if p.is_file() and p.suffix in {'.cpp', '.hpp', '.c', '.h', '.inc'}))
for name, count in [('verified-roundtrip', 56), ('verified-restart', 14), ('verified-ui', 90)]:
    gate(name + ' actual OBS checks', passed(WORK / name / 'dock-smoke.json', count))
    gate(name + ' final installed payload and normal exit', provenance(name) and clean(name))
gate('actual ZIP64 4296015872-byte file roundtrip', 'PASS actual 4,296,015,872-byte file ZIP64 export/import/SHA/marker' in (WORK / 'archive-final-large.log').read_text(encoding='utf-8-sig'))
gate('18 installer upgrade rollback uninstall tests', passed(WORK / 'installer/installer-tests.json', 18))
gate('running OBS blocks install with unchanged payload', read(WORK / 'verified-running-guard.json') == {'ExitCode': 10, 'HashUnchanged': True})
gate('installed binaries equal final deliverables', sha(BASE / 'obs-spatial-fx.dll') == sha(PLUGIN) and sha(BASE / 'Uninstall.exe') == sha(SETUP))
gate('payload excludes Qt and QA', not list(BASE.rglob('Qt*.dll')) and not list(BASE.rglob('*qa-driver*')))
gate('95 effects and 202 actual GPU checks', passed(WORK / 'verified-gpu/render-tests.json', 202) and len(read(WORK / 'verified-gpu/render-tests.json')['effects']) == 95)
gate('8 coordinate GPU checks', passed(WORK / 'verified-gpu/coordinates/coordinate-tests.json', 8))
gate('GPU final installed provenance and normal exit', provenance('verified-bridge') and clean('verified-bridge'))
p = read(WORK / 'verified-gpu/performance.json')
gate('30 seconds 1080p60 resource reuse with no added skipped frames', len(p['samples']) == 30 and p['warmMetrics']['allocations'] == p['afterMetrics']['allocations'] and p['afterMetrics']['failures'] == 0 and all(s['activeFps'] > 59 and s['renderSkippedFrames'] == p['before']['renderSkippedFrames'] for s in p['samples']))
state = read(WORK / 'environment-restored.json')
gate('test environment restored', state['noObsProcesses'] and state['noQA'] and state['noListener'] and all(state['restored'].values()))
b = SETUP.read_bytes()
pe = struct.unpack_from('<I', b, 0x3c)[0]
gate('x64 GUI Windows executable', b[:2] == b'MZ' and b[pe:pe + 4] == b'PE\0\0' and struct.unpack_from('<H', b, pe + 4)[0] == 0x8664 and struct.unpack_from('<H', b, pe + 92)[0] == 2)
gate('version 3.5.0 in source and installer', 'VERSION 3.5.0' in (ROOT / 'CMakeLists.txt').read_text(encoding='utf-8') and '3.5.0' in (ROOT / 'installer/setup.cpp').read_text(encoding='utf-8'))
gate('new delivery destination', not DIST.exists())
DIST.mkdir()
shutil.copy2(SETUP, DIST / SETUP.name)
shutil.copy2(ROOT / 'docs/v3.5/V35_REPORT.md', DIST / 'V35_REPORT.md')
tracked = {n for n in subprocess.check_output(['git', 'ls-files', '-z'], cwd=ROOT).decode('utf-8').split('\0') if n}
new = {'data/PROJECT_NOTICES.txt', 'tests/project_document_tests.cpp', 'tests/project_archive_tests.cpp', 'tests/v35_qa.inc', 'tests/v35_ui_qa.inc', 'tools/test-installer-v35.ps1', 'tools/package-v35.py'}
for folder in ['src/project', 'docs/v3.5']:
    new.update(p.relative_to(ROOT).as_posix() for p in (ROOT / folder).rglob('*') if p.is_file())
sources = {n: (ROOT / n).read_bytes() for n in tracked | new if (ROOT / n).is_file() and (n.startswith(('src/', 'data/', 'tests/', 'tools/', 'installer/', 'docs/')) or n in {'CMakeLists.txt', 'README.md', 'BUILDING.md', 'LICENSE', '.gitignore', 'AGENTS.md', 'DEVELOPMENT_STANDARDS.md'})}
assert 'src/project/vendor/miniz/miniz.c' in sources
assert 'docs/v3.4.1/OBS_Spatial_FX_V3.4.1_Manual.html' not in sources
zip_exact(DIST / 'OBS_Spatial_FX_V3.5.0_Source.zip', sources)
zip_exact(DIST / 'OBS_Spatial_FX_V3.5.0_Symbols.zip', {p.name: p.read_bytes() for p in [ROOT / 'build/obs-spatial-fx.pdb', ROOT / 'build/OBS_Spatial_FX_V3.5.0_Setup.pdb']})
evidence = {}
root_logs = {'build-final.log', 'test-final.log', 'archive-final-large.log', 'archive-unicode-red.log', 'archive-unicode-green.log', 'verified-running-guard.json', 'verified-running-guard.log', 'environment-restored.json', 'verified-gpu.log', 'verified-coordinates.log', 'verified-roundtrip.log', 'verified-restart.log', 'verified-ui.log', 'installer-final.log', 'ui-first-run.log'}
allowed_json = {'installed-path.json', 'dock-smoke.json', 'render-tests.json', 'coordinate-tests.json', 'performance.json', 'ui-workflows.json'}
for path in WORK.rglob('*'):
    if not path.is_file():
        continue
    rel = path.relative_to(WORK)
    include = len(rel.parts) == 1 and path.name in root_logs
    include |= rel.parts[0].startswith('verified-') and len(rel.parts) <= 3 and (path.name in allowed_json or path.name == 'obs-runtime.log' or path.suffix == '.png')
    include |= rel.parts[0] == 'installer' and len(rel.parts) == 2 and path.suffix in {'.log', '.json'}
    include |= rel.as_posix() in {'obs-roundtrip-final/qa-harness-crash.txt', 'red-ui/dock-smoke.json', 'obs-ui-first/dock-smoke.json'}
    if include:
        data = path.read_bytes()
        if path.suffix in {'.log', '.txt'}:
            text = data.decode('utf-8-sig', errors='replace')
            text = re.sub(r'(Possible connect address: )[^\r\n]+', r'\1[local QA]', text)
            data = text.encode('utf-8')
        evidence[rel.as_posix()] = data
zip_exact(DIST / 'OBS_Spatial_FX_V3.5.0_Evidence.zip', evidence)
manifest = {'version': '3.5.0', 'requiredOBS': ['33.0.1'], 'sourceBaseCommit': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(), 'sourceBranch': subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT, text=True).strip(), 'sourceIncludesWorkingChanges': True, 'pluginSha256': sha(PLUGIN), 'shaderSha256': {p.name: sha(p) for p in sorted((ROOT / 'data/effects').iterdir()) if p.is_file()}, 'sourceMembers': len(sources), 'evidenceMembers': len(evidence), 'artifacts': {p.name: sha(p) for p in DIST.iterdir() if p.is_file()}, 'checks': checks, 'manualUpdated': False, 'githubPublished': False, 'limitations': ['Other OBS versions / Windows11 / NVIDIA / integrated GPU NOT RUN', 'Physical second-computer transfer NOT RUN; offline original-path-unavailable restoration PASS', '30second 1080p60 samples; long streaming and physical power measurement NOT RUN']}
write(DIST / 'DELIVERY_MANIFEST.json', manifest)
(DIST / 'SHA256SUMS.txt').write_text(''.join(sha(p) + '  ' + p.name + '\n' for p in sorted(DIST.iterdir()) if p.is_file() and p.name != 'SHA256SUMS.txt'), encoding='utf-8')
write(WORK / 'package-audit.json', {'checks': checks, 'failed': 0, 'output': str(DIST), 'artifacts': {p.name: sha(p) for p in DIST.iterdir() if p.is_file()}})
print('Packaged ' + str(DIST), flush=True)
