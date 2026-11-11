import json, pathlib, concurrent.futures, urllib.request, zipfile
import fetch_dependencies as original
ROOT=original.ROOT

def run(kind):
    if kind=='ninja':
        release=json.load(urllib.request.urlopen(urllib.request.Request('https://api.github.com/repos/ninja-build/ninja/releases/tags/v1.13.1',headers=original.UA),timeout=30))
        asset=next(a for a in release['assets'] if a['name']=='ninja-win.zip')
        folder=ROOT/'third_party/ninja'
    else:
        release=json.loads((ROOT/('tools/cmake-release.json' if kind=='cmake' else 'tools/qt-release.json')).read_text(encoding='utf-8-sig'))
        asset=next(a for a in release['assets'] if (a['name'].endswith('windows-x86_64.zip') if kind=='cmake' else a['name']=='windows-deps-qt6-2025-07-11-x64.zip'))
        folder=ROOT/'third_party'/kind
    original.UA['Accept']='application/octet-stream'
    dest=original.fetch(asset['url']+'?download=1',ROOT/'third_party'/('api-'+asset['name']))
    import hashlib
    expected=asset.get('digest','').replace('sha256:','')
    if kind=='qt6': expected='0e76bf0555dd5382838850b748d3dcfab44a1e1058441309ab54e1a65b156d0a'
    if expected: assert hashlib.sha256(dest.read_bytes()).hexdigest()==expected
    with zipfile.ZipFile(dest) as z: z.extractall(folder)
    print('READY',kind,flush=True)
with concurrent.futures.ThreadPoolExecutor(max_workers=3) as pool: list(pool.map(run,['cmake','ninja','qt6']))
