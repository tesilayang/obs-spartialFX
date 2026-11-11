"""Reproducible public upstream inputs. Requires Python 3.12+, Git, VS2022 C++."""
from pathlib import Path
import argparse,hashlib,json,subprocess,tarfile,urllib.request,zipfile
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--verify-only',action='store_true');args=parser.parse_args()
manifest=json.loads((ROOT/'tools/dependencies.json').read_text(encoding='utf-8'))
third=ROOT/'third_party';third.mkdir(exist_ok=True)
for entry in manifest['files']:
 archive=third/entry['file']
 if not archive.exists():
  if args.verify_only:raise SystemExit('Missing '+str(archive))
  request=urllib.request.Request(entry['url'],headers={'User-Agent':'OBS-Spatial-FX-build','Accept':'application/octet-stream'})
  partial=archive.with_suffix('.partial')
  with urllib.request.urlopen(request,timeout=120) as response,partial.open('wb') as out:
   while block:=response.read(1024*1024):out.write(block)
  partial.replace(archive)
 actual=hashlib.sha256(archive.read_bytes()).hexdigest()
 if actual!=entry['sha256']:raise SystemExit('SHA256 MISMATCH: '+str(archive))
 print('PASS SHA256',entry['name'],actual,flush=True)
 if not args.verify_only:
  folder=third/entry['folder'];folder.mkdir(exist_ok=True)
  if entry['archive']=='zip':
   with zipfile.ZipFile(archive) as z:z.extractall(folder)
  else:
   with tarfile.open(archive) as t:t.extractall(folder,filter='data')
template=third/'obs-plugintemplate'
if not template.exists() and not args.verify_only:
 subprocess.run(['git','-c','http.sslBackend=openssl','clone','https://github.com/obsproject/obs-plugintemplate.git',str(template)],check=True)
 subprocess.run(['git','-C',str(template),'checkout',manifest['templateCommit']],check=True)
actual=subprocess.check_output(['git','-C',str(template),'rev-parse','HEAD'],text=True).strip()
if actual!=manifest['templateCommit']:raise SystemExit('Template revision mismatch')
print('PASS official template commit',actual)
