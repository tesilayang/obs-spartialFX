import json,pathlib,time,sys,statistics,base64
from obs_client import OBS
from native_control import LegacyNative as Native
out=pathlib.Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
for attempt in range(100):
 try:o=OBS();break
 except OSError:time.sleep(.3)
time.sleep(5)
scene=o.call('GetCurrentProgramScene')['currentProgramSceneName'];name=next(x['sourceName'] for x in o.call('GetSceneItemList',{'sceneName':scene})['sceneItems'] if x.get('inputKind')=='obs_spatial_fx_stage')
control=Native(out,name);original=control.get()
def update(p):control.update(p)
update({'schema_version':3,'fx_enabled':True,'timeline_preview':True,'timeline_position':0,'playing':True,'loop':True,'animation_duration':6,'keyframes':[{'time':0,'camera_x':-120,'camera_z':-1500,'camera_fov':40},{'time':6,'camera_x':120,'camera_z':-1300,'camera_fov':40}],'play_token':original.get('play_token',0)+10,'light_enabled':True,'shadows_enabled':True,'fx_vignette':.2});time.sleep(3)
start=o.call('GetStats');samples=[];begin=time.monotonic()
try:
 for i in range(60):
  # Repeated settings snapshots stress publication without GPU resource recreation.
  update({'camera_x':177+(i%9)*4});time.sleep(1)
  stats=o.call('GetStats');stats['elapsed']=time.monotonic()-begin;samples.append(stats)
  if i%10==9: print('Sample',i+1,'fps',stats['activeFps'],'render ms',stats['averageFrameRenderTime'],flush=True)
 end=o.call('GetStats');shot=o.call('GetSourceScreenshot',{'sourceName':name,'imageFormat':'png','imageWidth':1920,'imageHeight':1080});(out/'performance-render.png').write_bytes(base64.b64decode(shot['imageData'].split(',',1)[1]))
finally:
 update(original);o.close()
lag=end['renderSkippedFrames']-start['renderSkippedFrames'];frames=end['renderTotalFrames']-start['renderTotalFrames']
report={'durationSeconds':time.monotonic()-begin,'video':{'width':1920,'height':1080,'fps':60},'start':start,'end':end,'samples':samples,'frames':frames,'renderLagFrames':lag,'meanFps':statistics.mean(x['activeFps'] for x in samples),'maxReportedAverageRenderMs':max(x['averageFrameRenderTime'] for x in samples),'checks':[{'name':'at least 60 seconds measured','pass':time.monotonic()-begin>=60},{'name':'at least 3500 frames rendered','pass':frames>=3500},{'name':'render lag below 0.1 percent','pass':lag/max(1,frames)<.001}]}
report['failed']=sum(not c['pass'] for c in report['checks']);(out/'performance.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps({k:v for k,v in report.items() if k not in ['samples','start','end']},indent=2));sys.exit(bool(report['failed']))
