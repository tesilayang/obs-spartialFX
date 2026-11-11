import pathlib,sys,zipfile,json
sys.argv=['portable-msvc.py','--vs','2022','--msvc-version','14.44','--sdk-version','22621','--accept-license']
source=pathlib.Path('tools/portable-msvc.py').read_text(encoding='utf-8-sig')
context={}
exec(source[:source.index('### download MSVC')],context)
pathlib.Path('tools/vsmanifest.json').write_text(json.dumps(context['vsmanifest']))
pkg='microsoft.vc.'+context['msvc_ver']+'.crt.x64.store.base'
for payload in context['packages'][pkg][0]['payloads']:
    print('RUNTIME LIB PACKAGE',payload['fileName'],payload['size'],flush=True)
    context['download_progress'](payload['url'],payload['sha256'],payload['fileName'])
    with zipfile.ZipFile(pathlib.Path('downloads')/payload['fileName']) as z:
        for name in z.namelist():
            if name.startswith('Contents/'):
                dest=pathlib.Path('msvc')/pathlib.Path(name).relative_to('Contents');dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(z.read(name))
print('RUNTIME LIBS READY',flush=True)
