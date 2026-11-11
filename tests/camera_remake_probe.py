"""Actual D3D11 schema4 camera/layer/composite/light coordinate comparisons."""
import base64, json, sys, time
from pathlib import Path
from PIL import Image, ImageChops, ImageStat
from obs_client import OBS
from native_control import Native

out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
assets=Path(__file__).resolve().parents[1]/'work/v3.2/gpu33'
o=OBS();checks=[];stamp=str(time.time_ns());scene='Spatial FX32 GPU '+stamp
for attempt in range(60):
 try:o.call('GetSceneList');break
 except RuntimeError:time.sleep(.3)
o.call('CreateScene',{'sceneName':scene});layers=[]
for i,filename in enumerate(['wall.png','actor.png','foreground.png']):
 name='SFX32 material '+str(i)+' '+stamp
 o.call('CreateInput',{'sceneName':scene,'inputName':name,'inputKind':'image_source','inputSettings':{'file':str(assets/filename)},'sceneItemEnabled':False})
 source=next(x for x in o.call('GetInputList')['inputs'] if x['inputName']==name)
 layers.append({'uuid':source['inputUuid'],'scene_uuid':scene,'item_id':i+1,'label':name,'x':0 if i==0 else (-150 if i==1 else 400),'y':0,'z':500 if i==0 else 0,'sx':1,'sy':1,'cast_shadow':i!=0,'animation':[]})
stage='Spatial FX32 Stage '+stamp
base={'schema_version':4,'width':1920,'height':1080,'fx_enabled':False,'layers':layers,'camera_x':0,'camera_y':0,'camera_z':-1500,'camera_rx':0,'camera_ry':0,'camera_rz':0,'camera_fov':40,'animation':[],'timeline_preview':True,'timeline_position_us':'0','playing':False,'use_legacy_playback_range':False}
o.call('CreateInput',{'sceneName':scene,'inputName':stage,'inputKind':'obs_spatial_fx_stage','inputSettings':base,'sceneItemEnabled':True});o.call('SetCurrentProgramScene',{'sceneName':scene});time.sleep(2)
n=Native(out,stage)
def update(p):n.update(p);time.sleep(.2)
def shot(name):
 data=o.call('GetSourceScreenshot',{'sourceName':stage,'imageFormat':'png','imageWidth':1920,'imageHeight':1080})
 p=out/(name+'.png');p.write_bytes(base64.b64decode(data['imageData'].split(',',1)[1]));return Image.open(p).convert('RGB')
def diff(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
def check(b,name,value=None):
 checks.append({'name':name,'pass':bool(b),'value':value});print(('PASS ' if b else 'FAIL ')+name+' '+str(value),flush=True)
def track(param,values):return {'parameter':param,'enabled':True,'shortest':False,'keys':[{'time_us':str(round(t*1000000)),'value':v,'mode':0} for t,v in values]}
try:
 update({'animation':[track('x',[(0,0),(2,200)])],'timeline_position_us':'1000000'});a=shot('camera-linear-midpoint')
 update({'animation':[],'camera_x':100});b=shot('camera-static-reference');check(diff(a,b)<.001,'camera parameter midpoint equals static reference pixels',diff(a,b))
 update({'camera_x':0,'layers':[dict(l,animation=[track('x',[(0,-150),(2,250)])]) if i==1 else l for i,l in enumerate(layers)],'timeline_position_us':'1000000'});a=shot('layer-linear-midpoint')
 static=[dict(l,x=50) if i==1 else l for i,l in enumerate(layers)];update({'layers':static});b=shot('layer-static-reference');check(diff(a,b)<.001,'layer midpoint equals same shared world coordinate pixels',diff(a,b))
 update({'layers':layers,'composite':{'sx':1,'sy':1,'opacity':1,'animation':[track('opacity',[(0,1),(2,.2)])]},'timeline_position_us':'1000000'});a=shot('composite-animated-opacity')
 update({'composite':{'sx':1,'sy':1,'opacity':.6,'animation':[]}});b=shot('composite-static-opacity');check(diff(a,b)<.001,'composite alpha curve equals static premultiplied reference with FX OFF',diff(a,b))
 update({'composite':{'sx':.7,'sy':.7,'opacity':1,'animation':[]}});a=shot('composite-scale');update({'composite':{'sx':1,'sy':1,'opacity':1,'animation':[]}});b=shot('composite-normal');check(diff(a,b)>1,'composite scale works independently of FX switch',diff(a,b))
 update({'animation':[track('x',[(0,0),(100000,200)])],'timeline_position_us':'50000000000'});a=shot('camera-long-time-midpoint');state=n.get();check(state['animation_duration']==100000,'GPU model retains100000second last key',state['animation_duration'])
 update({'animation':[],'camera_x':100});b=shot('camera-long-static-reference');check(diff(a,b)<.001,'50000second camera sample equals static reference',diff(a,b))
 moving=[dict(l,animation=[track('x',[(0,-150),(2,250)])]) if i==1 else l for i,l in enumerate(layers)]
 update({'camera_x':0,'layers':moving,'timeline_position_us':'2000000','fx_enabled':True,'light_enabled':True,'light_visible':True,'light_bind':layers[1]['uuid'],'light_bind_scene':scene,'light_bind_item':2,'light_x':0,'light_y':-200,'light_z':-600,'light_intensity':2,'light_ambient':.3,'light_range':3000,'light_size':90,'shadows_enabled':True});a=shot('light-bound-animated-parent')
 update({'layers':[dict(l,x=250) if i==1 else l for i,l in enumerate(layers)],'light_bind':'','light_bind_scene':'','light_bind_item':0,'light_x':250});b=shot('light-world-reference');check(diff(a,b)<.001,'animated layer and bound light equal analytic world reference pixels',diff(a,b))
 update({'fx_enabled':False,'layers':[dict(l,animation=[track('rz',[(0,0),(2,720)])]) if i==1 else l for i,l in enumerate(layers)],'timeline_position_us':'500000'});a=shot('rotation-full-turn-quarter')
 update({'layers':[dict(l,rz=180) if i==1 else l for i,l in enumerate(layers)]});b=shot('rotation-static-180');check(diff(a,b)<.001,'new rotation tracks preserve full turns in GPU output',diff(a,b))
 # Keep a populated moving three-layer/one-light scene for the subsequent installed stress run.
 update({'layers':layers,'animation':[track('x',[(0,-120),(6,120)]),track('z',[(0,-1500),(6,-1300)])],'timeline_position_us':'0','playing':True,'loop':True,'play_token':900,'fx_enabled':True,'light_x':-500,'light_y':-250,'light_z':-650,'light_bind':'','light_visible':False,'light_rim':1.5,'shadow_opacity':.8})
 shot('v32-final-render');video=o.call('GetVideoSettings')
except Exception as e:check(False,'schema4 probe exception',repr(e));video={}
finally:o.close()
report={'checks':checks,'failed':sum(not c['pass'] for c in checks),'stage':stage,'scene':scene,'video':video}
(out/'camera-render-tests.json').write_text(json.dumps(report,indent=2),encoding='utf-8');raise SystemExit(bool(report['failed']))
