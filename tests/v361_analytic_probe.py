"""Real D3D11 coverage regressions for all low-cost mask families."""
import base64,io,json,sys,time,math,traceback
from pathlib import Path
from PIL import Image,ImageChops,ImageStat,ImageDraw
from obs_client import OBS
from native_control import Native
out,bridge=map(lambda s:Path(s).resolve(),sys.argv[1:3]);out.mkdir(parents=True,exist_ok=True)
checks=[]
def check(ok,name,value=None):
    checks.append(dict(name=name,**{'pass':bool(ok)},value=value));print(('PASS ' if ok else 'FAIL ')+name+' '+str(value or ''),flush=True)
def diff(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
o=OBS();stamp=str(time.time_ns());scene='V361 masks '+stamp;stage='V361 mask stage '+stamp
o.call('CreateScene',dict(sceneName=scene));path=out/'opaque.png';Image.new('RGBA',(1280,720),(45,190,220,255)).save(path)
source='V361 opaque '+stamp;o.call('CreateInput',dict(sceneName=scene,inputName=source,inputKind='image_source',inputSettings={'file':str(path)},sceneItemEnabled=False))
uuid=next(s['inputUuid'] for s in o.call('GetInputList')['inputs'] if s['inputName']==source)
scale=1500/(1080/(2*math.tan(math.radians(20))))
layer=dict(uuid=uuid,instance_id='mask-target',visible=True,enabled=True,sx=scale,sy=scale)
base=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,fx_enabled=False,playing=False,timeline_preview=True,layers=[layer])
o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=base,sceneItemEnabled=True));o.call('SetCurrentProgramScene',dict(sceneName=scene));time.sleep(.3)
n=Native(bridge,stage);images=[]
def put(reveal,t):
    g=dict(instance_id='reveal',shape=0,mode=1,width=1280,height=720,reveal=reveal,reveal_period_us='6000000',reveal_start_us='0',reveal_loop=True,local_mask=True,receivers=[{'instance_id':'mask-target'}],detail=42,softness=86.4 if reveal==2 else 1)
    n.request(dict(base,presets_geometries=[g],timeline_position_us=str(round(t*1e6))));time.sleep(.12)
def shot(name):
    d=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=640,imageHeight=360));raw=base64.b64decode(d['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(raw);return Image.open(io.BytesIO(raw)).convert('RGBA')
try:
    n.request(dict(base,presets_geometries=[]));time.sleep(.15);original=shot('unmasked-original')
    for i in range(1,23):
        put(i,0);a=shot('mask-'+str(i)+'-0');put(i,.66);b=shot('mask-'+str(i)+'-partial');put(i,3);c=shot('mask-'+str(i)+'-open');put(i,6);z=shot('mask-'+str(i)+'-end')
        check(a.getchannel('A').getextrema()[1]==0,'mask'+str(i)+' starts closed')
        alpha=ImageStat.Stat(b.getchannel('A')).mean[0];check(.01<alpha<100,'mask'+str(i)+' intermediate coverage',round(alpha,3))
        check(ImageStat.Stat(c.getchannel('A')).mean[0]>50,'mask'+str(i)+' opens visible region')
        check(diff(original,c)<.4,'mask'+str(i)+' fully open preserves original edge pixels',round(diff(original,c),4))
        check(diff(a,z)<.001,'mask'+str(i)+' loop closure')
        images.append(b)
    for i in range(22):
        for j in range(i):check(diff(images[i],images[j])>.001,'distinct mask coverage '+str(j+1)+' vs '+str(i+1),round(diff(images[i],images[j]),4))
    sheet=Image.new('RGB',(320*4,200*6),'#1d2430');d=ImageDraw.Draw(sheet)
    for i,p in enumerate(images):x=i%4*320;y=i//4*200;sheet.paste(p.convert('RGB').resize((320,180)),(x,y));d.text((x+4,y+182),'mask '+str(i+1),fill='white')
    sheet.save(out/'mask-contact-sheet.jpg')
except Exception:check(False,'analytic mask probe exception',traceback.format_exc())
finally:o.close()
(out/'analytic-tests.json').write_text(json.dumps(dict(checks=checks,failed=sum(not c['pass'] for c in checks)),ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(any(not c['pass'] for c in checks))
