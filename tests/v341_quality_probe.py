"""Installed D3D11 pixel/resource/performance checks. Fixtures are synthetic QA data."""
import base64, copy, io, json, math, sys, time, traceback
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw
from obs_client import OBS
from native_control import Native

out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
bridge=Path(sys.argv[2]).resolve();checks=[];details={}
def check(ok,name,value=None):
 checks.append(dict(name=name,pass_=bool(ok),value=value));print(('PASS ' if ok else 'FAIL ')+name+' '+str(value or ''),flush=True)
def difference(a,b):return float(np.abs(np.asarray(a,dtype=float)-np.asarray(b,dtype=float)).mean())
w,h=1920,1080
y,x=np.mgrid[:h,:w];mask=x+.5>230+.367*(y+.5)
pixels=np.zeros((h,w,4),np.uint8);pixels[:,:,:3]=np.where(mask[:,:,None],220,25);pixels[:,:,3]=255
# Independent flat field/ramp/text fixtures, away from the tested diagonal.
pixels[40:260,1000:1800,:3]=110
for col in range(1000,1800):pixels[360:600,col,:3]=int(50+150*(col-1000)/800)
im=Image.fromarray(pixels);d=ImageDraw.Draw(im)
d.text((1000,700),'SPATIAL FX 3.4.1 / 0123456789',fill=(220,220,220,255),stroke_width=0)
for i in range(10):d.line((1000,800+i*12,1750,810+i*12),fill=(20,20,20,255),width=1)
im.save(out/'fixture.png')
# Analytical supersample reference for the same straight edge, only in its ROI.
yy,xx=np.mgrid[60:1000,180:650];coverage=np.zeros(xx.shape)
for j in range(8):
 for i in range(8):coverage+=(xx+(i+.5)/8>230+.367*(yy+(j+.5)/8))/64
ideal=25+195*coverage
boundary=(coverage>.001)&(coverage<.999)
o=OBS();stamp=str(time.time_ns());scene='V341 quality '+stamp;stage='V341 stage '+stamp
o.call('CreateScene',{'sceneName':scene})
name='V341 source '+stamp
o.call('CreateInput',{'sceneName':scene,'inputName':name,'inputKind':'image_source','inputSettings':{'file':str(out/'fixture.png')},'sceneItemEnabled':False})
uid=next(v['inputUuid'] for v in o.call('GetInputList')['inputs'] if v['inputName']==name)
scale=1500/(1080/(2*math.tan(math.radians(20))))
layer=dict(uuid=uid,instance_id='quality-actor',label='Quality fixture',sx=scale,sy=scale,x=0,y=0,z=0,visible=True,enabled=True)
base=dict(schema_version=5,width=w,height=h,camera_z=-1500,camera_fov=40,fx_enabled=False,light_enabled=False,playing=False,timeline_preview=False,layers=[layer])
o.call('CreateInput',{'sceneName':scene,'inputName':stage,'inputKind':'obs_spatial_fx_stage','inputSettings':base,'sceneItemEnabled':True})
o.call('SetCurrentProgramScene',{'sceneName':scene});time.sleep(.7);control=Native(bridge,stage)
def update(effects=None,**patch):
 s=dict(attached=True,group_states=[{'enabled':i==1} for i in range(13)],effects=effects or [])
 control.update(dict(layers=[dict(layer,fx_stack=s)],**patch));time.sleep(.15)
def effect(id,**parameters):return dict(id=id,enabled=True,strength=1,parameters=parameters)
def metrics():return control.get()['_metrics']
def shot(label):
 v=o.call('GetSourceScreenshot',{'sourceName':stage,'imageFormat':'png','imageWidth':w,'imageHeight':h})
 payload=base64.b64decode(v['imageData'].split(',',1)[1]);(out/(label+'.png')).write_bytes(payload)
 return Image.open(io.BytesIO(payload)).convert('RGBA')
def mse(a):
 values=np.asarray(a,dtype=float)[60:1000,180:650,0]
 return float(((values-ideal)[boundary]**2).mean())
def stable(label,expected_passes):
 time.sleep(.35);a=metrics();time.sleep(1);b=metrics();frames=b['renders']-a['renders'];passes=b['passes']-a['passes']
 check(b['allocations']==a['allocations'],label+' reuses all GPU textures after warmup',{'before':a,'after':b})
 check(frames>30 and abs(passes/frames-expected_passes)<.02,label+' executes expected passes per frame',dict(frames=frames,passes=passes))
 check(b['failures']==0,label+' has no shader/resource failure',b)
 return b
def performance(label,fx,seconds=8):
 update(fx);time.sleep(1);warm=metrics();start=o.call('GetStats');samples=[]
 for i in range(seconds):time.sleep(1);samples.append(o.call('GetStats'))
 end=metrics();last=samples[-1];frames=end['renders']-warm['renders']
 check(end['allocations']==warm['allocations'],label+' has stable allocations during timed 1080p60 run')
 check(frames>=seconds*50 and end['failures']==0,label+' sustains at least50fps in timed run',dict(frames=frames,seconds=seconds))
 details[label]=dict(before=start,samples=samples,warm=warm,after=end)
