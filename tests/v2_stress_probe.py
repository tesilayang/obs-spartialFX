import sys,json,pathlib,time
from obs_client import OBS
out=pathlib.Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
for i in range(100):
 try:o=OBS();break
 except OSError:time.sleep(.3)
time.sleep(5)
scene=o.call('GetCurrentProgramScene')['currentProgramSceneName'];stage=next(i['sourceName'] for i in o.call('GetSceneItemList',{'sceneName':scene})['sceneItems'] if i.get('inputKind')=='obs_spatial_fx_stage');begin=time.monotonic();count=0;error=None
try:
 for i in range(400):
  o.call('SetInputSettings',{'inputName':stage,'inputSettings':{'light_range':2300+i%25},'overlay':True});d=o.call('GetInputSettings',{'inputName':stage});assert d['inputSettings']['layers'];count+=1;time.sleep(.15)
  if count%100==0:print('Concurrent external updates',count,'elapsed',round(time.monotonic()-begin,1),flush=True)
except Exception as e:error=str(e)
finally:
 try:o.close()
 except OSError:pass
report={'externalUpdates':count,'settingsReads':count,'durationSeconds':time.monotonic()-begin,'error':error,'failed':int(count!=400 or error is not None)};(out/'concurrent-settings.json').write_text(json.dumps(report,indent=2));print(json.dumps(report),flush=True);sys.exit(bool(report['failed']))
