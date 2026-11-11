"""Prepare a legacy v1 public-settings fixture only in the workspace portable QA collection."""
import json,pathlib,shutil
root=pathlib.Path(__file__).resolve().parents[1]
p=root/'runtime/obs32/config/obs-studio/basic/scenes/Spatial_FX_QA.json'
shutil.copy2(p,root/'logs/v2/controlled-collection-before-migration.json')
d=json.loads(p.read_text(encoding='utf-8-sig'));r=json.loads((root/'logs/v2/gpu-private-retry/render-tests.json').read_text())
s=next(s for s in d['sources'] if s['name']==r['stage']);state=s['private_settings']['spatial_fx_model']
state['schema_version']=1;state.pop('animation_duration',None);state['playing']=False
state['keyframes']=[{'time':0,'camera_x':0,'camera_z':-1500,'camera_fov':40},{'time':3,'camera_x':200,'camera_z':-1400,'camera_fov':40}]
for l in state['layers']:l['z']=0;l['default_z']=0
s['settings']=state;s['private_settings'].pop('spatial_fx_model',None)
d['current_scene']=r['scene'];d['current_program_scene']=r['scene']
p.write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding='utf-8')
(root/'logs/v2/legacy-fixture-prepared.json').write_text(json.dumps({'stage':s['name'],'schema':state['schema_version'],'layers':len(state['layers']),'allZZero':all(l['z']==0 for l in state['layers']),'lastFrameTime':3,'privateModelAbsent':'spatial_fx_model' not in s['private_settings']},indent=2))
print('Prepared controlled v1 fixture:',s['name'])
