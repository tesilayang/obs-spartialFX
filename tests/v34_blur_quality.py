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
for attempt in range(100):
 try:o.call('CreateScene',{'sceneName':scene});break
 except RuntimeError as exc:
  if '207' not in str(exc) or attempt==99:raise
  time.sleep(.2)
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
 data=o.call('GetSourceScreenshot',{'sourceName':stage,'imageFormat':'png','imageWidth':1920,'imageHeight':1080})
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
 line=Image.new('RGBA',(1920,1080));dr=ImageDraw.Draw(line);dr.rectangle((954,0,965,1079),fill=(255,255,255,255));line.save(out/'line-fixture.png')
 o.call('SetInputSettings',{'inputName':actorName,'inputSettings':{'file':str(out/'line-fixture.png')}});update({'light_enabled':False,'fx_enabled':False});time.sleep(.2)
 baseline=shot('line-original')
 profiles={}
 for algorithm,label in ((0,'gaussian'),(1,'box')):
  for radius in (3,30,100):
   picture=apply('FX01.0',{'radius':radius,'algorithm':algorithm,'edge':2});picture.save(out/f'{label}-{radius}.png')
   profile=[picture.getpixel((x,540))[3] for x in range(1920)];profiles[f'{label}-{radius}']=profile
   weights=[math.exp(-.5*(i/max(radius/3,.33))**2) if algorithm==0 else 1 for i in range(-radius,radius+1)];norm=sum(weights)
   expected=[sum(weights[i+radius]*255 for i in range(-radius,radius+1) if 954<=x+i<=965)/norm for x in range(840,1080)]
   error=sum(abs(a-profile[x]) for x,a in zip(range(840,1080),expected))/240
   check(error<2.0,f'{label} radius{radius} matches dense convolution alpha MAE<2',round(error,5))
   if radius==100:check(min(profile[910:1010])>0,f'{label} radius100 has continuous coverage without repeated-copy gaps',min(profile[910:1010]))
 check(sum(abs(a-b) for a,b in zip(profiles['gaussian-100'],profiles['box-100']))/240>3,'Gaussian bell and uniform box have distinct dense profiles')
 # Opaque high frequency texture must actually average, rather than reproduce displaced copies.
 detail=Image.new('RGBA',(1920,1080));dr=ImageDraw.Draw(detail)
 for y in range(0,1080,6):
  for x in range(0,1920,6):dr.rectangle((x,y,x+5,y+5),fill=((255 if (x//6+y//6)%2 else 0),)*3+(255,))
 detail.save(out/'detail-fixture.png');o.call('SetInputSettings',{'inputName':actorName,'inputSettings':{'file':str(out/'detail-fixture.png')}});time.sleep(.2)
 for algorithm,label in ((0,'gaussian'),(1,'box')):
  picture=apply('FX01.0',{'radius':100,'algorithm':algorithm});picture.save(out/f'{label}-detail.png');spread=ImageStat.Stat(picture.crop((150,150,1770,930))).stddev[0]
  check(spread<2,f'{label} radius100 removes high-frequency checker grid',round(spread,4))
 for identifier,params,region in [('FX01.2',{'radius':100,'center_x':.5,'center_y':.5,'clear_radius':0,'feather':1},(1480,520,1520,560)),('FX01.5',{'radius':100,'angle':0,'center':.5,'width':0,'feather':.5},(940,790,980,830))]:
  picture=apply(identifier,params);picture.save(out/(identifier+'-feather.png'));spread=ImageStat.Stat(picture.crop(region)).stddev[0]
  check(spread<5,identifier+' mid-feather progressively defocuses detail without sharp double-image',round(spread,4))
 # Portrait-style alpha contour with hair and detailed clothing, inspect contact sheet.
 silhouette=Image.new('RGBA',(1920,1080));dr=ImageDraw.Draw(silhouette);dr.ellipse((790,100,1130,440),fill='white');dr.polygon([(820,400),(1100,400),(1320,1079),(590,1079)],fill=(180,100,50,255))
 for x in range(800,1120,12):dr.line((960,110,x,420),fill=(25,25,25,255),width=3)
 for y in range(470,1080,12):dr.line((650,y,1260,y),fill=(30,180,240,255),width=3)
 silhouette.save(out/'portrait-fixture.png');o.call('SetInputSettings',{'inputName':actorName,'inputSettings':{'file':str(out/'portrait-fixture.png')}});time.sleep(.2)
 shot('portrait-original')
 for algorithm,label in ((0,'gaussian'),(1,'box')):apply('FX01.0',{'radius':100,'algorithm':algorithm,'edge':2}).save(out/f'{label}-portrait.png')
 check(metrics()['failures']==0,'no GPU failures',metrics())
except Exception:check(False,'quality probe exception',traceback.format_exc())
finally:
 try:control.update({'layers':[layer,reference]})
 except Exception:pass
 o.close();(out/'blur-quality.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['pass'] for c in checks),'stage':stage},ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(any(not c['pass'] for c in checks))
