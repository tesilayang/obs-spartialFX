"""Measure the actually installed schema4 plugin, real OBS frames and native publication."""
import base64,json,pathlib,statistics,sys,time
from obs_client import OBS
from native_control import Native
out=pathlib.Path(sys.argv[1]).resolve();seconds=int(sys.argv[2]) if len(sys.argv)>2 else 1800
o=OBS();fixture=json.loads((out/'camera-render-tests.json').read_text());stage=fixture['stage'];n=Native(out,stage);original=n.get()
video=o.call('GetVideoSettings');start=o.call('GetStats');samples=[];updates=[];begin=time.monotonic()
try:
 for i in range(seconds):
  t=time.monotonic();n.update({'light_x':-500+(i%41)*10});updates.append((time.monotonic()-t)*1000)
  time.sleep(max(0,1-(time.monotonic()-t)));stats=o.call('GetStats');stats['elapsed']=time.monotonic()-begin;samples.append(stats)
  if (i+1)%60==0:
   (out/'stability-progress.json').write_text(json.dumps({'seconds':i+1,'fps':stats['activeFps'],'renderMs':stats['averageFrameRenderTime'],'memoryMiB':stats['memoryUsage']}),encoding='utf-8')
   print('seconds',i+1,'fps',stats['activeFps'],'renderMs',stats['averageFrameRenderTime'],'memoryMiB',stats['memoryUsage'],flush=True)
 end=o.call('GetStats');data=o.call('GetSourceScreenshot',{'sourceName':stage,'imageFormat':'png','imageWidth':1920,'imageHeight':1080});(out/'stability-final.png').write_bytes(base64.b64decode(data['imageData'].split(',',1)[1]))
finally:
 n.update({'playing':False});o.close()
frames=end['renderTotalFrames']-start['renderTotalFrames'];lag=end['renderSkippedFrames']-start['renderSkippedFrames'];duration=time.monotonic()-begin
checks=[{'name':'measured requested duration','pass':duration>=seconds},{'name':'actual1080p60 configuration','pass':video['baseWidth']==1920 and video['baseHeight']==1080 and video['fpsNumerator']/video['fpsDenominator']==60},{'name':'render lag below0.1percent','pass':lag/max(1,frames)<.001},{'name':'real frame count','pass':frames>=seconds*58}]
report={'checks':checks,'failed':sum(not c['pass'] for c in checks),'durationSeconds':duration,'video':video,'start':start,'end':end,'frames':frames,'renderLagFrames':lag,'meanFps':statistics.mean(x['activeFps'] for x in samples),'maxReportedAverageRenderMs':max(x['averageFrameRenderTime'] for x in samples),'memoryGrowthMiB':end['memoryUsage']-start['memoryUsage'],'publicationRoundTripP95Ms':sorted(updates)[int(len(updates)*.95)],'samples':samples}
(out/'stability.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print({k:v for k,v in report.items() if k not in ['start','end','samples']});raise SystemExit(bool(report['failed']))
