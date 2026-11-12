"""Installed OBS/D3D regression: independent material lights and useful FX controls."""
import base64,copy,io,json,math,sys,time,traceback
from pathlib import Path
from PIL import Image,ImageDraw,ImageChops,ImageStat
from obs_client import OBS
from native_control import Native

out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
bridge=Path(sys.argv[2]).resolve()
root=Path(__file__).resolve().parents[1]
defs={d['id']:d for d in json.loads((root/'build/fx-runtime-catalog.json').read_text(encoding='utf-8-sig'))['effects']}
checks=[]
def check(ok,name,value=None):
 checks.append({'name':name,'pass':bool(ok),'value':value});print(('PASS ' if ok else 'FAIL ')+name+' '+str(value),flush=True)
def delta(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean[:3])

img=Image.new('RGBA',(1920,1080),(24,24,24,255));draw=ImageDraw.Draw(img)
for x in range(1920):
 v=round(x/1919*255);draw.line((x,700,x,1079),fill=(v,v,v,255))
for i,c in enumerate(('#bd4253','#b3a626','#36af61','#2ca2aa','#444ab4','#ad42a8')):draw.rectangle((i*320,490,i*320+319,680),fill=c)
draw.ellipse((440,140,480,180),fill=(255,255,255,255))
draw.rectangle((1100,190,1160,250),fill=(200,200,200,255))
draw.ellipse((1410,290,1510,390),fill=(130,130,130,255))
img.save(out/'fixture.png')
sil=Image.new('RGBA',(1920,1080));d=ImageDraw.Draw(sil);d.rounded_rectangle((300,120,1620,960),100,fill=(65,80,95,255));sil.save(out/'silhouette.png')
o=OBS();stamp=str(time.time_ns());scene='V342 FX '+stamp;stage='V342 stage '+stamp;actor='V342 fixture '+stamp
o.call('CreateScene',{'sceneName':scene})
o.call('CreateInput',{'sceneName':scene,'inputName':actor,'inputKind':'image_source','inputSettings':{'file':str(out/'fixture.png')},'sceneItemEnabled':False})
uuid=next(i['inputUuid'] for i in o.call('GetInputList')['inputs'] if i['inputName']==actor)
scale=1500/(1080/(2*math.tan(math.radians(20))))
layer={'uuid':uuid,'instance_id':'v342-actor','sx':scale,'sy':scale,'visible':True,'enabled':True,'receive_light':False,'receive_shadow':False}
base={'schema_version':5,'width':1920,'height':1080,'camera_z':-1500,'camera_fov':40,'fx_enabled':False,'playing':False,'timeline_preview':False,'light_enabled':False,'light_visible':False,'light_x':0,'light_y':0,'light_z':-400,'light_intensity':0,'light_range':1,'layers':[layer]}
o.call('CreateInput',{'sceneName':scene,'inputName':stage,'inputKind':'obs_spatial_fx_stage','inputSettings':base,'sceneItemEnabled':True});o.call('SetCurrentProgramScene',{'sceneName':scene});time.sleep(.8)
n=Native(bridge,stage)
def shot(name):
 data=o.call('GetSourceScreenshot',{'sourceName':stage,'imageFormat':'png','imageWidth':960,'imageHeight':540})
 b=base64.b64decode(data['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(b)
 return Image.open(io.BytesIO(b)).convert('RGBA')
def apply(identifier=None,params=None,name='shot',**patch):
 l=dict(layer)
 if identifier:
  definition=defs[identifier];p=dict(definition['parameters']);p.update(params or {})
  groups=[{'enabled':False} for _ in range(13)];groups[definition['group']-1]['enabled']=True
  l['fx_stack']={'attached':True,'group_states':groups,'effects':[{'id':identifier,'enabled':True,'strength':1,'parameters':p}]}
 n.update(dict(patch,layers=[l]));time.sleep(.12);return shot(name)
try:
 o.call('SetInputSettings',{'inputName':actor,'inputSettings':{'file':str(out/'silhouette.png')}})
 for idx in range(6):
  identifier=f'FX05.{idx}';params={'flicker':0} if idx==2 else {}
  n.update({'light_enabled':False,'fx_enabled':False});plain=apply(name=identifier+'-plain')
  before=apply(identifier,params,identifier+'-before')
  check(delta(plain,before)>.02,identifier+' local rig produces visible output',delta(plain,before))
  # A disabled/out-of-range stage lamp must not replace the independent FX rig.
  for master in (True,False,True,False):
   after=apply(identifier,params,identifier+'-toggle-'+str(master),fx_enabled=master,light_enabled=True)
   check(delta(before,after)<.1,identifier+' survives stage toggle '+str(master),delta(before,after))
  # Rebuilding the serialized model must also preserve the light.
  state=n.get();n.request(state);time.sleep(.15)
  check(delta(before,shot(identifier+'-reloaded'))<.1,identifier+' survives state reapply')
 o.call('SetInputSettings',{'inputName':actor,'inputSettings':{'file':str(out/'fixture.png')}})
 n.update({'fx_enabled':False,'light_enabled':False})
 for mode in (0,1):
  p={'mode':mode,'radius':45,'threshold':.1,'gain':1}
  low=apply('FX04.0',p,f'bloom-mode{mode}-low')
  high=apply('FX04.0',dict(p,threshold=.95),f'bloom-mode{mode}-high')
  check(delta(low,high)>.15,'bloom threshold changes output in mode '+str(mode),delta(low,high))
  # A tiny highlight must spread continuously, without sparse sample islands.
  large=apply('FX04.0',dict(p,radius=120),f'bloom-mode{mode}-large')
  check(delta(low,large)>.1,'bloom radius changes output in mode '+str(mode),delta(low,large))
 # Neutral base colors and useful intermediate settings, rather than only endpoints.
 neutral={'gamma':1,'red':1,'green':1,'blue':1,'exposure':0,'saturation':1,'contrast':1}
 plain=apply(name='color-original');a=apply('FX03.0',neutral,'color-neutral')
 check(delta(plain,a)<.1,'neutral basic color preserves input',delta(plain,a))
 for key,value in [('gamma',1.5),('red',1.5),('green',.5),('blue',1.5),('exposure',1),('saturation',0),('contrast',1.5)]:
  b=apply('FX03.0',dict(neutral,**{key:value}),'color-'+key)
  check(delta(a,b)>1,'basic color '+key+' has useful response',delta(a,b))
 # Exposure must not clip before a following compensating grade.
 groups=[{'enabled':False} for _ in range(13)];groups[2]['enabled']=True
 def fx(identifier,p):return {'id':identifier,'enabled':True,'strength':1,'parameters':dict(defs[identifier]['parameters'],**p)}
 stack={'attached':True,'group_states':groups,'effects':[fx('FX03.0',dict(neutral,exposure=2)),fx('FX03.3',{'shadow_exp':-2,'mid_exp':-2,'high_exp':-2,'shadow_b':0,'high_r':0})]}
 n.update({'layers':[dict(layer,fx_stack=stack)]});time.sleep(.2);recovered=shot('color-hdr-recovered')
 check(delta(a,recovered)<1,'chained grades retain highlights until final output',delta(a,recovered))
 # All legal control combinations must leave finite intermediates. A subsequent
 # neutral luminance curve must not turn a blown-out white highlight black.
 extreme=fx('FX03.0',dict(neutral,gamma=.1,red=3,green=3,blue=3,exposure=5))
 identity=fx('FX03.4',{'black':0,'shadow':.25,'middle':.5,'highlight':.75,'white':1,'channel':0})
 n.update({'layers':[dict(layer,fx_stack={'attached':True,'group_states':groups,'effects':[extreme,identity]})]});time.sleep(.2)
 extreme_image=shot('color-extreme-curve');pixel=extreme_image.getpixel((230,80))
 check(min(pixel[:3])>245 and pixel[3]==255,'legal extreme grade stays finite through following luminance curve',pixel)
 # Every six-color triplet and all tonal zones must respond, not merely global RGB.
 def response(identifier,key,lo,hi,params=None):
  p=dict(params or {});p[key]=lo;left=apply(identifier,p,identifier+'-'+key+'-lo')
  p[key]=hi;right=apply(identifier,p,identifier+'-'+key+'-hi')
  amount=delta(left,right);check(amount>.02,identifier+' '+key+' has independent visible range',amount)
 for color in ('red','yellow','green','cyan','blue','magenta'):
  for suffix,lo,hi in (('hue',-45,45),('saturation',0,2),('density',-.8,.8)):
   response('FX03.1',color+'_'+suffix,lo,hi)
 for key in ('temperature','tint'):response('FX03.2',key,-1,1)
 for zone in ('shadow','mid','high'):
  response('FX03.3',zone+'_exp',-2,2)
  for color in ('r','g','b'):response('FX03.3',zone+'_'+color,-.4,.4)
 for key in ('black','shadow','middle','highlight','white'):response('FX03.4',key,0,1)
 response('FX03.7','levels',2,32)
 response('FX03.9','offset',-90,90,{'speed':0})
 for identifier,key,lo,hi,p in (
  ('FX04.1','length',12,180,{'gain':3,'threshold':.6}),
  ('FX04.1','threshold',0,1,{'gain':3}),
  ('FX04.5','threshold',0,1,{'gain':3}),
  ('FX04.6','threshold',0,1,{'gain':2}),
  ('FX04.6','radius',0,45,{'gain':2,'threshold':.5})):
  response(identifier,key,lo,hi,p)
 # Bloom controls have exact neutral endpoints on an SDR input.
 plain=apply(name='glow-original')
 for mode in (0,1):
  high=apply('FX04.0',{'mode':mode,'threshold':1,'gain':5},'glow-threshold-one-'+str(mode))
  zero=apply('FX04.0',{'mode':mode,'threshold':0,'gain':0},'glow-gain-zero-'+str(mode))
  check(delta(plain,high)<.1 and delta(plain,zero)<.1,'glow mode '+str(mode)+' neutral endpoints are exact',{'thresholdOne':delta(plain,high),'gainZero':delta(plain,zero)})
 halo=apply('FX04.0',{'mode':0,'radius':120,'threshold':.6,'gain':5},'glow-continuous-halo')
 row=[halo.getpixel((x,80))[0] for x in range(242,273)]
 check(row[0]>row[-1]+3 and all(row[i+1]<=row[i]+1 for i in range(len(row)-1)), 'tiny highlight has a continuous outward halo without sparse islands',row)
 # Warm repeated frames may not keep allocating textures.
 warm=n.get()['_metrics'];time.sleep(1.2);stable=n.get()['_metrics']
 check(stable['allocations']==warm['allocations'] and stable['passes']>warm['passes'],'glow reuses warm GPU surfaces',{'warm':warm,'after':stable})
 # Measure the new maximum-radius path on the real 1080p60 stage.
 apply('FX04.0',{'mode':0,'radius':160,'threshold':.6,'gain':2},'glow-performance')
 time.sleep(2);warm=n.get()['_metrics'];before=o.call('GetStats');samples=[]
 for _ in range(30):time.sleep(1);samples.append(o.call('GetStats'))
 after=n.get()['_metrics']
 check(warm['allocations']==after['allocations'] and after['failures']==0,'maximum-radius glow allocation plateau for 30 seconds',{'warm':warm,'after':after})
 check(min(s['activeFps'] for s in samples)>59 and all(s['renderSkippedFrames']==before['renderSkippedFrames'] for s in samples),'maximum-radius glow sustains 1080p60 without additional render drops',{'minimumFps':min(s['activeFps'] for s in samples),'maxRenderMs':max(s['averageFrameRenderTime'] for s in samples)})
 (out/'glow-performance.json').write_text(json.dumps({'before':before,'samples':samples,'warmMetrics':warm,'afterMetrics':after},indent=2),encoding='utf-8')
 # Uniform / empty input boundaries are legitimate no-halo cases.
 m=n.get()['_metrics'];check(m['failures']==0,'no FX GPU failures',m)
except Exception as exc:
 check(False,'probe exception',repr(exc));traceback.print_exc()
finally:
 (out/'fx342-tests.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['pass'] for c in checks)},ensure_ascii=False,indent=2),encoding='utf-8')
 o.close()
sys.exit(1 if any(not c['pass'] for c in checks) else 0)
