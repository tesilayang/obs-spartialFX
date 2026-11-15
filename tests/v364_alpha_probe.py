import sys,time,json,base64,math,hashlib
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from obs_client import OBS
from native_control import Native
from PIL import Image
out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
o=OBS();stamp=str(time.time_ns());scene='Alpha QA '+stamp
prior=o.call('GetCurrentProgramScene')['currentProgramSceneName']
o.call('CreateScene',dict(sceneName=scene)); results=[]; created=[]
def shot(name,w,h):
 d=o.call('GetSourceScreenshot',dict(sourceName=name,imageFormat='png',imageWidth=w,imageHeight=h));p=out/(name+'.png');p.write_bytes(base64.b64decode(d['imageData'].split(',',1)[1]));return Image.open(p).convert('RGBA')
try:
 for premult in [False,True]:
  name=('premult' if premult else 'straight')+'-'+stamp
  o.call('CreateInput',dict(sceneName=scene,inputName=name,inputKind='spatial_qa_alpha_fixture',inputSettings=dict(premultiplied=premult),sceneItemEnabled=False));created.append(name)
  uid=next(i['inputUuid'] for i in o.call('GetInputList')['inputs'] if i['inputName']==name)
  focal=512/(2*math.tan(math.radians(20)));scale=1500/focal
  model=dict(schema_version=6,width=512,height=512,camera_z=-1500,camera_fov=40,fx_enabled=False,light_enabled=False,timeline_preview=False,playing=False,layers=[dict(uuid=uid,instance_id='alpha',label=name,sx=scale,sy=scale,visible=True,enabled=True)])
  stage='stage-'+name;o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=model,sceneItemEnabled=True));created.append(stage)
  o.call('SetCurrentProgramScene',dict(sceneName=scene));time.sleep(.7)
  direct=shot(name,256,256);image=shot(stage,512,512)
  samples=[]
  for x,a in [(26,0),(78,16),(130,64),(182,128),(234,255)]:
   px=image.getpixel((128+x,256));expected=tuple(round(v*a/255) for v in [204,102,51])+(a,)
   samples.append(dict(alpha=a,direct=direct.getpixel((x,128)),stage=px,expected=expected,ok=max(abs(u-v) for u,v in zip(px,expected))<=2))
  results.append(dict(premultiplied=premult,samples=samples))
  o.call('RemoveInput',dict(inputName=stage));created.remove(stage)
 kinds=o.call('GetInputKindList')['inputKinds'];(out/'protocol-kinds.json').write_text(json.dumps([k for k in kinds if 'spout' in k.lower() or 'ndi' in k.lower()],indent=2))
finally:
 o.call('SetCurrentProgramScene',dict(sceneName=prior))
 for n in created:o.call('RemoveInput',dict(inputName=n))
 o.call('RemoveScene',dict(sceneName=scene))
(out/'alpha-results.json').write_text(json.dumps(results,indent=2));print(json.dumps(results,indent=2))
sys.exit(0 if all(s['ok'] for r in results for s in r['samples']) else 1)
