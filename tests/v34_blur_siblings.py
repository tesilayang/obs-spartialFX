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
 for identifier in ('FX01.3','FX01.6','FX01.7','FX01.8'):
  pic=apply(identifier);pic.save(out/(identifier+'.png'));check(pic.getchannel('A').getextrema()[1]>0,'non-dense blur sibling retains visible source '+identifier)
 still=apply('FX01.6');check(delta(still,original)<.001,'zero-motion blur is exact identity',delta(still,original))
 check(metrics()['failures']==0,'blur siblings have zero GPU failures',metrics())
except Exception:check(False,'siblings probe exception',traceback.format_exc())
finally:
 try:control.update({'layers':[layer,reference]})
 except Exception:pass
 o.close();(out/'blur-siblings.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['pass'] for c in checks),'stage':stage},ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(any(not c['pass'] for c in checks))
