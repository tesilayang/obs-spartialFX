import sys,time,json,base64,math,copy
from pathlib import Path
sys.path.insert(0,str(Path.cwd()/'tests'))
from obs_client import OBS
from native_control import Native
from PIL import Image,ImageChops,ImageStat
out=Path(sys.argv[2]);out.mkdir(parents=True,exist_ok=True);o=OBS();stamp=str(time.time_ns());scene='Animation GPU '+stamp;source='Animation fixture '+stamp;stage='Animation stage '+stamp;prior=o.call('GetCurrentProgramScene')['currentProgramSceneName'];o.call('CreateScene',dict(sceneName=scene));checks=[]
def track(parameter,a,b):return dict(parameter=parameter,enabled=True,keys=[dict(time_us='0',value=a,mode=0),dict(time_us='1000000',value=b,mode=0)])
def check(ok,label,value):checks.append(dict(status='PASS' if ok else 'FAIL',name=label,value=value));print(checks[-1])
try:
 o.call('CreateInput',dict(sceneName=scene,inputName=source,inputKind='spatial_qa_alpha_fixture',inputSettings=dict(premultiplied=False),sceneItemEnabled=False));uid=next(i['inputUuid'] for i in o.call('GetInputList')['inputs'] if i['inputName']==source)
 scale=1500/(512/(2*math.tan(math.radians(20))))
 base=dict(schema_version=6,width=512,height=512,camera_z=-1500,camera_fov=40,fx_enabled=True,light_enabled=False,timeline_preview=True,playing=False,layers=[dict(uuid=uid,instance_id='animated',label=source,sx=scale,sy=scale,visible=True,enabled=True)])
 o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=base,sceneItemEnabled=True));o.call('SetCurrentProgramScene',dict(sceneName=scene));n=Native(Path(sys.argv[1]),stage)
 def shot(label,m,t):
  m=copy.deepcopy(m);m['timeline_position_us']=str(int(t*1000000));n.request(m);time.sleep(.25);d=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=512,imageHeight=512));p=out/(label+'.png');p.write_bytes(base64.b64decode(d['imageData'].split(',',1)[1]));return Image.open(p).convert('RGBA')
 light=copy.deepcopy(base);light.update(light_enabled=True,light_x=0,light_y=0,light_z=-500,light_range=3000,light_ambient=0,light_intensity=0,light_rim=0,shadows_enabled=False,light_animation=dict(animation=[track('intensity',0,2)]))
 a=shot('light-zero',light,0);b=shot('light-two',light,1);check(sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)>1,'light intensity animation changes actual GPU pixels',sum(ImageStat.Stat(ImageChops.difference(a,b)).mean))
 fx=copy.deepcopy(base);fx['layers'][0]['fx_stack']=dict(attached=True,group_states=[dict(enabled=i==2) for i in range(13)],effects=[dict(id='FX03.0',enabled=True,strength=1,parameters=dict(gamma=1,red=1,green=1,blue=1,exposure=0,saturation=1,contrast=1),animation=[track('red',1,0)])])
 a=shot('fx-red-one',fx,0);b=shot('fx-red-zero',fx,1);check(a.getpixel((362,256))[0]>100 and b.getpixel((362,256))[0]<3,'pixel FX parameter animation reaches renderer',[a.getpixel((362,256)),b.getpixel((362,256))])
 wide=copy.deepcopy(base);wide['layers'][0]['fx_stack']=dict(attached=True,group_states=[dict(enabled=i==12) for i in range(13)],effects=[dict(id='FX13.1',enabled=True,strength=1,parameters=dict(mode=0),animation=[track('strength',0,1)])])
 a=shot('wide-zero',wide,0);b=shot('wide-one',wide,1);check(a.getpixel((256,20))[3]==0 and b.getpixel((256,20))==(0,0,0,255),'stage FX strength animation reaches widescreen',[a.getpixel((256,20)),b.getpixel((256,20))])
finally:
 o.call('SetCurrentProgramScene',dict(sceneName=prior))
 for name in [stage,source]:
  try:o.call('RemoveInput',dict(inputName=name))
  except Exception:pass
 o.call('RemoveScene',dict(sceneName=scene))
(out/'results.json').write_text(json.dumps(checks,indent=2));sys.exit(any(c['status']=='FAIL' for c in checks))
