"""GPU edge contracts: HDR neutral tint, source-pixel backlight and combined reuse."""
import base64,copy,io,json,math,sys,time,traceback
from pathlib import Path
from PIL import Image,ImageDraw,ImageChops,ImageStat
from obs_client import OBS
from native_control import Native

out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True);bridge=Path(sys.argv[2]).resolve();checks=[]
def check(ok,name,value=None):
 checks.append({'name':name,'pass':bool(ok),'value':value});print(('PASS ' if ok else 'FAIL ')+name+' '+str(value),flush=True)
def diff(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
for name,color in [('hdr',(192,32,32,255)),('white',(255,255,255,255)),('black',(0,0,0,255)),('clear',(0,0,0,0))]:Image.new('RGBA',(1920,1080),color).save(out/(name+'.png'))
im=Image.new('RGBA',(1920,1080));ImageDraw.Draw(im).rectangle((550,180,1370,900),fill=(70,80,90,255));im.save(out/'card.png');im.resize((3840,2160),Image.Resampling.NEAREST).save(out/'card4k.png')
o=OBS();stamp=str(time.time_ns());scene='FX342 boundaries '+stamp;stage='FX342 boundary stage '+stamp;actor='boundary actor '+stamp;ref='boundary reference '+stamp
o.call('CreateScene',{'sceneName':scene})
def source(name,path):
 o.call('CreateInput',{'sceneName':scene,'inputName':name,'inputKind':'image_source','inputSettings':{'file':str(path)},'sceneItemEnabled':False})
 return next(i['inputUuid'] for i in o.call('GetInputList')['inputs'] if i['inputName']==name)
uid=source(actor,out/'hdr.png');rid=source(ref,out/'white.png');scale=1500/(1080/(2*math.tan(math.radians(20))))
layer={'uuid':uid,'instance_id':'boundary-actor','sx':scale,'sy':scale,'visible':True,'enabled':True,'receive_light':False,'receive_shadow':False};hidden={'uuid':rid,'instance_id':'boundary-reference','visible':False,'enabled':False}
settings={'schema_version':5,'width':1920,'height':1080,'camera_z':-1500,'camera_fov':40,'fx_enabled':False,'light_enabled':False,'playing':False,'timeline_preview':False,'layers':[layer,hidden]}
o.call('CreateInput',{'sceneName':scene,'inputName':stage,'inputKind':'obs_spatial_fx_stage','inputSettings':settings,'sceneItemEnabled':True});o.call('SetCurrentProgramScene',{'sceneName':scene});time.sleep(.5);n=Native(bridge,stage)
def fx(id,p=None,reference=''):return {'id':id,'enabled':True,'parameters':p or {},'reference':reference}
def apply(effects=(),label='capture',factor=1):
 a=copy.deepcopy(layer);a['sx']/=factor;a['sy']/=factor
 a['fx_stack']={'attached':True,'group_states':[{'enabled':any(int(f['id'][2:4])==g for f in effects)}for g in range(1,14)],'effects':list(effects)}
 n.update({'layers':[a,hidden]});time.sleep(.2);data=o.call('GetSourceScreenshot',{'sourceName':stage,'imageFormat':'png','imageWidth':960,'imageHeight':540});b=base64.b64decode(data['imageData'].split(',')[1]);(out/(label+'.png')).write_bytes(b);return Image.open(io.BytesIO(b)).convert('RGBA')
def texture(name):o.call('SetInputSettings',{'inputName':actor,'inputSettings':{'file':str(out/(name+'.png'))}});time.sleep(.3)
try:
 grade=fx('FX03.0',{'exposure':2});plain=apply([grade],'hdr-original')
 for amount in (0,1):
  result=apply([grade,fx('FX05.0',{'amount':amount,'direction':0},'boundary-reference')],'hdr-environment-'+str(amount))
  check(diff(plain,result)<.2,'HDR exposure plus neutral environment amount '+str(amount)+' preserves RGB',{'difference':diff(plain,result),'before':plain.getpixel((480,270)),'after':result.getpixel((480,270))})
 texture('card');p={'radius':60,'light_x':325,'light_y':150,'light_z':200,'gain':2,'scale':1};normal=apply([fx('FX05.1',p)],'backlight-1080')
 texture('card4k');high=apply([fx('FX05.1',dict(p,radius=120,light_x=650,light_y=300,light_z=400))],'backlight-4k',factor=2)
 check(diff(normal,high)<1.5,'4K capture preserves proportional source-pixel radius and XYZ',{'difference':diff(normal,high)})
 texture('card');plain=apply(label='card-original');zero=apply([fx('FX05.1',dict(p,radius=0,gain=0))],'backlight-neutral');check(diff(plain,zero)<.1,'zero radius and zero gain preserve original')
 texture('clear');clear=apply([fx('FX05.1',p)],'backlight-empty');check(clear.getchannel('A').getextrema()==(0,0),'empty alpha creates no stray backlight',clear.getchannel('A').getextrema())
 texture('black');plain=apply(label='black-original');black=apply([fx('FX05.0',{'amount':1},'boundary-reference')],'environment-black');check(diff(plain,black)<.1,'black subject remains black under neutral environment')
 texture('card');chain=[fx('FX03.4',{'gamma':1.2}),fx('FX05.0',{'amount':.7,'radius':120},'boundary-reference'),fx('FX05.1',dict(p,radius=150))];apply(chain,'combined-performance');time.sleep(2);warm=n.get()['_metrics'];before=o.call('GetStats');samples=[]
 for _ in range(30):time.sleep(1);samples.append(o.call('GetStats'))
 after=n.get()['_metrics'];check(warm['allocations']==after['allocations'] and after['failures']==0,'combined levels/environment/backlight resources plateau',{'warm':warm,'after':after})
 check(min(s['activeFps'] for s in samples)>59 and samples[-1]['renderSkippedFrames']==before['renderSkippedFrames'],'combined effects sustain 1080p60 for 30 seconds',{'minimumFps':min(s['activeFps'] for s in samples),'maxRenderMs':max(s['averageFrameRenderTime'] for s in samples),'renderDrops':samples[-1]['renderSkippedFrames']-before['renderSkippedFrames']})
 (out/'performance.json').write_text(json.dumps({'before':before,'samples':samples,'warmMetrics':warm,'afterMetrics':after},indent=2),encoding='utf-8')
except Exception:check(False,'boundary probe exception',traceback.format_exc())
finally:
 o.close();(out/'boundary-tests.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['pass'] for c in checks)},ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(any(not c['pass'] for c in checks))
