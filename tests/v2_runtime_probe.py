import base64,json,pathlib,time,sys
from PIL import Image,ImageChops,ImageStat
from obs_client import OBS
from native_control import Native
output=pathlib.Path(sys.argv[1]);output.mkdir(parents=True,exist_ok=True)
deadline=time.monotonic()+35
while True:
    try: o=OBS();break
    except (ConnectionError,OSError):
        if time.monotonic()>deadline:raise
        time.sleep(.3)
time.sleep(4)
scene=o.call('GetCurrentProgramScene')['currentProgramSceneName']
items=o.call('GetSceneItemList',{'sceneName':scene})['sceneItems']
name=next(x['sourceName'] for x in items if x.get('inputKind')=='obs_spatial_fx_stage')
control=Native(output,name);settings=control.get()
report={'scene':scene,'input':name,'settings':settings,'video':o.call('GetVideoSettings'),'stats':o.call('GetStats'),'checks':[]}
def check(condition,label):
    report['checks'].append({'name':label,'pass':bool(condition)});print(('PASS ' if condition else 'FAIL ')+label,flush=True)
def screenshot(filename):
    shot=o.call('GetSourceScreenshot',{'sourceName':name,'imageFormat':'png','imageWidth':1920,'imageHeight':1080})
    path=output/filename;path.write_bytes(base64.b64decode(shot['imageData'].split(',',1)[1]));return Image.open(path).convert('RGBA')
a=screenshot('stage-render.png')
check(a.getbbox() is not None,'real OBS stage screenshot has visible pixels')
check(len(a.convert('RGB').getcolors(1920*1080) or [])>20,'render contains textured color detail')
check(report['video']['baseWidth']==1920 and report['video']['baseHeight']==1080,'actual OBS canvas is 1920x1080')
check(report['video']['fpsNumerator']/report['video']['fpsDenominator']==60,'actual OBS video clock is 60fps')
check(settings.get('schema_version')==2,'native installed stage settings use schema2')
base_x=settings.get('camera_x',0)
control.update({'camera_x':base_x+160});time.sleep(.35)
b=screenshot('camera-parallax.png');difference=sum(ImageStat.Stat(ImageChops.difference(a.convert('RGB'),b.convert('RGB'))).mean)
check(difference>.3,'live camera change visibly alters projection and parallax')
control.update({'camera_x':base_x});time.sleep(.3)
report['cameraImageDifference']=difference
report['finalStats']=o.call('GetStats');report['failed']=sum(not c['pass'] for c in report['checks'])
(output/'render-probe.json').write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8')
o.close();print(json.dumps({'failed':report['failed'],'difference':difference},indent=2));sys.exit(bool(report['failed']))
