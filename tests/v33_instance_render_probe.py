"""Real OBS D3D screenshots for independent instances sharing one input."""
import base64, json, sys, time
from pathlib import Path
from PIL import Image, ImageDraw, ImageChops, ImageStat
from obs_client import OBS
from native_control import Native

out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
checks=[]
def check(ok,name,value=None):
 checks.append({'name':name,'pass':bool(ok),'value':value});print(('PASS ' if ok else 'FAIL ')+name+' '+str(value),flush=True)
def difference(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
fixture=out/'instance-fixture.png';image=Image.new('RGBA',(240,180),(0,0,0,0));draw=ImageDraw.Draw(image);draw.rounded_rectangle((4,4,236,176),18,fill=(20,210,160,255));draw.rectangle((25,25,100,70),fill=(255,210,80,255));image.save(fixture)
o=OBS();stamp=str(time.time_ns());scene='Spatial v33 instances '+stamp;source='Shared instance source '+stamp;stage='Independent instance stage '+stamp
try:
 o.call('CreateScene',{'sceneName':scene})
 created=o.call('CreateInput',{'sceneName':scene,'inputName':source,'inputKind':'image_source','inputSettings':{'file':str(fixture)},'sceneItemEnabled':False})
 item=o.call('GetSceneItemId',{'sceneName':scene,'sourceName':source})['sceneItemId']
 scene_uuid=next(s['sceneUuid'] for s in o.call('GetSceneList')['scenes'] if s['sceneName']==scene)
 common={'uuid':created['inputUuid'],'scene_uuid':scene_uuid,'item_id':item,'label':source,'y':0,'z':0,'sx':1,'sy':1,'cast_shadow':False}
 layers=[dict(common,instance_id='first',instance_number=1,x=-400),dict(common,instance_id='second',instance_number=2,x=400)]
 base={'schema_version':4,'width':1920,'height':1080,'fx_enabled':False,'layers':layers,'camera_x':0,'camera_y':0,'camera_z':-1500,'camera_fov':40,'timeline_preview':True,'timeline_position_us':'0','playing':False,'animation':[]}
 o.call('CreateInput',{'sceneName':scene,'inputName':stage,'inputKind':'obs_spatial_fx_stage','inputSettings':base,'sceneItemEnabled':True});o.call('SetCurrentProgramScene',{'sceneName':scene});time.sleep(1)
 n=Native(out,stage)
 def update(p):n.update(p);time.sleep(.18)
 def shot(name):
  data=o.call('GetSourceScreenshot',{'sourceName':stage,'imageFormat':'png','imageWidth':1920,'imageHeight':1080});path=out/(name+'.png');path.write_bytes(base64.b64decode(data['imageData'].split(',',1)[1]));return Image.open(path).convert('RGB')
 both=shot('two-shared-source-instances');check(both.crop((0,0,960,1080)).getbbox() is not None and both.crop((960,0,1920,1080)).getbbox() is not None,'same input renders at two independent stage positions')
 track={'parameter':'x','enabled':True,'keys':[{'time_us':'0','value':400,'mode':0},{'time_us':'2000000','value':700,'mode':0}]}
 update({'layers':[layers[0],dict(layers[1],animation=[track])],'timeline_position_us':'1000000'});moving=shot('second-instance-animation-midpoint')
 check(difference(both.crop((0,0,960,1080)),moving.crop((0,0,960,1080)))<.001,'animating second instance preserves first instance pixels')
 check(difference(both.crop((960,0,1920,1080)),moving.crop((960,0,1920,1080)))>.1,'second instance actually moves in GPU output')
 update({'layers':[layers[0],dict(layers[1],x=550)]});static=shot('second-instance-static-reference');check(difference(moving,static)<.001,'duplicate animation midpoint equals independent static world pose',difference(moving,static))
 update({'fx_enabled':True,'light_enabled':True,'light_bind':created['inputUuid'],'light_bind_scene':scene_uuid,'light_bind_item':item,'light_bind_instance':'second','light_x':0,'light_y':-80,'light_z':-600,'light_intensity':2,'light_ambient':.3,'shadows_enabled':False,'light_visible':True,'light_rim':1})
 bound=shot('light-bound-to-second-instance');update({'light_bind':'','light_bind_instance':'','light_bind_scene':'','light_bind_item':0,'light_x':550});world=shot('light-explicit-world-reference');check(difference(bound,world)<.001,'light bound to duplicate equals mathematical world reference pixels',difference(bound,world))
 check(difference(bound,static)>.1,'light comparison produces a visible rendered lighting change')
 # Disabling only the second instance must not deactivate its shared source.
 update({'fx_enabled':False,'layers':[layers[0],dict(layers[1],x=550,visible=False)]});single=shot('first-instance-remains-active');check(difference(single.crop((0,0,960,1080)),both.crop((0,0,960,1080)))<.001,'hiding duplicate keeps sibling source active and unchanged')
 check(single.crop((960,0,1920,1080)).getbbox() is None,'hidden duplicate no longer draws on stage')
 video=o.call('GetVideoSettings')
except Exception as error:
 check(False,'GPU probe exception',repr(error));video={}
finally:o.close()
(out/'instance-render-tests.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['pass'] for c in checks),'video':video,'scene':scene,'stage':stage},indent=2),encoding='utf8')
raise SystemExit(any(not c['pass'] for c in checks))
