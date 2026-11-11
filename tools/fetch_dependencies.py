"""Fetch upstream build inputs into this workspace; no global installation."""
import concurrent.futures, hashlib, json, pathlib, tarfile, time, urllib.request, zipfile
ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / 'third_party'
UA = {'User-Agent': 'OBS-Spatial-FX-build'}
def fetch(url, dest):
    dest = pathlib.Path(dest)
    if dest.exists() and dest.stat().st_size > 1000: return dest
    for attempt in range(4):
        try:
            print('DOWNLOAD', url, flush=True)
            with urllib.request.urlopen(urllib.request.Request(url, headers=UA),timeout=120) as response, dest.with_suffix('.partial').open('wb') as output:
                while block := response.read(1024*1024): output.write(block)
            dest.with_suffix('.partial').replace(dest)
            print('FETCHED',dest.name,dest.stat().st_size,'sha256',hashlib.sha256(dest.read_bytes()).hexdigest(),flush=True)
            return dest
        except Exception as error:
            print('RETRY',attempt,str(error),flush=True)
            if attempt == 3: raise
            time.sleep(2)
def job(kind):
    if kind == 'cmake':
        release=json.loads((ROOT/'tools/cmake-release.json').read_text(encoding='utf-8-sig'))
        asset=next(a for a in release['assets'] if a['name'].endswith('windows-x86_64.zip'))
        archive=fetch(asset['browser_download_url'],OUT/asset['name'])
        assert 'sha256:'+hashlib.sha256(archive.read_bytes()).hexdigest()==asset['digest']
        with zipfile.ZipFile(archive) as z: z.extractall(OUT/'cmake')
    elif kind == 'qt':
        release=json.loads((ROOT/'tools/qt-release.json').read_text(encoding='utf-8-sig'))
        asset=next(a for a in release['assets'] if a['name']=='windows-deps-qt6-2025-07-11-x64.zip')
        archive=fetch(asset['browser_download_url'],OUT/asset['name'])
        assert hashlib.sha256(archive.read_bytes()).hexdigest()=='0e76bf0555dd5382838850b748d3dcfab44a1e1058441309ab54e1a65b156d0a'
        with zipfile.ZipFile(archive) as z: z.extractall(OUT/'qt6')
    elif kind == 'obs':
        archive=fetch('https://codeload.github.com/obsproject/obs-studio/tar.gz/refs/tags/32.2.2',OUT/'obs-32.2.2.tar.gz')
        with tarfile.open(archive) as t: t.extractall(OUT,filter='data')
    elif kind == 'ninja':
        archive=fetch('https://github.com/ninja-build/ninja/releases/download/v1.13.1/ninja-win.zip',OUT/'ninja-win.zip')
        with zipfile.ZipFile(archive) as z: z.extractall(OUT/'ninja')
    print('READY',kind,flush=True)
if __name__=='__main__':
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: list(pool.map(job,['cmake','qt','obs','ninja']))
