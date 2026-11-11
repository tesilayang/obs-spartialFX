from pathlib import Path
source=Path('tools/portable-msvc.py').read_text(encoding='utf-8-sig')
needle='for pkg in sorted(msvc_packages):'
replacement='''# Workspace bootstrap: omit ASAN/PGO/UWP/redist/source packages not used by this plugin.
msvc_packages = [p for p in msvc_packages if not any(k in p for k in ('.asan.', '.pgo.', '.premium.', '.store.', '.redist.', '.crt.source.'))]
import concurrent.futures
def prefetch_package(pkg):
  if pkg not in packages: return
  p = first(packages[pkg], lambda p: p.get('language') in (None, 'en-US'))
  for payload in p['payloads']:
    download_progress(payload['url'], payload['sha256'], payload['fileName'])
with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
  list(pool.map(prefetch_package, sorted(msvc_packages)))
for pkg in sorted(msvc_packages):'''
source=source.replace(needle,replacement)
needle='  # download .cab files\n  for pkg in cabs:'
replacement='''  # Download independent SDK cabinet payloads concurrently.
  def prefetch_cab(pkg):
    payload = first(sdk_pkg['payloads'], lambda p: p['fileName'].replace('\\\\', '/') == f'Installers/{pkg}')
    download_progress(payload['url'], payload['sha256'], pkg)
  with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
    list(pool.map(prefetch_cab, sorted(set(cabs))))
  # download .cab files
  for pkg in cabs:'''
source=source.replace(needle,replacement)
Path('tools/portable-msvc-minimal.py').write_text(source,encoding='utf-8')
