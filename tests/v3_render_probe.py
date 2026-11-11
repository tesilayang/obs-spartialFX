"""Real OBS32 D3D11 render regression; synthetic assets are explicit QA fixtures."""
import sys,time,json,base64,math
from pathlib import Path
from PIL import Image,ImageDraw,ImageChops,ImageStat
from obs_client import OBS
from native_control import LegacyNative as Native
out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
wall=Image.new('RGBA',(1920,1080),(177,178,171,255));d=ImageDraw.Draw(wall)
for y in range(0,1080,180):
 d.line((0,y,1920,y),fill=(126,128,125),width=3)
 for x in range(0,1920,240):d.line((x+(120 if y//180%2 else 0),y,x+(120 if y//180%2 else 0),y+180),fill=(150,150,145),width=2)
for x in range(20,1920,80):d.ellipse((x,35,x+6,41),fill=(120,120,117))
wall.save(out/'wall.png')
actor=Image.new('RGBA',(400,600));d=ImageDraw.Draw(actor);d.ellipse((95,10,305,220),fill=(225,190,165));d.polygon([(100,210),(300,210),(380,580),(20,580)],fill=(205,218,229));d.ellipse((115,70,135,95),fill=(50,70,110));d.ellipse((265,70,285,95),fill=(50,70,110))
for x in range(90,320,20):d.line((x,250,x,550),fill=(135,155,180),width=2)
actor.save(out/'actor.png')
front=Image.new('RGBA',(320,420));d=ImageDraw.Draw(front);d.rounded_rectangle((25,30,295,400),radius=50,fill=(75,135,180));d.ellipse((80,90,240,250),fill=(0,0,0,0));front.save(out/'foreground.png')
for attempt in range(100):
 try:o=OBS();break
 except OSError:time.sleep(.25)
checks=[]
def check(c,label,value=None):
 checks.append({'name':label,'pass':bool(c),'value':value});print(('PASS ' if c else 'FAIL ')+label+(' '+str(value) if value is not None else ''),flush=True)
time.sleep(4)
stamp=str(int(time.time()));scene='Spatial FX V3 Render '+stamp
o.call('CreateScene',{'sceneName':scene})
def make(name,kind,settings):
 name=name+' '+stamp;o.call('CreateInput',{'sceneName':scene,'inputName':name,'inputKind':kind,'inputSettings':settings,'sceneItemEnabled':False});return name,o.call('GetInputList')['inputs']
def source(name,file):
 name,_=make(name,'image_source',{'file':str(file)})
 inputs=o.call('GetInputList')['inputs'];return name,next(i['inputUuid'] for i in inputs if i['inputName']==name)
wallName,wallId=source('Spatial V3 QA wall',out/'wall.png');actorName,actorId=source('Spatial V3 QA actor',out/'actor.png');frontName,frontId=source('Spatial V3 QA front',out/'foreground.png')
focal=1080/(2*math.tan(math.radians(20)))
layers=[{'uuid':wallId,'label':'QA wall','x':0,'y':0,'z':500,'sx':2000/focal,'sy':2000/focal,'cast_shadow':False,'rim_light':False},
 {'uuid':actorId,'label':'QA actor','x':-80,'y':50,'z':0,'sx':1500/focal,'sy':1500/focal,'cast_shadow':True},
 {'uuid':frontId,'label':'QA foreground','x':400,'y':110,'z':-160,'sx':1340/focal,'sy':1340/focal,'cast_shadow':True}]
settings={'schema_version':3,'fx_enabled':True,'width':1920,'height':1080,'layers':layers,'camera_z':-1500,'camera_fov':40,'camera_x':0,'camera_y':0,'camera_rx':0,'camera_ry':0,'camera_rz':0,'camera_look_at':False,'playing':False,'light_enabled':True,'light_visible':False,'light_x':-500,'light_y':-250,'light_z':-650,'light_intensity':2.6,'light_range':3000,'light_ambient':.25,'light_red':1,'light_green':.86,'light_blue':.65,'light_size':60,'light_type':0,'light_falloff':2,'light_softness':15,'light_rim':1.5,'shadows_enabled':True,'shadow_opacity':.85,'receiver_uuid':wallId,'animation_duration':4,'animation_speed':1,'extra_lights':[{'light_enabled':False,'light_type':1,'light_size':200}]}
stage='Spatial FX V3 QA Stage '+stamp;o.call('CreateInput',{'sceneName':scene,'inputName':stage,'inputKind':'obs_spatial_fx_stage','inputSettings':settings,'sceneItemEnabled':True});o.call('SetCurrentProgramScene',{'sceneName':scene});time.sleep(2)
control=Native(out,stage)
def update(p):control.update(p);time.sleep(.35)
def shot(name):
 data=o.call('GetSourceScreenshot',{'sourceName':stage,'imageFormat':'png','imageWidth':1920,'imageHeight':1080})
 p=out/name;p.write_bytes(base64.b64decode(data['imageData'].split(',',1)[1]));return Image.open(p).convert('RGB')
def diff(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
try:
 # Master switch must bypass all lighting, post and focus even when their values are non-neutral.
 update({'fx_enabled':False,'fx_exposure':3,'fx_saturation':0,'aperture':20,'light_visible':True});original=shot('master-off-original.png')
 update({'fx_exposure':-4,'light_x':900,'light_intensity':9,'light_rim':5,'light_visible':False,'aperture':0});same=shot('master-off-adjusted.png');check(diff(original,same)<.001,'FX OFF bypasses every lighting and post-processing control',diff(original,same))
 update({'fx_enabled':True,'fx_exposure':0,'fx_saturation':1,'light_x':-500,'light_intensity':2.6,'light_rim':1.5})
 lit=shot('point-light-soft-shadow.png');check(sum(ImageStat.Stat(lit).mean)>30,'shader renders populated actual GPU output',ImageStat.Stat(lit).mean)
 update({'light_enabled':False});unlit=shot('unlit.png');check(diff(lit,unlit)>.5,'point light changes receiver and foreground pixels',diff(lit,unlit));update({'light_enabled':True})
 update({'shadow_opacity':0});no=shot('no-shadow.png');update({'shadow_opacity':.85});yes=shot('layer-shadow.png');check(diff(no,yes)>.05,'foreground alpha casts shadow on selected actual wall',diff(no,yes))
 update({'receiver_uuid':actorId});other=shot('receiver-changed.png');check(diff(yes,other)>.05,'receiver selector routes shadow to specified material',diff(yes,other));update({'receiver_uuid':wallId})
 update({'light_rim':0});no=shot('rim-off.png');update({'light_rim':4});yes=shot('rim-on.png');check(diff(no,yes)>.005,'directional alpha-edge rim changes actual pixels',diff(no,yes))
 update({'light_type':1,'light_size':250,'light_softness':40});area=shot('area-light.png');check(diff(area,yes)>.02,'area emitter size creates different penumbra',diff(area,yes));update({'light_type':0,'light_size':500,'shadows_enabled':False,'light_rim':0});pointDirect=shot('point-direct.png');update({'light_type':1});areaDirect=shot('area-direct.png');check(diff(pointDirect,areaDirect)>.01,'area size changes direct illumination independent of shadows',diff(pointDirect,areaDirect))
 update({'light_type':0,'light_size':60,'light_softness':15,'light_rim':1.5,'shadows_enabled':True})
 update({'layers':[dict(l,receive_light=False) if l['uuid']==actorId else l for l in layers]});a=shot('actor-unlit.png');update({'layers':layers});b=shot('actor-lit.png');check(diff(a,b)>.05,'per-layer receive-light switch affects chosen actor',diff(a,b))
 update({'focus_distance':600,'aperture':20});a=shot('depth-of-field.png');update({'aperture':0});b=shot('in-focus.png');check(diff(a,b)>.02,'focus distance and aperture produce actual blur',diff(a,b))
 update({'camera_x':220});a=shot('parallax-depth.png');update({'layers':[dict(l,perspective=0) for l in layers]});b=shot('parallax-flat.png');check(diff(a,b)>.1,'layer depth and camera distance produce differential parallax',diff(a,b));update({'layers':layers,'camera_x':0})
 update({'camera_look_at':True,'camera_x':300,'camera_target_x':-80,'camera_target_y':50});a=shot('target-orbit.png');update({'camera_look_at':False,'camera_x':0});b=shot('neutral-camera.png');check(diff(a,b)>.1,'camera target and orbit change rendered perspective',diff(a,b))
 update({'light_bind':actorId});a=shot('bound-light.png');update({'light_bind':''});b=shot('world-light.png');check(diff(a,b)>.01,'source binding transforms emitter into layer space',diff(a,b))
 keys=[{'time':0,'camera_x':-200,'camera_z':-1500,'camera_fov':40},{'time':4,'camera_x':200,'camera_z':-1250,'camera_fov':40}]
 update({'keyframes':keys,'playing':True,'loop':True,'play_token':100});a=shot('motion-a.png');time.sleep(1);b=shot('motion-b.png');check(diff(a,b)>.2,'motion speed duration and keyframes animate GPU output',diff(a,b))
 update({'playing':False,'camera_x':0})
 # A visible emitter is independent from its illumination contribution and is depth composited.
 update({'playing':False,'timeline_preview':False,'light_enabled':False,'light_x':0,'light_y':0,'light_z':-900,'light_size':220,'light_visible':False});hidden=shot('emitter-hidden.png')
 update({'light_visible':True});visible=shot('emitter-visible.png');check(diff(hidden,visible)>.1,'emitter visibility creates an actual renderable light entity',diff(hidden,visible))
 # Animated parent translation and rotation affect both the lamp and shaded materials.
 moving=[dict(l) for l in layers];moving[1]['motion']=[{'time':0,'x':-80,'y':50,'z':0,'sx':1500/focal,'sy':1500/focal},{'time':2,'x':320,'y':50,'z':0,'rz':30,'sx':1800/focal,'sy':1800/focal}]
 update({'layers':moving,'light_enabled':True,'light_bind':actorId,'light_x':0,'light_y':-200,'light_z':-400,'timeline_preview':True,'timeline_position':0});bound0=shot('anchor-start.png')
 update({'timeline_position':2});bound1=shot('anchor-end.png');check(diff(bound0,bound1)>.2,'per-layer motion and bound emitter move together on timeline',diff(bound0,bound1))
 update({'light_bind':'','timeline_preview':False,'timeline_position':0,'light_visible':False,'light_x':-500,'light_y':-250,'light_z':-650,'light_intensity':2.6,'keyframes':[]})
 # Exact sceneitem association must distinguish two copies of one OBS input.
 duplicates=[dict(layers[0]),dict(layers[1],scene_uuid='qa-shared',item_id=1101,x=-500),dict(layers[1],scene_uuid='qa-shared',item_id=1102,x=500)]
 update({'layers':duplicates,'light_enabled':False,'light_visible':True,'light_size':160,'light_bind':actorId,'light_bind_scene':'qa-shared','light_bind_item':1101,'light_x':0,'light_y':-150,'light_z':-500});copy1=shot('duplicate-anchor-one.png')
 update({'light_bind_item':1102});copy2=shot('duplicate-anchor-two.png');check(diff(copy1,copy2)>.2,'same source UUID anchors to exact chosen sceneitem in GPU output',diff(copy1,copy2))
 update({'light_bind':'','light_bind_scene':'','light_bind_item':0,'light_visible':False,'layers':layers,'light_x':-500,'light_y':-250,'light_z':-650})
 # Restore a useful lit scene, intentionally retain all UUID-linked sources for restart.
 update({'light_enabled':True,'shadow_opacity':.85,'light_rim':1.5,'layers':layers,'camera_x':0,'camera_z':-1500})
 final=shot('v3-final-render.png')
except Exception as e:
 check(False,'probe exception',str(e))
finally:
 o.close()
report={'checks':checks,'failed':sum(not c['pass'] for c in checks),'scene':scene,'stage':stage,'wallUuid':wallId,'actorUuid':actorId}
(out/'render-tests.json').write_text(json.dumps(report,indent=2),encoding='utf-8');sys.exit(bool(report['failed']))


