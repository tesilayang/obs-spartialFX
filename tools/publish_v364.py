"""Package immutable v3.6.4 Git sources; verify public GitHub assets."""
import argparse,hashlib,json,shutil,subprocess,zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'work/publish-v364';WORK.mkdir(parents=True,exist_ok=True)
DIST=ROOT/'dist/v3.6.4-release';OLD=ROOT/'dist/v3.6.4'
REPO='tesilayang/obs-spartialFX';TAG='v3.6.4'
GH=shutil.which('gh') or 'F:/Program Files (x86)/github/bin/gh.exe'
def raw(*args):return subprocess.check_output(args,cwd=ROOT)
def run(*args):return raw(*args).decode('utf-8').strip()
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def write(p,v):p.write_text(json.dumps(v,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest().upper()
def api(path):return json.loads(run(GH,'api',path))
def require(ok,msg):
 if not ok:raise RuntimeError(msg)
def package(commit):
 commit=run('git','rev-parse',commit)
 require(commit==run('git','rev-parse','HEAD'),'Package current committed sources')
 require(not DIST.exists(),'Preserve existing release directory')
 for line in (OLD/'SHA256SUMS.txt').read_text(encoding='utf-8').splitlines():
  digest,name=line.split('  ',1);require(sha(OLD/name)==digest,'Original delivery changed: '+name)
 provenance=read(OLD/'PAYLOAD_MANIFEST.json')
 require(provenance['installer']==sha(OLD/'OBS_Spatial_FX_V3.6.4_Setup.exe'),'Installer differs from verified delivery')
 require(provenance['dll']==sha(ROOT/'build/obs-spatial-fx.dll'),'Current DLL differs from verified delivery')
 installed=read(ROOT/'work/v3.6.4/final2-bridge/installed-path.json')
 require(installed['InstalledHash']==provenance['dll'],'Actual loaded DLL mismatch')
 require(read(ROOT/'work/v3.6.4/final2-acceptance/acceptance.json')['failed']==0,'Native acceptance failed')
 tests=(WORK/'release-tests.log').read_text(encoding='utf-8-sig')
 require('100% tests passed out of 10' in tests and '***Failed' not in tests and 'Exception:' not in tests,'Release CTest failure')
 with zipfile.ZipFile(OLD/'OBS_Spatial_FX_V3.6.4_Source.zip') as z:
  names=[n for n in z.namelist() if n.startswith(('src/','data/','installer/')) or n=='CMakeLists.txt']
  for name in names:
   require(z.read(name)==(ROOT/name).read_bytes(),'Validated source changed: '+name)
   require(run('git','hash-object','--path='+name,str(ROOT/name))==run('git','rev-parse',commit+':'+name),'Committed source mismatch: '+name)
 DIST.mkdir()
 shutil.copy2(OLD/'OBS_Spatial_FX_V3.6.4_Setup.exe',DIST/'OBS_Spatial_FX_V3.6.4_Setup.exe')
 subprocess.run(['git','archive','--format=zip','-o',str(DIST/'OBS_Spatial_FX_V3.6.4_Source.zip'),commit],cwd=ROOT,check=True)
 files=run('git','-c','core.quotepath=false','ls-tree','-r','--name-only',commit).splitlines()
 with zipfile.ZipFile(DIST/'OBS_Spatial_FX_V3.6.4_Source.zip') as z:
  require(z.testzip() is None,'Source ZIP CRC failed')
  require(set(files)=={n for n in z.namelist() if not n.endswith('/')},'Source ZIP file list differs from commit')
  for name in files:require(z.read(name)==raw('git','show',commit+':'+name),'Source ZIP blob mismatch: '+name)
 evidence={}
 with zipfile.ZipFile(OLD/'OBS_Spatial_FX_V3.6.4_Evidence.zip') as z:
  for name in z.namelist():
   allowed=(name.startswith(('final2-alpha/','final2-animation/','final2-clock/','final2-restart/')) and Path(name).suffix in {'.json','.png'})
   allowed|=name in {'final2-acceptance/acceptance.json','final2-spout/spout-results.json','final2-bridge/installed-path.json','final2-restart-bridge/installed-path.json','final2-bridge/obs-runtime.log','final2-restart-bridge/obs-runtime.log','hardware-cpu.json','hardware-gpu.json','cleanup.json','final-sender-discovery.json','payload-manifest.json','test-final2.log','build-final2.log','install-final2.log'}
   if allowed:evidence['validation/'+name]=z.read(name)
  evidence['validation/final2-performance/performance.json']=z.read('final2-performance/performance.json')
  evidence['validation/perf363/performance.json']=z.read('perf363/performance.json')
 evidence['publication/release-tests.log']=(WORK/'release-tests.log').read_bytes()
 evidence['publication/provenance.json']=json.dumps(dict(commit=commit,tag=TAG,installer=provenance['installer'],dll=provenance['dll'],sourceComparedFiles=len(names),liveNDI='BLOCKED'),indent=2).encode()
 with zipfile.ZipFile(DIST/'OBS_Spatial_FX_V3.6.4_Evidence.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for name,data in sorted(evidence.items()):z.writestr(name,data)
  z.writestr('EVIDENCE_MANIFEST.json',json.dumps({n:dict(size=len(b),sha256=hashlib.sha256(b).hexdigest().upper())for n,b in evidence.items()},indent=2))
 with zipfile.ZipFile(DIST/'OBS_Spatial_FX_V3.6.4_Evidence.zip') as z:require(z.testzip() is None,'Evidence CRC failed')
 for name in ['IMPLEMENTATION.md','TEST_REPORT.md','PUBLISH_REPORT.md']:
  shutil.copy2(ROOT/'docs/v3.6.4'/name,DIST/name)
 manifest=dict(version='3.6.4',tag=TAG,commit=commit,dll_sha256=provenance['dll'],installer_sha256=provenance['installer'],assets=[dict(name=p.name,bytes=p.stat().st_size,sha256=sha(p))for p in sorted(DIST.iterdir())])
 write(DIST/'RELEASE_MANIFEST.json',manifest)
 (DIST/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in sorted(DIST.iterdir())),encoding='utf-8')
 print(json.dumps(dict(status='PASS',directory=str(DIST),commit=commit,assets=len(list(DIST.iterdir())))))

def verify():
 manifest=read(DIST/'RELEASE_MANIFEST.json');commit=manifest['commit'];checks=[]
 def check(ok,name):
  checks.append(dict(name=name,status='PASS' if ok else 'FAIL'));write(WORK/'online-verification.json',dict(commit=commit,checks=checks));require(ok,name);print(checks[-1],flush=True)
 repo=api('repos/'+REPO);check(repo['visibility']=='public' and repo['default_branch']=='main','public repository and default branch')
 check(api('repos/'+REPO+'/branches/main')['commit']['sha']==commit,'main matches release commit')
 tag=api('repos/'+REPO+'/git/ref/tags/'+TAG)['object']
 if tag['type']=='tag':tag=api('repos/'+REPO+'/git/tags/'+tag['sha'])['object']
 check(tag['sha']==commit,'tag matches source commit')
 tree=api('repos/'+REPO+'/git/trees/'+commit+'?recursive=1')
 online={x['path']:x['sha']for x in tree['tree']if x['type']=='blob'}
 local={line.split('\t',1)[1]:line.split()[2]for line in run('git','-c','core.quotepath=false','ls-tree','-r',commit).splitlines()}
 check(not tree['truncated'] and online==local,'all remote source blobs match commit')
 body=raw(GH,'api','repos/'+REPO+'/contents/DEVELOPMENT_STANDARDS.md?ref='+commit,'-H','Accept: application/vnd.github.raw+json')
 check(body==raw('git','show',commit+':DEVELOPMENT_STANDARDS.md'),'online development standards match')
 release=api('repos/'+REPO+'/releases/tags/'+TAG);latest=api('repos/'+REPO+'/releases/latest')
 check(not release['draft'] and not release['prerelease'] and latest['tag_name']==TAG,'release published and latest')
 check(release['body'].strip()==(ROOT/'docs/v3.6.4/RELEASE_NOTES.md').read_text(encoding='utf-8').strip(),'release notes contain approved changes only')
 wanted={p.name:p for p in DIST.iterdir() if p.is_file()};online={a['name']:a for a in release['assets']}
 check(set(wanted)==set(online),'release asset names match')
 for name,p in wanted.items():
  a=online[name];check(a['size']==p.stat().st_size and a.get('digest','').lower()=='sha256:'+sha(p).lower(),'online size and SHA256 '+name)
 downloaded=WORK/'downloaded'
 subprocess.run([GH,'release','download',TAG,'--repo',REPO,'--dir',str(downloaded),'--clobber'],cwd=ROOT,check=True)
 for name,p in wanted.items():check(sha(downloaded/name)==sha(p),'downloaded SHA256 '+name)
 print(release['html_url'])

if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--package',action='store_true');parser.add_argument('--commit',default='HEAD');parser.add_argument('--verify',action='store_true');args=parser.parse_args()
 if args.package:package(args.commit)
 if args.verify:verify()
