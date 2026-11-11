import base64,json,pathlib,time,sys
from PIL import Image,ImageChops,ImageStat
from obs_client import OBS
output=pathlib.Path(sys.argv[1]);output.mkdir(parents=True,exist_ok=True)
for attempt in range(100):
 try:o=OBS();break
 except OSError:time.sleep(.3)
time.sleep(6)
scene=o.call('GetCurrentProgramScene')['currentProgramSceneName']
name=next(x['sourceName'] for x in o.call('GetSceneItemList',{'sceneName':scene})['sceneItems'] if x.get('inputKind')=='obs_spatial_fx_stage')
original=o.call('GetInputSettings',{'inputName':name})['inputSettings'];checks=[]
def update(p):o.call('SetInputSettings',{'inputName':name,'inputSettings':p,'overlay':True});time.sleep(.22)
def shot(file):
 d=o.call('GetSourceScreenshot',{'sourceName':name,'imageFormat':'png','imageWidth':1920,'imageHeight':1080});p=output/file;p.write_bytes(base64.b64decode(d['imageData'].split(',',1)[1]));return Image.open(p).convert('RGB')
def check(c,label,value=None):
 checks.append({'name':label,'pass':bool(c),'value':value});print(('PASS ' if c else 'FAIL ')+label,flush=True)
def diff(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
try:
 update({'playing':False,'camera_x':177,'light_enabled':False,'shadows_enabled':False,'fx_saturation':1,'fx_exposure':0})
 a=shot('unlit.png')
 update({'light_enabled':True,'light_ambient':.25,'light_intensity':2});b=shot('point-light.png');v=diff(a,b);check(v>.5,'point light changes actual pixels',v)
 update({'shadows_enabled':True,'shadow_opacity':0});a=shot('receiver-no-shadow.png')
 update({'shadow_opacity':.8});b=shot('alpha-shadow.png');v=diff(a,b);check(v>.1,'alpha shadow changes receiver pixels',v)
 update({'fx_saturation':0,'fx_exposure':1});a=shot('fx-monochrome.png');v=diff(a,b);check(v>.5,'FX exposure and saturation change output',v)
 update({'playing':True,'loop':True,'play_token':original.get('play_token',0)+3});a=shot('animation-a.png');time.sleep(1.0);b=shot('animation-b.png');v=diff(a,b);check(v>.3,'keyframe animation advances rendered camera',v)
 update({'playing':False,'layers':[dict(l,uuid='00000000-0000-0000-0000-000000000001') for l in original['layers']]});a=shot('deleted-source-safe.png');check(a.size==(1920,1080),'missing sources render safely without crash')
 update(original);a=shot('restored.png');check(len(a.getcolors(1920*1080) or [])>20,'source associations recover after updates')
finally:
 update(original);o.close()
report={'checks':checks,'failed':sum(not c['pass'] for c in checks)};(output/'features.json').write_text(json.dumps(report,indent=2),encoding='utf-8');sys.exit(bool(report['failed']))
