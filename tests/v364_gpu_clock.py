"""GPU clock determinism and temporal reset evidence for installed v3.6.4."""
import sys,time,json,copy,base64,io
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
from obs_client import OBS
from native_control import Native
bridge,out=map(Path,sys.argv[1:3]);out.mkdir(parents=True,exist_ok=True);o=OBS();stamp=str(time.time_ns());scene='V364 FX clock '+stamp;stage=scene+' stage';source=scene+' source';prior=o.call('GetCurrentProgramScene')['currentProgramSceneName'];checks=[]
def check(ok,name,value=None):checks.append(dict(status='PASS' if ok else 'FAIL',name=name,value=value));print(checks[-1],flush=True)
def delta(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
def image(label,m,t,playing=False):
 x=copy.deepcopy(m);x.update(timeline_position_us=str(round(t*1e6)),timeline_preview=True,playing=playing,play_token=time.time_ns()//1000);n.request(x);time.sleep(.15);d=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=960,imageHeight=540));raw=base64.b64decode(d['imageData'].split(',',1)[1]);(out/(label+'.png')).write_bytes(raw);return Image.open(io.BytesIO(raw)).convert('RGBA')
try:
 o.call('CreateScene',dict(sceneName=scene));o.call('CreateInput',dict(sceneName=scene,inputName=source,inputKind='image_source',inputSettings=dict(file=str(Path('work/v3.6.4/perf-fixture.png').resolve())),sceneItemEnabled=False));uid=next(s['inputUuid'] for s in o.call('GetInputList')['inputs'] if s['inputName']==source)
 base=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,fx_enabled=False,light_enabled=False,playing=False,timeline_preview=True,layers=[dict(uuid=uid,instance_id='clock-fixture',visible=True,enabled=True)])
 o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=base,sceneItemEnabled=True));o.call('SetCurrentProgramScene',dict(sceneName=scene));n=Native(bridge,stage)
 geometry=copy.deepcopy(base);geometry['layers'][0]['fx_stack']=dict(attached=True,group_states=[dict(enabled=i==10) for i in range(13)],effects=[dict(id='FX11.3',enabled=True,strength=1,parameters=dict(amplitude=.4,speed=1,phase=0))]);a=image('geometry-quarter',geometry,.25);b=image('geometry-threequarters',geometry,.75);c=image('geometry-quarter-restored',geometry,.25);check(delta(a,b)>1,'geometry procedural follows timeline time',delta(a,b));check(delta(a,c)<.01,'geometry returns to same pixels on backwards seek',delta(a,c))
 temporal=copy.deepcopy(base);temporal['layers'][0]['animation']=[dict(parameter='x',enabled=True,keys=[dict(time_us='0',value=-500,mode=0),dict(time_us='2000000',value=500,mode=0)])];temporal['layers'][0]['fx_stack']=dict(attached=True,group_states=[dict(enabled=i==0) for i in range(13)],effects=[dict(id='FX01.7',enabled=True,strength=1,parameters=dict(decay=.95,shift_x=-20,shift_y=0))]);image('history-playing',temporal,0,True);time.sleep(.5);a=image('history-after-seek',temporal,1.5);warm=n.get()['_metrics'];plain=copy.deepcopy(temporal);plain['layers'][0]['fx_stack']['effects'][0]['strength']=0;b=image('history-reference',plain,1.5);check(delta(a,b)<.01,'history clears across seek without prior-position ghost',delta(a,b));image('history-rewarm',temporal,.5);warm=n.get()['_metrics'];image('history-second-seek',temporal,1.5);after=n.get()['_metrics'];check(warm['allocations']==after['allocations'] and after['failures']==0,'seek invalidates history without GPU resource allocation',dict(warm=warm,after=after))
finally:
 o.call('SetCurrentProgramScene',dict(sceneName=prior))
 for name in [stage,source]:
  try:o.call('RemoveInput',dict(inputName=name))
  except Exception:pass
 o.call('RemoveScene',dict(sceneName=scene));o.close()
(out/'results.json').write_text(json.dumps(checks,indent=2),encoding='utf-8');sys.exit(any(c['status']=='FAIL' for c in checks))
