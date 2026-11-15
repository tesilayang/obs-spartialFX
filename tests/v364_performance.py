"""Scoped 1080p60 comparison; new object tracks are ignored by baseline v3.6.3."""
import sys,time,json,copy,statistics,subprocess,base64
from pathlib import Path
from PIL import Image,ImageDraw
from obs_client import OBS
from native_control import Native
bridge,out,label=Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve(),sys.argv[3]
out.mkdir(parents=True,exist_ok=True)
root=Path(__file__).resolve().parents[1]
for attempt in range(100):
 try:o=OBS();break
 except OSError:time.sleep(.3)
ready=json.loads((bridge/'native-ready.json').read_text(encoding='utf-8-sig'));pid=ready['pid']
stamp=str(time.time_ns());scene='V364 perf '+stamp;source='V364 fixture '+stamp;stage='V364 perf stage '+stamp;prior=o.call('GetCurrentProgramScene')['currentProgramSceneName'];o.call('CreateScene',dict(sceneName=scene))
fixture=root/'work/v3.6.4/perf-fixture.png'
if not fixture.exists():
 im=Image.new('RGBA',(800,600));d=ImageDraw.Draw(im);d.rounded_rectangle((50,30,750,570),50,fill=(80,130,200,255));d.ellipse((220,120,600,500),fill=(230,120,70,255));im.save(fixture)
def track(p,a,b):return dict(parameter=p,enabled=True,keys=[dict(time_us='0',value=a,mode=0),dict(time_us='6000000',value=b,mode=0)])
rows=[]
try:
 o.call('CreateInput',dict(sceneName=scene,inputName=source,inputKind='image_source',inputSettings=dict(file=str(fixture)),sceneItemEnabled=False))
 uid=next(s['inputUuid'] for s in o.call('GetInputList')['inputs'] if s['inputName']==source)
 base=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,fx_enabled=False,light_enabled=False,playing=True,loop=True,timeline_preview=True,layers=[dict(uuid=uid,instance_id='perf-object',visible=True,enabled=True,animation=[track('x',-250,250)])])
 o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=base,sceneItemEnabled=True));o.call('SetCurrentProgramScene',dict(sceneName=scene));n=Native(bridge,stage)
 for case in ['baseline','light-animation','fx-animation','combined']:
  m=copy.deepcopy(base);m['play_token']=time.time_ns()//1000
  if case in ['light-animation','combined']:
   m.update(fx_enabled=True,light_enabled=True,light_present=True,light_instance_id='perf-light',light_z=-600,light_range=3000,light_intensity=1.5,light_ambient=.3,light_rim=.4,shadows_enabled=False,light_animation=dict(animation=[track('intensity',.5,2.5),track('x',-300,300)]))
  if case in ['fx-animation','combined']:
   m['layers'][0]['fx_stack']=dict(attached=True,group_states=[dict(enabled=i in [0,2]) for i in range(13)],effects=[dict(id='FX03.0',enabled=True,strength=1,parameters=dict(red=1,green=1,blue=1,gamma=1,exposure=0,saturation=1,contrast=1),animation=[track('red',.5,1)]),dict(id='FX01.7',enabled=True,strength=.4,parameters=dict(decay=.6,shift_x=2,shift_y=0))])
  n.request(m);time.sleep(3);warm=n.get().get('_metrics',{});before=o.call('GetStats');start=time.monotonic();samples=[]
  tag='v364-'+label+'-'+case
  sampler=subprocess.Popen(['powershell','-NoProfile','-File',str(root/'tests/v364_gpu_sample.ps1'),'-Label',tag,'-ProcessIds',str(pid),'-Seconds','30'],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
  for i in range(30):time.sleep(1);samples.append(o.call('GetStats'))
  text=sampler.communicate(timeout=30)[0].decode(errors='replace');counter=root/'tests'/(tag+'.json')
  if counter.exists():counter.replace(out/(tag+'.json'))
  after=n.get().get('_metrics',{});row=dict(case=case,versionLabel=label,elapsedSeconds=time.monotonic()-start,meanCPU=statistics.mean(x['cpuUsage'] for x in samples),meanFPS=statistics.mean(x['activeFps'] for x in samples),meanRenderMs=statistics.mean(x['averageFrameRenderTime'] for x in samples),skippedFrames=samples[-1]['renderSkippedFrames']-before['renderSkippedFrames'],totalFrames=samples[-1]['renderTotalFrames']-before['renderTotalFrames'],warm=warm,after=after,samples=samples,gpuSamplerExit=sampler.returncode,gpuSamplerLog=text)
  row['status']='PASS' if row['meanFPS']>59 and row['skippedFrames']==0 and warm['allocations']==after['allocations'] and after['failures']==0 and sampler.returncode==0 else 'FAIL';rows.append(row)
  (out/'performance.json').write_text(json.dumps(dict(version=o.call('GetVersion'),video=o.call('GetVideoSettings'),pid=pid,source=str(fixture),note='v3.6.3 ignores new light/FX animation channels; comparison measures added animation work, not identical changing images. GPU counters are process engine utilization; no power conclusion.',workloads=rows),indent=2),encoding='utf-8');print(json.dumps({k:v for k,v in row.items() if k not in ['samples','warm','after']}),flush=True)
finally:
 o.call('SetCurrentProgramScene',dict(sceneName=prior))
 for name in [stage,source]:
  try:o.call('RemoveInput',dict(inputName=name))
  except Exception:pass
 o.call('RemoveScene',dict(sceneName=scene));o.close()
sys.exit(any(r['status']!='PASS' for r in rows))
