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
def ui(action,**args):
 import uuid,os
 token=uuid.uuid4().hex;temp=bridge/'native-command.tmp';temp.write_text(json.dumps(dict(id=token,source=stage,action=action,**args)),encoding='utf-8')
 # QFile may briefly hold the prior command without FILE_SHARE_DELETE on Windows.
 # Match Native.request's bounded retry; never hide a permanent publication failure.
 for attempt in range(100):
  try:os.replace(temp,bridge/'native-command.json');break
  except PermissionError:
   if attempt==99:raise
   time.sleep(.02)
 deadline=time.monotonic()+10
 while time.monotonic()<deadline:
  try:
   answer=json.loads((bridge/'native-answer.json').read_text(encoding='utf-8-sig'))
   if answer.get('id')==token:
    if not answer.get('ok'):raise RuntimeError('UI bridge failed '+action)
    time.sleep(.2);return answer.get('state',{})
  except (FileNotFoundError,json.JSONDecodeError,PermissionError):pass
  time.sleep(.03)
 raise TimeoutError(action)
try:
 neutral=Image.new('RGBA',(1920,1080));draw=ImageDraw.Draw(neutral);draw.rounded_rectangle((240,100,1680,980),80,fill=(65,65,65,255));neutral.save(out/'neutral.png');o.call('SetInputSettings',{'inputName':actorName,'inputSettings':{'file':str(out/'neutral.png')}})
 for identifier in ('FX05.1','FX05.2','FX05.3'):
  params={'gain':3};params.update({'flicker':0,'warmth':0} if identifier=='FX05.2' else {})
  st=stack(defs[identifier],params);st['effects'][0]['color']={'r':.4,'g':.5,'b':.6}
  update({'layers':layersfx(st),'light_enabled':False,'fx_enabled':False},.4);ui('ui-refresh');original=shot(identifier+'-original')
  widget='spatial.fx34.gpu-actor.'+identifier+'.color'
  ui('fx-color-open',widget=widget);ui('fx-color-set',color='#FF0000');red=shot(identifier+'-live-red')
  ui('fx-color-set',color='#0000FF');blue=shot(identifier+'-live-blue')
  check(delta(red,blue)>.5,identifier+' GPU pixels preview new color while picker is open',delta(red,blue))
  ui('fx-color-cancel');cancelled=shot(identifier+'-cancelled');check(delta(original,cancelled)<.001,identifier+' cancel restores actual original GPU pixels',delta(original,cancelled))
  ui('fx-color-open',widget=widget);ui('fx-color-set',color='#23B869');ui('fx-color-accept');accepted=shot(identifier+'-accepted')
  check(delta(accepted,original)>.1,identifier+' accepted color remains visible after closing picker',delta(accepted,original))
  control.current=None;active=control.get();color=active['layers'][0]['fx_stack']['effects'][0]['color'];check(abs(color['g']-184/255)<.005,identifier+' accepts exact selected color in saved state',color)
 check(metrics()['failures']==0,'live color preview has no GPU shader failures',metrics())
except Exception:check(False,'live color probe exception',traceback.format_exc())
finally:
 try:ui('fx-color-cancel')
 except Exception:pass
 try:control.update({'layers':[layer,reference]})
 except Exception:pass
 o.close();(out/'live-colors.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['pass'] for c in checks)},ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(any(not c['pass'] for c in checks))
