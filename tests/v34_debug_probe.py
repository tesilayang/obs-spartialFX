"""Real D3D11 integration evidence. Synthetic images are QA fixtures, never user media."""
import base64, copy, json, math, sys, time, traceback
from pathlib import Path
from PIL import Image, ImageDraw, ImageChops, ImageStat
from obs_client import OBS
from native_control import Native

out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
bridge=Path(sys.argv[2]).resolve() if len(sys.argv)>2 else out
catalog=json.loads((Path(__file__).resolve().parents[1]/'build/fx-runtime-catalog.json').read_text(encoding='utf-8-sig'))['effects']
checks=[];results=[]
def check(ok,label,value=None):
 checks.append({'name':label,'pass':bool(ok),'value':value})
 print(('PASS ' if ok else 'FAIL ')+label+(f' {value}' if value is not None else ''),flush=True)
def delta(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
img=Image.new('RGBA',(640,360));d=ImageDraw.Draw(img)
colors=['#ed3344','#f2da27','#36de61','#2cdae1','#3e48f5','#dc42ce']
for x in range(640):
 t=x/639;d.line((x,20,x,339),fill=(int(t*255),int((1-t)*220),int((.3+.7*abs(math.sin(t*math.pi*2)))*255),255))
for y in range(25,335,15):
 for x in range(20,620,15):
  if (x//15+y//15)%2:d.rectangle((x,y,x+6,y+6),fill=(25,25,25,255))
for i,c in enumerate(colors):d.rectangle((28+i*98,50,118+i*98,148),fill=c)
d.rounded_rectangle((170,170,470,315),radius=35,fill=(238,238,238,255))
d.ellipse((265,200,350,285),fill=(0,0,0,0));d.text((28,155),'SPATIAL FX / 1080p 60 / 0123456789',fill='white',stroke_width=1)
for i,alpha in enumerate((32,96,160,224)):d.rectangle((30+i*90,320,100+i*90,352),fill=(210,165,40,alpha))
img.resize((1920,1080)).save(out/'fixture.png')
alternate=Image.new('RGBA',(640,360));dr=ImageDraw.Draw(alternate);dr.ellipse((60,35,580,325),fill=(35,220,135,255));dr.rectangle((200,80,440,280),fill=(225,40,60,255));alternate.resize((1920,1080)).save(out/'alternate.png')
aux=Image.new('RGBA',(640,360));dr=ImageDraw.Draw(aux);dr.ellipse((60,20,580,340),fill=(200,100,230,255));dr.rectangle((0,150,640,210),fill=(55,240,90,180));aux.resize((1920,1080)).save(out/'auxiliary.png')
cube=out/'invert.cube';cube.write_text('LUT_3D_SIZE 2\n'+'\n'.join(f'{1-r} {1-g} {1-b}' for b in range(2) for g in range(2) for r in range(2))+'\n',encoding='utf-8')
o=OBS();stamp=str(time.time_ns());scene='Spatial V34 GPU '+stamp;stage='Spatial V34 GPU Stage '+stamp
o.call('CreateScene',{'sceneName':scene})
def source(name,path):
 name+=' '+stamp;o.call('CreateInput',{'sceneName':scene,'inputName':name,'inputKind':'image_source','inputSettings':{'file':str(path)},'sceneItemEnabled':False})
 return name,next(i['inputUuid'] for i in o.call('GetInputList')['inputs'] if i['inputName']==name)
actorName,actorId=source('V34 actor',out/'fixture.png');auxName,auxId=source('V34 hidden auxiliary',out/'auxiliary.png')
scale=1500/(1080/(2*math.tan(math.radians(20))))
layer={'uuid':actorId,'instance_id':'gpu-actor','label':'V34 actor','sx':scale,'sy':scale,'x':0,'y':0,'z':0,'visible':True,'enabled':True}
reference={'uuid':auxId,'instance_id':'gpu-hidden-reference','label':'Hidden raw reference','visible':False,'enabled':False,'opacity':0}
base={'schema_version':5,'width':1920,'height':1080,'camera_z':-1500,'camera_fov':40,'fx_enabled':False,'playing':False,'timeline_preview':False,'layers':[layer,reference],
 'light_enabled':True,'light_x':-300,'light_y':-180,'light_z':-400,'light_range':3000,'light_intensity':2,'light_ambient':.3,'light_visible':False}
o.call('CreateInput',{'sceneName':scene,'inputName':stage,'inputKind':'obs_spatial_fx_stage','inputSettings':base,'sceneItemEnabled':True});o.call('SetCurrentProgramScene',{'sceneName':scene});time.sleep(1)
control=Native(bridge,stage)
def update(p,wait=.12):control.update(p);time.sleep(wait)
def shot(name=None):
 data=o.call('GetSourceScreenshot',{'sourceName':stage,'imageFormat':'png','imageWidth':640,'imageHeight':360})
 payload=base64.b64decode(data['imageData'].split(',',1)[1])
 if name:(out/(name+'.png')).write_bytes(payload)
 import io
 return Image.open(io.BytesIO(payload)).convert('RGBA')
def stack(definition,params=None,on=True,group=True,strength=1):
 f={'id':definition['id'],'enabled':on,'strength':strength,'parameters':dict(definition['parameters']),'reference':'gpu-hidden-reference','color':{'r':.8,'g':.2,'b':.65},'color2':{'r':.1,'g':.65,'b':.9}}
 if params:
  assert set(params)<=set(definition['parameters']),(definition['id'],params)
  for key,value in params.items():
   limit=definition['parameter_limits'][key];assert limit['min']<=value<=limit['max'],(definition['id'],key,value)
  f['parameters'].update(params)
 if definition['asset']==3:f.update(path=str(cube),reference='')
 states=[{'enabled':False} for _ in range(13)];states[definition['group']-1]['enabled']=group
 return {'attached':True,'group_states':states,'effects':[f]}
def layersfx(s):return [dict(layer,fx_stack=s),dict(reference)]
def metrics():return control.get()['_metrics']

defs={d['id']:d for d in catalog}
def apply(identifier,params=None,wait=.18,**settings):
 update(dict(settings,layers=layersfx(stack(defs[identifier],params))),wait)
 return shot()
try:
 original=shot('original')
 check('FX11.1' not in defs and 'FX12.9' in defs,'removed parallax and registered stable gradient ID')
 # Different kernels on identical input/radius, with lens high-light boost disabled.
 gaussian=apply('FX01.0',{'radius':30,'algorithm':0});gaussian.save(out/'blur-gaussian.png')
 box=apply('FX01.0',{'radius':30,'algorithm':1});box.save(out/'blur-box.png')
 lens=apply('FX01.4',{'radius':30,'boost':0,'aperture':0});lens.save(out/'blur-lens.png')
 polygon=apply('FX01.4',{'radius':30,'boost':0,'aperture':1});polygon.save(out/'blur-pentagon.png')
 for a,b,label in [(gaussian,box,'Gaussian versus uniform box'),(gaussian,lens,'Gaussian versus aperture disc'),(lens,polygon,'circular versus pentagonal aperture')]:check(delta(a,b)>.1,'distinct blur: '+label,delta(a,b))
 identity=apply('FX01.0',{'radius':0});check(delta(identity,original)<.001,'zero blur radius restores exact original')
 # Boundary policy is independent from kernel; edge ramp differentiates reflection.
 edge=Image.new('RGBA',(1920,1080),(0,0,0,255));ed=ImageDraw.Draw(edge)
 for x in range(1920):ed.line((x,0,x,1079),fill=(min(x*6,255),100,180,255))
 edge.save(out/'edge-ramp.png');o.call('SetInputSettings',{'inputName':actorName,'inputSettings':{'file':str(out/'edge-ramp.png')}});time.sleep(.2)
 extend=apply('FX01.0',{'radius':60,'algorithm':0,'edge':0});mirror=apply('FX01.0',{'radius':60,'algorithm':0,'edge':1});transparent=apply('FX01.0',{'radius':60,'algorithm':0,'edge':2})
 check(delta(extend,mirror)>.01,'mirror boundary reflects edge ramp instead of clamping',delta(extend,mirror));check(transparent.getpixel((0,180))[3]<extend.getpixel((0,180))[3],'transparent edge reduces coverage while extend stays opaque')
 o.call('SetInputSettings',{'inputName':actorName,'inputSettings':{'file':str(out/'fixture.png')}});time.sleep(.2)
 gradientAlpha=apply('FX12.9',{'mode':2});check(delta(gradientAlpha.getchannel('A'),original.getchannel('A'))<.001,'gradient preserves original transparent silhouette exactly')
 # Actual default stage lights off: every material effect contributes visibly.
 for i in range(6):
  identifier=f'FX05.{i}';pic=apply(identifier,light_enabled=False,fx_enabled=False);pic.save(out/(identifier+'-local-light.png'))
  check(delta(original,pic)>.1,'local default light actually affects '+identifier,delta(original,pic))
 # Explicit spatial light is still used, rather than replaced by local rig.
 a=apply('FX05.2',light_enabled=True,light_x=-400,light_y=-200,light_z=-350,light_intensity=2)
 b=apply('FX05.2',light_enabled=True,light_x=400,light_y=200,light_z=-350,light_intensity=2)
 check(delta(a,b)>.1,'configured spatial lamp movement changes material lighting',delta(a,b))
 zero=apply('FX05.2',light_enabled=True,light_intensity=0);check(delta(zero,original)<.001,'explicit enabled zero-intensity lamp stays dark')
 # Opaque fixture enables analytical gradients, screen masks, LED cells.
 uniform=Image.new('RGBA',(1920,1080),(150,120,90,255));uniform.save(out/'opaque.png')
 o.call('SetInputSettings',{'inputName':actorName,'inputSettings':{'file':str(out/'opaque.png')}});update({'light_enabled':False,'layers':[layer,reference]});opaque=shot('opaque-original')
 def gradient(params):
  st=stack(defs['FX12.9'],params);st['effects'][0]['color']={'r':1,'g':0,'b':0};st['effects'][0]['color2']={'r':0,'g':0,'b':1};update({'layers':layersfx(st)});return shot()
 solid=gradient({'mode':0});check(solid.getpixel((320,180))[:3]==(255,0,0),'solid overlay uses selected color')
 linear=gradient({'mode':1,'angle':0});linear.save(out/'gradient-linear.png')
 check(linear.getpixel((20,180))[0]>245 and linear.getpixel((620,180))[2]>245,'linear gradient reaches chosen endpoints')
 reverse=gradient({'mode':1,'reverse':1,'angle':0});check(delta(linear,reverse)>50 and reverse.getpixel((20,180))[2]>245,'reverse swaps linear endpoints')
 vertical=gradient({'mode':1,'angle':90});check(vertical.getpixel((320,20))[0]>240 and vertical.getpixel((320,340))[2]>240,'linear angle rotates gradient ninety degrees')
 circle=gradient({'mode':2,'scale':1,'softness':1});ellipse=gradient({'mode':2,'scale_x':2,'scale_y':.5});ellipse.save(out/'gradient-ellipse.png')
 check(circle.getpixel((380,180))==circle.getpixel((320,240)),'radial default is a pixel-correct circle')
 check(ellipse.getpixel((380,180))[0]>ellipse.getpixel((320,240))[0]+40,'radial independent axes create ellipse')
 shifted=gradient({'mode':2,'center_x':.25,'center_y':.5});check(shifted.getpixel((160,180))[0]>250,'gradient center follows XY')
 # LED preserves source color, dark gaps and visible subpixel placement.
 led=apply('FX13.0',{'spacing':30,'fill':.7,'gain':1,'glow':0,'mode':0});led.save(out/'led-circle.png')
 center=led.getpixel((5,5));gap=led.getpixel((0,0));check(sum(center[:3])>250 and sum(gap[:3])<10,'LED cells show source-colored emitters separated by black gaps',{'center':center,'gap':gap})
 rgb=apply('FX13.0',{'spacing':60,'fill':.8,'gain':1,'glow':0,'mode':1});rgb.save(out/'led-rgb.png')
 check(delta(led,rgb)>15,'RGB emitters and circular cells have different structure',delta(led,rgb))
 check(led.getchannel('A').getextrema()==(255,255),'LED gaps are dark screen surface, not transparency holes')
 # Stage mask acts after all layers and is fixed to canvas despite output transform.
 bars=apply('FX13.1',{'mode':0});bars.save(out/'21x9-blackbars.png')
 check(bars.getpixel((320,5))==(0,0,0,255) and bars.getpixel((320,355))==(0,0,0,255),'whole-stage opaque top/bottom black bars')
 check(bars.getpixel((320,180))==opaque.getpixel((320,180)),'21by9 preserves center pixels')
 check(bars.getpixel((320,42))==(0,0,0,255) and bars.getpixel((320,44))==opaque.getpixel((320,44)),'21by9 boundary has expected 43px bars at640x360')
 crop=apply('FX13.1',{'mode':1});crop.save(out/'21x9-transparent.png');check(crop.getpixel((320,5))[3]==0 and crop.getpixel((320,180))[3]==255,'transparent crop removes only outside21by9')
 # A small controlling layer must still mask the entire other layer.
 small=dict(layer,sx=scale*.25,sy=scale*.25,fx_stack=stack(defs['FX13.1'],{'mode':0}));back=dict(layer,instance_id='full-background')
 update({'layers':[small,back]});whole=shot('21x9-multiple-layers');check(whole.getpixel((600,5))==(0,0,0,255),'21by9 masks other layers outside assigned object')
 update({'layers':layersfx(stack(defs['FX13.1'],{'mode':0})),'composite':{'sx':.5,'sy':.5,'opacity':1}});scaled=shot('21x9-scaled-output');check(scaled.getpixel((10,5))==(0,0,0,255),'black bars stay canvas-wide when output is scaled');check(scaled.getpixel((10,90))[3]==0 and scaled.getpixel((320,180))[3]==255,'scaled composition is really half-size behind fixed bars');check(control.get()['composite']['sx']==.5,'scale test reaches authoritative composite state')
 update({'layers':[layer,reference],'composite':{'sx':1,'sy':1,'opacity':1}});restored=shot();check(delta(restored,opaque)<.001,'disabling all effects restores exact opaque original')
 check(control.get()['_metrics']['failures']==0,'no GPU effect failures',control.get()['_metrics'])
except Exception:
 check(False,'patch probe exception',traceback.format_exc())
finally:
 try:control.update({'layers':[layer,reference],'composite':{'sx':1,'sy':1,'opacity':1}})
 except Exception:pass
 o.close()
 (out/'patch-tests.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['pass'] for c in checks),'stage':stage},ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(any(not c['pass'] for c in checks))
