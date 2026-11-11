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
try:
 video=o.call('GetVideoSettings');check(video['baseWidth']==1920 and video['baseHeight']==1080 and video['fpsNumerator']/video['fpsDenominator']==60,'actual video configuration is 1920x1080 at60fps',video)
 original=shot('original');m0=metrics();check(m0['allocations']==0 and m0['passes']==0,'empty scene allocates no FX surfaces and runs no FX pass',m0)
 d0=catalog[0];update({'layers':layersfx(stack(d0,on=False))});check(delta(original,shot())<.001,'unchecked child preserves original pixels')
 update({'layers':layersfx(stack(d0,group=False))});check(delta(original,shot())<.001,'disabled category preserves original pixels')
 # Compile/render each effect and every discrete mode. Pixel identity defaults are additionally perturbed.
 perturb={'FX03.0':{'exposure':1,'saturation':.3},'FX03.1':{'red_hue':70,'green_saturation':.1},'FX03.2':{'temperature':1},'FX10.0':{'tl_x':.18},'FX12.9':{'mode':1}}
 for definition in catalog:
  identifier=definition['id'];variants=[('default',{})]
  for mode in definition.get('modes',[]):
   variants.extend((mode['key']+'-'+str(i),{mode['key']:i}) for i in range(mode['count']))
  biggest=0;rendered=False;start=metrics();before=shot();defaultVisible=False
  for label_,params in variants:
   params.update(perturb.get(identifier,{}));update({'layers':layersfx(stack(definition,params))},.2)
   picture=shot(identifier+'-'+label_);difference=delta(original,picture);biggest=max(biggest,difference)
   rendered=rendered or picture.getbbox() is not None
   if label_=='default':defaultVisible=picture.getchannel('A').getextrema()[1]>0
  if biggest<.002:
   limits=definition.get('parameter_limits',{})
   for key,limit in limits.items():
    if any(mode['key']==key for mode in definition.get('modes',[])):continue
    value=limit['max'] if abs(limit['max']-limit['initial'])>abs(limit['min']-limit['initial']) else limit['min']
    update({'layers':layersfx(stack(definition,{key:value}))},.18);picture=shot();biggest=max(biggest,delta(original,picture))
    if biggest>.002:picture.save(out/(identifier+'-perturbed.png'));break
  end=metrics();compiled=end['failures']==start['failures'];progress=end['passes']>start['passes'] if not definition['geometry'] and not definition.get('stage') else True
  check(compiled and progress,'GPU renders '+identifier+' '+definition['name'],{'modes':len(variants),'pixelDifference':round(biggest,4),'failures':end['failures']})
  check(defaultVisible,'nonempty default output '+identifier)
  # Identity/temporal and reference-dependent behavior is recorded separately; never infer visual correctness from compile.
  results.append({'id':identifier,'name':definition['name'],'gpuPass':compiled and progress,'defaultVisible':defaultVisible,'variants':len(variants),'maxPixelDifference':biggest,'visibleChange':biggest>.002,'geometry':definition['geometry'],'history':definition['history']})
 update({'layers':[layer,reference]});check(delta(original,shot())<.001,'removing all FX restores original pixels exactly')
 # Source dependencies are captured even when hidden/disabled/transparent.
 mask=next(d for d in catalog if d['id']=='FX12.2')
 update({'layers':layersfx(stack(mask))});hidden=shot('hidden-mask-reference');shown=[dict(layer,fx_stack=stack(mask)),dict(reference,visible=True,enabled=True,opacity=1,x=20000)]
 update({'layers':shown});check(delta(hidden,shot())<.001,'hidden auxiliary matches enabled offstage auxiliary raw pixels')
 check(delta(original,hidden)>.1,'hidden auxiliary actually affects the mask/composite result')
 # Per-instance independence for the same OBS source.
 left=dict(layer,x=-480*scale,sx=scale/2,sy=scale/2);right=dict(left,x=480*scale,instance_id='gpu-twin')
 update({'layers':[left,right]});twins=shot('twins-original')
 invert=next(d for d in catalog if d['id']=='FX03.8');update({'layers':[dict(left,fx_stack=stack(invert)),right]});changed=shot('twins-independent')
 check(delta(twins.crop((320,0,640,360)),changed.crop((320,0,640,360)))<.001 and delta(twins.crop((0,0,320,360)),changed.crop((0,0,320,360)))>.1,'same-source instances have independent FX pixels')
 # History holds the effect output, not repeatedly strength-mixed display.
 hold=next(d for d in catalog if d['id']=='FX09.4');holdstack=stack(hold,{'update_fps':1},strength=.5)
 update({'layers':layersfx(holdstack)},1.1);held0=shot('hold-before-source-change');o.call('SetInputSettings',{'inputName':actorName,'inputSettings':{'file':str(out/'alternate.png')}});time.sleep(.15);held1=shot('hold-after-source-change')
 historySamples=[held1];historyStart=time.monotonic()
 for i in range(30):time.sleep(.06);historySamples.append(shot())
 historyElapsed=time.monotonic()-historyStart;changes=[delta(a,b) for a,b in zip(historySamples,historySamples[1:])];jumps=sum(v>.05 for v in changes)
 check(jumps<=math.ceil(historyElapsed)+1,'half-strength held temporal output has stable plateaus, no recursive drift',{'samples':len(historySamples),'changedSamples':jumps,'seconds':historyElapsed})
 check(delta(held0,held1)>.1,'half-strength held output still mixes the current source',delta(held0,held1));o.call('SetInputSettings',{'inputName':actorName,'inputSettings':{'file':str(out/'fixture.png')}})
 # Representative steady-state resource reuse at actual1080p60 (not all92 effects simultaneously).
 chain=[next(d for d in catalog if d['id']==identifier) for identifier in ('FX01.0','FX03.0','FX04.4')]
 s=stack(chain[0]);s['group_states']=[{'enabled':True} for _ in range(13)];s['effects']=[stack(d)['effects'][0] for d in chain]
 update({'layers':layersfx(s)},2);warm=metrics();stats0=o.call('GetStats');samples=[]
 for i in range(30):time.sleep(1);samples.append(o.call('GetStats'))
 cold=metrics();stats1=samples[-1];check(cold['allocations']==warm['allocations'],'steady FX chain reuses textures for30seconds',{'warm':warm,'after':cold})
 check(cold['renders']-warm['renders']>=1400 and cold['failures']==0,'1080p60 render remains alive without shader failures',{'frames':cold['renders']-warm['renders'],'elapsedSeconds':30})
 (out/'performance.json').write_text(json.dumps({'before':stats0,'samples':samples,'warmMetrics':warm,'afterMetrics':cold},indent=2),encoding='utf-8')
 shot('final-chain')
except Exception:
 check(False,'probe exception',traceback.format_exc())
finally:
 try:control.update({'layers':[layer,reference]})
 except Exception:pass
 o.close()
 report={'checks':checks,'failed':sum(not c['pass'] for c in checks),'effects':results,'scene':scene,'stage':stage,'assetNotes':'Synthetic QA fixtures; GPU compile success does not by itself prove artistic acceptance.'}
 (out/'render-tests.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(bool(report['failed']))
