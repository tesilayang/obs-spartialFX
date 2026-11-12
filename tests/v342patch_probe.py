"""Real OBS33 levels, environment-material response and continuous backlight tests."""
import base64,copy,io,json,math,sys,time,traceback
from pathlib import Path
from PIL import Image,ImageDraw,ImageChops,ImageStat
from obs_client import OBS
from native_control import Native
out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
bridge=Path(sys.argv[2]).resolve();checks=[]
def check(ok,name,value=None):
 checks.append({'name':name,'pass':bool(ok),'value':value});print(('PASS ' if ok else 'FAIL ')+name+': '+str(value),flush=True)
def diff(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean[:3])
image=Image.new('RGBA',(1920,1080));d=ImageDraw.Draw(image)
for x in range(1920):
 v=round(255*x/1919);d.line((x,0,x,1079),fill=(v,v,v,255))
image.save(out/'ramp.png')
sil=Image.new('RGBA',(1920,1080));d=ImageDraw.Draw(sil);d.rectangle((550,180,1370,900),fill=(70,80,90,255));sil.save(out/'silhouette.png')
detail=sil.copy();d=ImageDraw.Draw(detail)
for x in range(600,1300,12):d.rectangle((x,300,x+5,780),fill=(140,140,140,255))
detail.save(out/'detail.png')
for name,color in [('red',(240,55,25,255)),('blue',(25,55,240,255)),('white',(255,255,255,255))]:Image.new('RGBA',(1920,1080),color).save(out/(name+'.png'))
checker=Image.new('RGBA',(1920,1080));d=ImageDraw.Draw(checker)
for y in range(0,1080,16):
 for x in range(0,1920,16):d.rectangle((x,y,x+15,y+15),fill=(240,55,25,255) if (x//16+y//16)%2 else (25,55,240,255))
checker.save(out/'checker.png')
o=OBS();stamp=str(time.time_ns());scene='FX342patch '+stamp;stage='FX342patch stage '+stamp
o.call('CreateScene',{'sceneName':scene})
def source(name,path):
 name+=' '+stamp;o.call('CreateInput',{'sceneName':scene,'inputName':name,'inputKind':'image_source','inputSettings':{'file':str(path)},'sceneItemEnabled':False})
 return name,next(a['inputUuid'] for a in o.call('GetInputList')['inputs'] if a['inputName']==name)
actor,uuid=source('actor',out/'ramp.png');reference,refid=source('environment',out/'checker.png')
scale=1500/(1080/(2*math.tan(math.radians(20))))
layer={'uuid':uuid,'instance_id':'patch-actor','sx':scale,'sy':scale,'visible':True,'enabled':True,'receive_light':False,'receive_shadow':False}
hidden={'uuid':refid,'instance_id':'patch-reference','visible':False,'enabled':False}
base={'schema_version':5,'width':1920,'height':1080,'camera_z':-1500,'camera_fov':40,'fx_enabled':False,'playing':False,'timeline_preview':False,'light_enabled':False,'layers':[layer,hidden]}
o.call('CreateInput',{'sceneName':scene,'inputName':stage,'inputKind':'obs_spatial_fx_stage','inputSettings':base,'sceneItemEnabled':True});o.call('SetCurrentProgramScene',{'sceneName':scene});time.sleep(.6);n=Native(bridge,stage)
def shot(name):
 data=o.call('GetSourceScreenshot',{'sourceName':stage,'imageFormat':'png','imageWidth':960,'imageHeight':540})
 b=base64.b64decode(data['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(b);return Image.open(io.BytesIO(b)).convert('RGBA')
def apply(identifier=None,params=None,name='capture',**extra):
 a=copy.deepcopy(layer)
 if identifier:
  groups=[{'enabled':False} for _ in range(13)];groups[int(identifier[2:4])-1]['enabled']=True
  a['fx_stack']={'attached':True,'group_states':groups,'effects':[dict({'id':identifier,'enabled':True,'strength':1,'parameters':params or {}},**extra)]}
 n.update({'layers':[a,hidden]});time.sleep(.15);return shot(name)
def texture(name):o.call('SetInputSettings',{'inputName':actor,'inputSettings':{'file':str(out/(name+'.png'))}});time.sleep(.2)
def env(name):o.call('SetInputSettings',{'inputName':reference,'inputSettings':{'file':str(out/(name+'.png'))}});time.sleep(.2)
try:
 plain=apply(name='levels-original')
 identity={'channel':0,'input_black':0,'input_white':1,'output_black':0,'output_white':1,'gamma':1}
 same=apply('FX03.4',identity,'levels-identity');check(diff(plain,same)<.2,'levels identity preserves ramp',diff(plain,same))
 for key,value in [('input_black',.25),('input_white',.75),('output_black',.3),('output_white',.6),('gamma',1.8)]:
  p=dict(identity,**{key:value});b=apply('FX03.4',p,'levels-'+key)
  x=480;raw=plain.getpixel((x,270))[0]/255
  t=max(0,min(1,(raw-p['input_black'])/max(p['input_white']-p['input_black'],1e-6)))
  expected=round(255*(p['output_black']+(p['output_white']-p['output_black'])*t**(1/p['gamma'])))
  actual=b.getpixel((x,270))[0];check(abs(actual-expected)<=3,'levels '+key+' follows normalized input/gamma/output mapping',{'expected':expected,'actual':actual})
 for name,value in [('black',0),('white',1)]:
  p=dict(identity,output_black=value,output_white=value);b=apply('FX03.4',p,'levels-pure-'+name)
  check(b.getchannel('R').getextrema()==(int(value*255),int(value*255)),'equal output endpoints produce pure '+name,b.getchannel('R').getextrema())
 for limits in [(1,1),(.8,.2)]:
  b=apply('FX03.4',dict(identity,input_black=limits[0],input_white=limits[1]),'levels-degenerate-'+str(limits[0]));check(b.getbbox() is not None,'degenerate input levels remain finite',b.getchannel('R').getextrema())
 texture('silhouette');env('checker');plain=apply(name='environment-original')
 p={'amount':1,'radius':120,'direction':0,'scale':1}
 b=apply('FX05.0',p,'environment-checker',reference='patch-reference')
 crop=b.crop((350,180,610,380));sd=ImageStat.Stat(crop).stddev[:3]
 check(max(sd)<2,'environment does not copy reference checker texture onto flat material',sd)
 check(ImageChops.difference(plain.getchannel('A'),b.getchannel('A')).getbbox() is None,'environment preserves subject alpha')
 env('red');red=apply('FX05.0',p,'environment-red',reference='patch-reference');env('blue');blue=apply('FX05.0',p,'environment-blue',reference='patch-reference')
 rp=red.getpixel((480,270));bp=blue.getpixel((480,270));check(rp[0]>rp[2]+10 and bp[2]>bp[0]+10,'environment changes material tint with selected source',{'red':rp,'blue':bp})
 texture('detail');env('red');plain=apply(name='detail-original');lit=apply('FX05.0',p,'detail-lit',reference='patch-reference')
 lum=lambda p:.2126*p[0]+.7152*p[1]+.0722*p[2]
 old=lum(plain.getpixel((301,250)))-lum(plain.getpixel((304,250)));new=lum(lit.getpixel((301,250)))-lum(lit.getpixel((304,250)))
 check(abs(new)>abs(old)*.7,'environment retains subject fine contrast',{'before':old,'after':new})
 texture('silhouette');plain=apply(name='back-original')
 p={'radius':120,'light_x':0,'light_y':0,'light_z':200,'gain':2,'scale':1}
 back=apply('FX05.1',p,'back-soft')
 row=[back.getpixel((x,270))[3] for x in range(205,275)]
 jumps=[abs(row[i+1]-row[i]) for i in range(len(row)-1)]
 check(max(jumps)<14,'large backlight radius has continuous alpha without shifted silhouette bands',{'maxStep':max(jumps),'profile':row})
 check(max(row)>3,'backlight spreads beyond the subject alpha',max(row))
 colors={'r':.1,'g':.3,'b':1}
 colored=apply('FX05.1',p,'back-blue',color=colors);q=colored.getpixel((270,270));check(q[2]>q[0]+10,'backlight color controls visible halo',q)
 samples={}
 for key,lo,hi in [('light_x',-650,650),('light_y',-500,500),('light_z',80,900)]:
  a=apply('FX05.1',dict(p,**{key:lo}),key+'-lo');b=apply('FX05.1',dict(p,**{key:hi}),key+'-hi');samples[key]=(a,b)
  check(diff(a,b)>.05,'backlight '+key+' independently changes illumination',diff(a,b))
 a,b=samples['light_x'];left=(278,260,292,280);right=(669,260,683,280)
 energy=lambda img,box:sum(ImageStat.Stat(ImageChops.subtract(img,plain).crop(box)).mean[:3])
 check(energy(a,left)>energy(a,right) and energy(b,right)>energy(b,left),'backlight X swaps illuminated rim side',{'negative':[energy(a,left),energy(a,right)],'positive':[energy(b,left),energy(b,right)]})
 off=apply('FX05.1',dict(p,gain=0),'back-zero');check(diff(off,plain)<.1 and ImageChops.difference(off.getchannel('A'),plain.getchannel('A')).getbbox() is None,'zero backlight gain is exact bypass')
 n.update({'fx_enabled':True,'light_enabled':True,'light_intensity':0,'light_range':1});on=apply('FX05.1',p,'back-stage-on');n.update({'fx_enabled':False});off=apply('FX05.1',p,'back-stage-off');check(diff(on,off)<.1,'new backlight stays independent of stage lighting')
 apply('FX05.1',dict(p,radius=150),'back-performance');time.sleep(2);warm=n.get()['_metrics'];before=o.call('GetStats');samples=[]
 for _ in range(30):time.sleep(1);samples.append(o.call('GetStats'))
 after=n.get()['_metrics'];check(warm['allocations']==after['allocations'] and after['failures']==0,'backlight reuses resources during real 1080p60 run',{'warm':warm,'after':after})
 check(min(s['activeFps'] for s in samples)>59 and samples[-1]['renderSkippedFrames']==before['renderSkippedFrames'],'backlight 30 second 1080p60 steady test',{'minFps':min(s['activeFps'] for s in samples),'maxRenderMs':max(s['averageFrameRenderTime'] for s in samples),'skipped':samples[-1]['renderSkippedFrames']-before['renderSkippedFrames']})
 (out/'performance.json').write_text(json.dumps({'before':before,'samples':samples,'warmMetrics':warm,'afterMetrics':after},indent=2),encoding='utf-8')
except Exception as exc:check(False,'probe exception',repr(exc));traceback.print_exc()
finally:
 (out/'patch-tests.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['pass'] for c in checks)},ensure_ascii=False,indent=2),encoding='utf-8');o.close()
sys.exit(int(any(not c['pass'] for c in checks)))
