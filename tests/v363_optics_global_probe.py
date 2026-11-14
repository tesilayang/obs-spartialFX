"""Optical padding must preserve world-space receiver and caster projection."""
import base64,copy,io,json,sys,time,traceback
from pathlib import Path
from PIL import Image,ImageDraw,ImageChops,ImageStat
from obs_client import OBS
from v361_native import UI
out,bridge=map(lambda x:Path(x).resolve(),sys.argv[1:3]);out.mkdir(parents=True,exist_ok=True)
o=OBS();checks=[];stamp=str(time.time_ns());scene='Optics projection '+stamp;stage='Optics projection stage '+stamp
def check(ok,name,value=None):
 checks.append(dict(name=name,**{'pass':bool(ok)},value=value));print(('PASS ' if ok else 'FAIL ')+name+' '+str(value or ''),flush=True)
def diff(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
o.call('CreateScene',dict(sceneName=scene));ids=[]
for label,size in [('receiver',(1920,1080)),('caster',(240,500))]:
 path=out/(label+'.png');im=Image.new('RGBA',size,(180,180,180,255) if label=='receiver' else (0,0,0,0))
 if label=='caster':ImageDraw.Draw(im).rectangle((70,20,169,479),fill=(90,120,170,255))
 im.save(path);name=label+' '+stamp;o.call('CreateInput',dict(sceneName=scene,inputName=name,inputKind='image_source',inputSettings={'file':str(path)},sceneItemEnabled=False));ids.append(next(i['inputUuid'] for i in o.call('GetInputList')['inputs'] if i['inputName']==name))
layers=[dict(uuid=ids[0],instance_id='receiver',z=700,sx=1.5,sy=1.5,cast_shadow=False,rim_light=False),dict(uuid=ids[1],instance_id='caster',x=-280,z=0,cast_shadow=True,rim_light=False)]
base=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,fx_enabled=True,playing=False,timeline_preview=True,layers=layers,light_enabled=True,light_x=-1000,light_y=0,light_z=-500,light_intensity=0,light_ambient=1,light_size=0,light_rim=0,light_softness=80,shadow_opacity=1,shadows_enabled=True,receiver_instance='receiver',receiver_uuid=ids[0])
o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=base,sceneItemEnabled=True));o.call('SetCurrentProgramScene',dict(sceneName=scene));time.sleep(.5);n=UI(bridge,stage)
def stack(identifier,parameters):
 groups=[dict(enabled=False) for _ in range(13)];groups[int(identifier[2:4])-1]['enabled']=True
 return dict(attached=True,group_states=groups,effects=[dict(id=identifier,enabled=True,strength=1,optics_revision=1,parameters=parameters)])
def shot(name,**patch):
 m=copy.deepcopy(base);m.update(patch);n.request(m);time.sleep(.2)
 data=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=1920,imageHeight=1080));raw=base64.b64decode(data['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(raw);return Image.open(io.BytesIO(raw)).convert('RGBA')
try:
 plain=shot('global-shadow-original');unshadowed=shot('no-global-shadow',shadow_opacity=0)
 check(diff(plain,unshadowed)>.1,'global shadow visible in baseline',diff(plain,unshadowed))
 neutral=stack('FX04.0',dict(radius=160,threshold=1,gain=2))
 for receiver in [False,True]:
  ll=copy.deepcopy(layers);ll[0 if receiver else 1]['fx_stack']=neutral
  padded=shot('padded-'+('receiver' if receiver else 'caster'),layers=ll)
  check(diff(plain,padded)<.3,'neutral padding preserves global '+('receiver' if receiver else 'caster')+' projection',diff(plain,padded))
 # A visible 2D outer shadow does not enlarge the global caster silhouette.
 ll=copy.deepcopy(layers);ll[1]['fx_stack']=stack('FX05.5',dict(mode=0,angle=0,distance=80,radius=40,spread=20,opacity=1))
 local=shot('local-and-global-shadow',layers=ll)
 check(diff(plain.crop((1000,0,1900,1080)),local.crop((1000,0,1900,1080)))<.1,'local shadow never creates a second world-space occluder',diff(plain.crop((1000,0,1900,1080)),local.crop((1000,0,1900,1080))))
 mask=dict(instance_id='caster-mask',shape=0,mode=1,width=120,height=500,local_mask=True,receivers=[{'instance_id':'caster'}],softness=1,enabled=True,visible=True)
 a=shot('masked-caster-original',presets_geometries=[mask]);ll=copy.deepcopy(layers);ll[1]['fx_stack']=neutral
 b=shot('masked-padded-caster',presets_geometries=[mask],layers=ll)
 check(diff(a,b)<.3,'caster geometry mask retained when optics allocate separate surface',diff(a,b))
 check(n.get()['_metrics']['failures']==0,'projection and mask pipelines have no GPU failures',n.get()['_metrics'])
except Exception:check(False,'global projection probe exception',traceback.format_exc())
finally:
 (out/'projection-tests.json').write_text(json.dumps(dict(checks=checks,failed=sum(not c['pass'] for c in checks)),ensure_ascii=False,indent=2),encoding='utf-8');o.close()
raise SystemExit(any(not c['pass'] for c in checks))