try:
 video=o.call('GetVideoSettings');check(video['baseWidth']==w and video['baseHeight']==h and video['fpsNumerator']/video['fpsDenominator']==60,'actual OBS canvas is1080p60',video)
 original=shot('original');a=metrics();time.sleep(.5);b=metrics()
 check(a['passes']==0 and a['allocations']==0 and b['passes']==0,'default-off performs no FX draw or allocation',b)
 check(difference(original,im)<.2,'fixture projects at original pixel scale',difference(original,im))
 update([dict(effect('FX02.2'),enabled=False)]);check(difference(original,shot('disabled'))<.001,'unchecked SMAA is pixel-identical')
 update([effect('FX02.2',quality=0,threshold=.15)]);smaa=shot('smaa-low');stable('SMAA Low',3)
 check(mse(smaa)<mse(original)*.85,'SMAA reduces diagonal error versus8x coverage reference',dict(before=mse(original),after=mse(smaa)))
 update([effect('FX02.2',quality=1,threshold=.1)]);smaa_medium=shot('smaa-medium');stable('SMAA Medium',3)
 update([effect('FX02.3',threshold=.1667,subpixel=.5)]);fxaa=shot('fxaa');stable('FXAA',1)
 check(mse(fxaa)<mse(original)*.9,'FXAA reduces diagonal error versus8x coverage reference',dict(before=mse(original),after=mse(fxaa)))
 check(difference(smaa,fxaa)>.001,'SMAA and FXAA produce distinct pixels',difference(smaa,fxaa))
 for label,picture in [('SMAA',smaa),('FXAA',fxaa)]:
  values=np.asarray(picture);check(np.max(np.abs(values[70:230,1050:1750,:].astype(int)-np.asarray(original)[70:230,1050:1750,:].astype(int)))<=1,label+' preserves constant fields')
 update([effect('FX02.4',amount=.6,denoise=1)]);rcas=shot('rcas');stable('RCAS',1)
 check(difference(rcas,original)>.001,'RCAS changes real image detail',difference(rcas,original))
 check(np.array_equal(np.asarray(rcas)[:,:,3],np.asarray(original)[:,:,3]),'RCAS preserves input alpha')
 update([effect('FX02.4',amount=0,denoise=1)]);check(difference(original,shot('rcas-zero'))<.001,'RCAS amount0 is identity')
 update([effect('FX02.2'),effect('FX02.3')]);stable('Conflicting imported AA resolves to SMAA only',3)
 update([effect('FX02.2'),effect('FX02.4')]);stable('SMAA then RCAS',4)
 # Transparent silhouettes, including hidden RGB, must remain bounded premultiplied output.
 transparent=np.asarray(im).copy();transparent[:,:,3]=np.where(mask,255,0);transparent[:,:,:3]=[210,100,30];transparent[~mask,:3]=[0,255,255]
 Image.fromarray(transparent).save(out/'transparent.png');o.call('SetInputSettings',{'inputName':name,'inputSettings':{'file':str(out/'transparent.png')},'overlay':True});time.sleep(.4)
 update([]);alpha_original=shot('alpha-original')
 for id in ['FX02.2','FX02.3','FX02.4']:
  update([effect(id)]);picture=shot('alpha-'+id);arr=np.asarray(picture);check(arr[:,:,3].min()==0 and arr[:,:,3].max()==255,id+' preserves fully transparent and opaque regions')
  if id!='FX02.4':check(np.count_nonzero((arr[:,:,3]>0)&(arr[:,:,3]<255))>100,id+' smooths alpha silhouettes')
  visible=arr[:,:,3]>20;check(np.abs(arr[visible,0].astype(int)-210).mean()<3 and np.abs(arr[visible,1].astype(int)-100).mean()<3,id+' avoids colored/dark fringe from hidden RGB')
 # Dark opaque color edges must not be hidden by an alpha luminance floor.
 for colorName,color in [('dark-gray',(55,55,55)),('red',(255,0,0)),('blue',(0,0,255))]:
  dark=np.zeros((h,w,4),np.uint8);dark[:,:,3]=255;dark[mask,:3]=color
  path=out/('dark-'+colorName+'.png');Image.fromarray(dark).save(path)
  o.call('SetInputSettings',{'inputName':name,'inputSettings':{'file':str(path)},'overlay':True});time.sleep(.25)
  update([]);before=shot(colorName+'-original');update([effect('FX02.3')]);after=shot(colorName+'-fxaa')
  check(difference(before,after)>.001,'FXAA processes opaque '+colorName+' edge without alpha floor',difference(before,after))
 o.call('SetInputSettings',{'inputName':name,'inputSettings':{'file':str(out/'fixture.png')},'overlay':True});time.sleep(.3)
 for label,fx in [('baseline',[]),('fxaa',[effect('FX02.3')]),('smaa-low',[effect('FX02.2')]),('rcas',[effect('FX02.4')]),('fxaa-rcas',[effect('FX02.3'),effect('FX02.4')])]:performance(label,fx)
 # Source size changes allocate once, then both AA algorithms settle again.
 im.resize((1280,720)).save(out/'resized.png')
 o.call('SetInputSettings',{'inputName':name,'inputSettings':{'file':str(out/'resized.png')},'overlay':True});time.sleep(.3)
 update([effect('FX02.2')]);stable('SMAA after source resize',3)
 update([effect('FX02.3')]);stable('FXAA after source resize',1)
 o.call('SetInputSettings',{'inputName':name,'inputSettings':{'file':str(out/'fixture.png')},'overlay':True});time.sleep(.3)
 update([effect('FX02.2')]);stable('SMAA after source size restore',3)
 first=control.get()['layers'][0];second=copy.deepcopy(first);second['instance_id']='quality-twin';second['x']=300
 control.update({'layers':[first,second]});stable('Two SMAA instances share immutable LUTs only',6)
 update([]);stable('All effects disabled again',0)
except Exception:check(False,'quality probe exception',traceback.format_exc())
finally:
 try:control.update({'layers':[layer]})
 except Exception:pass
 o.close()
 report=dict(checks=[{'name':c['name'],'pass':c['pass_'],'value':c['value']} for c in checks],failed=sum(not c['pass_'] for c in checks),performance=details,scene=scene,stage=stage,hardwareScope='Actual host GPU only. Integrated GPU NOT RUN.')
 (out/'quality-tests.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(bool(report['failed']))
