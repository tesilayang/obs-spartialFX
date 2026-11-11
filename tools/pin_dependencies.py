from pathlib import Path
import hashlib,json,urllib.request
root=Path('.')
entries=[]
for label,file,folder,api in [
 ('cmake','api-cmake-4.4.5-windows-x86_64.zip','cmake','tools/cmake-release.json'),
 ('qt6','api-windows-deps-qt6-2025-07-11-x64.zip','qt6','tools/qt-release.json')]:
 release=json.loads(Path(api).read_text(encoding='utf-8-sig'));asset=next(a for a in release['assets'] if a['name']==file[4:])
 entries.append(dict(name=label,file=file,url=asset['url']+'?download=1',sha256=hashlib.sha256((root/'third_party'/file).read_bytes()).hexdigest(),folder=folder,archive='zip'))
release=json.load(urllib.request.urlopen(urllib.request.Request('https://api.github.com/repos/ninja-build/ninja/releases/tags/v1.13.1',headers={'User-Agent':'OBS-Spatial-FX-build'}),timeout=30));asset=next(a for a in release['assets'] if a['name']=='ninja-win.zip')
entries.append(dict(name='ninja',file='api-ninja-win.zip',url=asset['url']+'?download=1',sha256=hashlib.sha256(Path('third_party/api-ninja-win.zip').read_bytes()).hexdigest(),folder='ninja',archive='zip'))
entries.append(dict(name='obs-32.0.0',file='obs-32.0.0.tar.gz',url='https://codeload.github.com/obsproject/obs-studio/tar.gz/refs/tags/32.0.0',sha256=hashlib.sha256(Path('third_party/obs-32.0.0.tar.gz').read_bytes()).hexdigest(),folder='',archive='tar'))
Path('tools/dependencies.json').write_text(json.dumps({'files':entries,'templateCommit':'3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc'},indent=2),encoding='utf-8')
print('Pinned',len(entries),'archives')
