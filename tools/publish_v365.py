"""Package immutable v3.6.5 Git sources; verify public GitHub assets."""
import argparse,hashlib,json,shutil,subprocess,zipfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'work/publish-v365';WORK.mkdir(parents=True,exist_ok=True)
DIST=ROOT/'dist/v3.6.5'
REPO='tesilayang/obs-spartialFX';TAG='v3.6.5'
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
 require(not DIST.exists(),'Preserve existing delivery directory')
 proof=read(ROOT/'work/v3.6.5/payload-manifest.json')
 for name,digest in proof['sources'].items():
  require(sha(ROOT/name)==digest,'Validated source changed: '+name)
  require(raw('git','show',commit+':'+name)==(ROOT/name).read_bytes(),'Committed product differs: '+name)
 require(proof['dll']==sha(ROOT/'build/obs-spatial-fx.dll'),'DLL differs')
 require(proof['installer']==sha(ROOT/'build/OBS_Spatial_FX_V3.6.5_Setup.exe'),'Installer differs')
 require(read(ROOT/'work/v3.6.5/delivery-bridge/installed-path.json')['InstalledHash']==proof['dll'],'Loaded payload differs')
 require(read(ROOT/'work/v3.6.5/delivery-acceptance/acceptance.json')['failed']==0,'Native acceptance failed')
 require(read(ROOT/'work/v3.6.5/cleanup.json')['status']=='PASS','QA cleanup failed')
 require('100% tests passed out of 4' in (ROOT/'work/v3.6.5/tests-delivery.log').read_text(encoding='utf-8-sig'),'CTest failed')
 DIST.mkdir()
 shutil.copy2(ROOT/'build/OBS_Spatial_FX_V3.6.5_Setup.exe',DIST/'OBS_Spatial_FX_V3.6.5_Setup.exe')
 subprocess.run(['git','archive','--format=zip','-o',str(DIST/'OBS_Spatial_FX_V3.6.5_Source.zip'),commit],cwd=ROOT,check=True)
 files=run('git','-c','core.quotepath=false','ls-tree','-r','--name-only',commit).splitlines()
 with zipfile.ZipFile(DIST/'OBS_Spatial_FX_V3.6.5_Source.zip') as z:
  require(z.testzip() is None,'Source ZIP CRC failed')
  require(set(files)=={n for n in z.namelist() if not n.endswith('/')},'Source ZIP file list differs')
  for name in files:require(z.read(name)==raw('git','show',commit+':'+name),'Source ZIP blob mismatch: '+name)
 evidence={};work=ROOT/'work/v3.6.5'
 selected=['build-baseline.log','tests-baseline.log','build-fix.log','tests-fix.log','tests-final.log','tests-trace.log','tests-delivery.log','build-delivery.log','build-package.log','install-delivery.log','install-package.log','cleanup.json','payload-manifest.json','external-typing.json','delivery-bridge/installed-path.json','delivery-bridge/dock-smoke.json','delivery-acceptance.log','acceptance-console.log','acceptance2-console.log','acceptance3-console.log']
 for name in selected:evidence['validation/'+name]=(work/name).read_bytes()
 for folder in ['delivery-acceptance']:
  for p in (work/folder).iterdir():
   if p.suffix in {'.json','.png'}:evidence['validation/'+folder+'/'+p.name]=p.read_bytes()
 for p in (work/'delivery-bridge').glob('*.png'):evidence['validation/delivery-bridge/'+p.name]=p.read_bytes()
 # Publish the relevant runtime lines without unrelated user scene/source logs.
 runtime=(work/'delivery-bridge/obs-runtime.log').read_text(encoding='utf-8-sig',errors='replace')
 evidence['validation/obs-runtime-excerpt.log']='\n'.join(line for line in runtime.splitlines() if any(key in line for key in ['[SpatialFX','OBS 33.0.1','==== Shutting down','Number of memory leaks:','video settings reset','base resolution:','output resolution:','fps:'])).encode('utf8')
 with zipfile.ZipFile(DIST/'OBS_Spatial_FX_V3.6.5_Evidence.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for name,data in sorted(evidence.items()):z.writestr(name,data)
  z.writestr('EVIDENCE_MANIFEST.json',json.dumps({n:dict(size=len(b),sha256=hashlib.sha256(b).hexdigest().upper())for n,b in evidence.items()},indent=2))
 for name in ['IMPLEMENTATION.md','TEST_REPORT.md','RELEASE_NOTES.md','PUBLISH_REPORT.md']:
  shutil.copy2(ROOT/'docs/v3.6.5'/name,DIST/name)
 shutil.copy2(work/'payload-manifest.json',DIST/'PAYLOAD_MANIFEST.json')
 write(DIST/'RELEASE_MANIFEST.json',dict(version='3.6.5',tag=TAG,commit=commit,dll_sha256=proof['dll'],installer_sha256=proof['installer'],assets=[dict(name=p.name,bytes=p.stat().st_size,sha256=sha(p))for p in sorted(DIST.iterdir())]))
 (DIST/'SHA256SUMS.txt').write_text(''.join(sha(p)+'  '+p.name+'\n' for p in sorted(DIST.iterdir())),encoding='utf8')
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
 check(release['body'].strip()==(ROOT/'docs/v3.6.5/RELEASE_NOTES.md').read_text(encoding='utf-8').strip(),'release notes contain approved changes only')
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
