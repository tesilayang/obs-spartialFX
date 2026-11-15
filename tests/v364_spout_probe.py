import sys,time,json,base64,math
from pathlib import Path
sys.path.insert(0,str(Path.cwd()/'tests'))
from obs_client import OBS
from PIL import Image
out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True);o=OBS();stamp=str(time.time_ns());scene='Alpha VTube '+stamp;name='Alpha Spout '+stamp;stage='Alpha Spout stage '+stamp;prior=o.call('GetCurrentProgramScene')['currentProgramSceneName'];o.call('CreateScene',dict(sceneName=scene))
try:
 created=o.call('CreateInput',dict(sceneName=scene,inputName=name,inputKind='spout_capture',inputSettings=dict(spoutsenders='VTubeStudioSpout',compositemode=2),sceneItemEnabled=True));o.call('SetCurrentProgramScene',dict(sceneName=scene));time.sleep(2)
 transform=o.call('GetSceneItemTransform',dict(sceneName=scene,sceneItemId=created['sceneItemId']))['sceneItemTransform'];w=int(transform['sourceWidth']);h=int(transform['sourceHeight']);print('sender',w,h)
 uid=next(i['inputUuid'] for i in o.call('GetInputList')['inputs'] if i['inputName']==name)
 model=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,fx_enabled=False,light_enabled=False,timeline_preview=False,playing=False,layers=[dict(uuid=uid,instance_id='spout-alpha',label=name,sx=1500/(1080/(2*math.tan(math.radians(20)))),sy=1500/(1080/(2*math.tan(math.radians(20)))),visible=True,enabled=True)])
 o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=model,sceneItemEnabled=False));time.sleep(.5);result=dict(sender=dict(width=w,height=h),mode=2,lights=False,fx=False,frames=[])
 for source,label in [(scene,'direct-scene'),(stage,'spatial')]:
  d=o.call('GetSourceScreenshot',dict(sourceName=source,imageFormat='png',imageWidth=1920,imageHeight=1080));p=out/(label+'.png');p.write_bytes(base64.b64decode(d['imageData'].split(',',1)[1]));im=Image.open(p).convert('RGBA');pixels=list(im.getdata());partial=[p for p in pixels if 0<p[3]<255];bad=sum(max(p[:3])>p[3]+3 for p in partial);result['frames'].append(dict(source=label,partial_pixels=len(partial),rgb_exceeds_alpha=bad,max_excess=max((max(p[:3])-p[3] for p in partial),default=0)))
  from native_control import Native
 n=Native(Path(sys.argv[2]),stage);matrix=[]
 for label,scaleFactor,angle,opacity,lit in [('scale-small',.73,0,1,False),('scale-large',1.37,0,1,False),('rotate',1,17,1,False),('half-opacity',1,0,.5,False),('lighting',1,0,1,True)]:
  import copy
  m=copy.deepcopy(model);m['layers'][0]['sx']*=scaleFactor;m['layers'][0]['sy']*=scaleFactor;m['layers'][0]['rz']=angle;m['layers'][0]['opacity']=opacity;m['fx_enabled']=lit;m['light_enabled']=lit;m['light_rim']=0
  n.request(m);time.sleep(.3);d=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=1920,imageHeight=1080));p=out/(label+'.png');p.write_bytes(base64.b64decode(d['imageData'].split(',',1)[1]));im=Image.open(p).convert('RGBA');partial=[p for p in im.getdata() if 0<p[3]<255];bad=sum(max(p[:3])>p[3]+3 for p in partial);matrix.append(dict(case=label,partial_pixels=len(partial),rgb_exceeds_alpha=bad,status='PASS' if partial and (lit or bad==0) else 'FAIL',note='lighting may intentionally exceed alpha' if lit else 'unlit premult bound'))
  for bg in [(0,0,0),(255,255,255),(30,120,220)]:
   import numpy as np
   arr=np.asarray(im).astype(float);rgb=np.clip(arr[:,:,:3]+np.array(bg)*(1-arr[:,:,3:4]/255),0,255).astype('uint8');Image.fromarray(rgb).save(out/(label+'-bg-'+str(bg[0])+'.png'))
 result['matrix']=matrix
 (out/'spout-results.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
finally:
 o.call('SetCurrentProgramScene',dict(sceneName=prior))
 for n in [stage,name]:
  try:o.call('RemoveInput',dict(inputName=n))
  except Exception:pass
 o.call('RemoveScene',dict(sceneName=scene))
