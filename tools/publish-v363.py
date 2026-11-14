"""Build immutable v3.6.3 release assets or verify the public GitHub release.

Package: python tools/publish-v363.py --commit HEAD
Verify:  python tools/publish-v363.py --verify
Requires the retained validated local delivery for payload provenance. New clones
can build normally; they do not need this author-machine publication script.
"""
from pathlib import Path
import argparse, hashlib, json, re, shutil, subprocess, zipfile

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'work/publish-v363'
DIST=ROOT/'dist/v3.6.3-release'
REPO='tesilayang/obs-spartialFX'
GH=shutil.which('gh') or 'F:/Program Files (x86)/github/bin/gh.exe'

def run(*args):
    if args[0]=='git':args=('git','-c','core.quotepath=false',*args[1:])
    return subprocess.check_output(args,cwd=ROOT).decode('utf-8').strip()

def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,obj):p.write_text(json.dumps(obj,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')

def archive(path,members):
    with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
        for name,p in sorted(members.items()):z.write(p,name)
    with zipfile.ZipFile(path) as z:assert z.testzip() is None

def package(commit):
    commit=run('git','rev-parse',commit)
    assert commit==run('git','rev-parse','HEAD'),'Release source must be the validated current HEAD'
    assert not run('git','status','--porcelain','--untracked-files=no'),'Commit all tracked changes first'
    old=ROOT/'dist/v3.6.3-shadow-isolation'
    dll=ROOT/'build/obs-spatial-fx.dll'
    assert sha(dll)=='374b284c6509d1e36a7a2fb21abf9da86d2cb3c5567b42d21351edb72e924b5e'
    expected_setup='a707d741164b6156d9f75e30e64b36efae9cb6bb976876ad49963ab1033397e8'
    assert sha(ROOT/'build/OBS_Spatial_FX_V3.6.3_Setup.exe')==expected_setup,'Setup differs from final installed/smoked payload'
    assert sha(Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx/Uninstall.exe'))==expected_setup,'Installed setup differs from final tested payload'
    # Establish that only publication material changed since actual GPU validation.
    with zipfile.ZipFile(old/'OBS_Spatial_FX_V3.6.3_ShadowFix_Source.zip') as z:
        for name in z.namelist():
            if name.startswith(('src/','data/','installer/')) or name=='CMakeLists.txt':
                assert z.read(name)==(ROOT/name).read_bytes(),name
    build=(WORK/'build-final.log').read_text(encoding='utf-8-sig')
    assert 'Linking CXX executable OBS_Spatial_FX_V3.6.3_Setup.exe' in build and 'FAILED:' not in build
    assert all(x['pass'] for x in read(WORK/'final-install-publish/install-tests.json'))
    assert all(x['status']=='PASS' for x in read(WORK/'runtime-smoke.json')['checks'])
    assert read(WORK/'manual-qa.json')['status']=='PASS'
    env=read(WORK/'environment-restored.json')
    assert all(env[k] for k in ('noObsProcesses','noQA','noListener')) and all(env['restored'].values())
    provenance=read(WORK/'bridge-release/installed-path.json')
    assert provenance['InstalledHash'].lower()==sha(dll)
    assert all(x['Sha256'].lower()==sha(dll) for x in provenance['LoadedModules'])
    for x in provenance['ShaderAssets']:assert x['Sha256'].lower()==sha(ROOT/'data/effects'/x['Name'])
    log=(WORK/'bridge-release/obs-runtime.log').read_text(encoding='utf-8-sig')
    assert 'module 3.6.3 loaded' in log and 'module unloaded' in log
    assert not re.search(r'Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(?:failed|failure)',log)
    assert read(WORK/'bridge-release/exit-status.json')['NormalExit']
    for checksums in (ROOT/'dist').glob('*/SHA256SUMS.txt'):
        for line in checksums.read_text(encoding='utf-8').splitlines():
            digest,name=line.split('  ',1)
            assert sha(checksums.parent/name)==digest,(checksums.parent.name,name)
    assert not DIST.exists(),'Preserve existing publication snapshot'
    DIST.mkdir()
    shutil.copy2(ROOT/'build/OBS_Spatial_FX_V3.6.3_Setup.exe',DIST/'OBS_Spatial_FX_V3.6.3_Setup.exe')
    manual=ROOT/'docs/v3.6.3/manual'
    shutil.copy2(manual/'index.html',DIST/'OBS_Spatial_FX_V3.6.3_Manual.html')
    subprocess.run(['git','archive','--format=zip','-o',str(DIST/'OBS_Spatial_FX_V3.6.3_Source.zip'),commit],cwd=ROOT,check=True)
    with zipfile.ZipFile(DIST/'OBS_Spatial_FX_V3.6.3_Source.zip') as z:
        assert z.testzip() is None
        files=run('git','ls-tree','-r','--name-only',commit).splitlines()
        assert set(files)==set(z.namelist())-{n for n in z.namelist() if n.endswith('/')}
        for name in files:assert z.read(name)==subprocess.check_output(['git','show',commit+':'+name],cwd=ROOT),name
    archive(DIST/'OBS_Spatial_FX_V3.6.3_Manual.zip',{str(p.relative_to(manual)):p for p in manual.rglob('*') if p.is_file()})
    archive(DIST/'OBS_Spatial_FX_V3.6.3_Symbols.zip',{p.name:p for p in [ROOT/'build/obs-spatial-fx.pdb',ROOT/'build/OBS_Spatial_FX_V3.6.3_Setup.pdb']})
    evidence={}
    # Preserve the evidence packages for each directly relevant prior delivery.
    for directory in ['v3.6.3','v3.6.3-fxfix','v3.6.3-shadow-isolation','v3.6.2-shadowfix']:
        for p in (ROOT/'dist'/directory).glob('*Evidence.zip'):evidence['retained/'+p.name]=p
    for p in WORK.rglob('*'):
        if not p.is_file():continue
        rel=p.relative_to(WORK)
        if len(rel.parts)==1 and p.suffix in {'.log','.md'}:evidence['publication/'+str(rel)]=p
        elif len(rel.parts)==1 and p.name in {'manual-qa.json','runtime-smoke.json','environment-restored.json','repository-audit.json'}:evidence['publication/'+str(rel)]=p
        elif rel.parts[0] in {'bridge-release','final-install-publish'} and p.suffix in {'.json','.log','.png'} and not p.name.startswith('native-'):evidence['publication/'+str(rel)]=p
    archive(DIST/'OBS_Spatial_FX_V3.6.3_Evidence.zip',evidence)
    shutil.copy2(ROOT/'docs/v3.6.3/PUBLISH_REPORT.md',DIST/'PUBLISH_REPORT.md')
    assets=[dict(name=p.name,bytes=p.stat().st_size,sha256=sha(p)) for p in sorted(DIST.iterdir())]
    write(DIST/'RELEASE_MANIFEST.json',dict(version='3.6.3',tag='v3.6.3',commit=commit,dll_sha256=sha(dll),assets=assets))
    (DIST/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in sorted(DIST.iterdir())),encoding='utf-8')
    print(json.dumps(dict(directory=str(DIST),commit=commit,assets=len(list(DIST.iterdir()))),ensure_ascii=False))

def verify():
    manifest=read(DIST/'RELEASE_MANIFEST.json');commit=manifest['commit']
    checks=[]
    def check(name,value):
        checks.append(dict(name=name,status='PASS' if value else 'FAIL'))
        write(WORK/'online-verification.json',dict(commit=commit,checks=checks))
        assert value,name
    repo=json.loads(run(GH,'api','repos/'+REPO))
    check('repository is public and default main',repo['visibility']=='public' and repo['default_branch']=='main')
    branch=json.loads(run(GH,'api','repos/'+REPO+'/branches/main'))
    check('online main is exact source commit',branch['commit']['sha']==commit)
    tag=json.loads(run(GH,'api','repos/'+REPO+'/git/ref/tags/v3.6.3'))['object']
    if tag['type']=='tag':tag=json.loads(run(GH,'api','repos/'+REPO+'/git/tags/'+tag['sha']))['object']
    check('release tag points at exact source commit',tag['sha']==commit)
    tree=json.loads(run(GH,'api','repos/'+REPO+'/git/trees/'+commit+'?recursive=1'))
    remote={x['path']:x['sha'] for x in tree['tree'] if x['type']=='blob'}
    local={line.split('\t',1)[1]:line.split()[2] for line in run('git','ls-tree','-r',commit).splitlines()}
    check('all online source/document blobs match local commit',not tree['truncated'] and remote==local)
    raw=subprocess.check_output([GH,'api','repos/'+REPO+'/contents/DEVELOPMENT_STANDARDS.md?ref='+commit,'-H','Accept: application/vnd.github.raw+json'],cwd=ROOT)
    check('online development standards match local bytes',raw==(ROOT/'DEVELOPMENT_STANDARDS.md').read_bytes())
    release=json.loads(run(GH,'api','repos/'+REPO+'/releases/tags/v3.6.3'))
    latest=json.loads(run(GH,'api','repos/'+REPO+'/releases/latest'))
    check('release is public/latest, not draft/prerelease',not release['draft'] and not release['prerelease'] and latest['tag_name']=='v3.6.3')
    assets={x['name']:x for x in release['assets']}
    wanted={p.name:p for p in DIST.iterdir() if p.is_file()}
    check('release asset list complete',set(wanted)==set(assets))
    download=WORK/'download-check'
    download.mkdir(exist_ok=False)
    run(GH,'release','download','v3.6.3','--repo',REPO,'--dir',str(download))
    for name,p in wanted.items():
        check(name+' size and downloaded SHA256',assets[name]['size']==p.stat().st_size and sha(download/name)==sha(p))
        if assets[name].get('digest'):check(name+' server digest',assets[name]['digest']=='sha256:'+sha(p))
    for row in manifest['assets']:check(row['name']+' manifest bytes',sha(download/row['name'])==row['sha256'])
    print(json.dumps(dict(status='PASS',checks=len(checks),commit=commit,release=release['html_url']),ensure_ascii=False))

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--commit',default='HEAD');parser.add_argument('--verify',action='store_true');args=parser.parse_args()
    verify() if args.verify else package(args.commit)
