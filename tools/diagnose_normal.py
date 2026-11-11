import json,pathlib,os
p=pathlib.Path('logs/phase6-installed-normal/render-probe.json');d=json.loads(p.read_text());print('layers',d['settings']['layers']);print('video',d['video'])
p=pathlib.Path('runtime/obs32/config/obs-studio/basic/scenes/Spatial_FX_QA.json');d=json.loads(p.read_text(encoding='utf-8-sig'))
for s in d['sources']:
 if s['id']=='image_source':
  f=s['settings'].get('file');print(s['name'],repr(f),os.path.exists(f) if f else False)
