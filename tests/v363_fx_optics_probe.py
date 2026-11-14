"""Real OBS/D3D silhouette spill, local shadow and color-selected star regressions."""
import base64,copy,io,json,math,sys,time,traceback
from pathlib import Path
from PIL import Image,ImageDraw,ImageChops,ImageStat
from obs_client import OBS
from v361_native import UI

out,bridge=map(lambda x:Path(x).resolve(),sys.argv[1:3]);out.mkdir(parents=True,exist_ok=True)
checks=[];o=OBS();stamp=str(time.time_ns());stage='Optics stage '+stamp;scene='Optics QA '+stamp
def check(ok,name,value=None):
 checks.append(dict(name=name,**{'pass':bool(ok)},value=value));print(('PASS ' if ok else 'FAIL ')+name+' '+str(value or ''),flush=True)
def write(name,value):(out/name).write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf-8')
def delta(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
o.call('CreateScene',dict(sceneName=scene))
path=out/'fixture.png';im=Image.new('RGBA',(400,300));d=ImageDraw.Draw(im)
d.rectangle((0,60,95,239),fill=(255,255,255,255))
d.rectangle((155,80,244,219),fill=(255,35,35,255));d.rectangle((305,80,394,219),fill=(35,35,255,255));im.save(path)
actor='Optics fixture '+stamp;o.call('CreateInput',dict(sceneName=scene,inputName=actor,inputKind='image_source',inputSettings={'file':str(path)},sceneItemEnabled=False))
uuid=next(s['inputUuid'] for s in o.call('GetInputList')['inputs'] if s['inputName']==actor)
# 1 source pixel == 1 output pixel. Source rectangle occupies [760,1160]x[390,690].
scale=1500/(1080/(2*math.tan(math.radians(20))))
layer=dict(uuid=uuid,instance_id='optics-actor',label=actor,sx=scale,sy=scale,visible=True,enabled=True,receive_light=False,receive_shadow=False)
base=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,fx_enabled=False,light_enabled=False,timeline_preview=False,playing=False,layers=[layer])
o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=base,sceneItemEnabled=True));o.call('SetCurrentProgramScene',dict(sceneName=scene));time.sleep(.8);n=UI(bridge,stage)
def fx(identifier,p=None,color=None,color2=None):
 f=dict(id=identifier,enabled=True,strength=1,optics_revision=1,parameters=p or {})
 if color is not None:f['color']=dict(zip('rgb',color))
 if color2 is not None:f['color2']=dict(zip('rgb',color2))
 return f
def put(effects=None,**patch):
 m=copy.deepcopy(base);m.update(patch);groups=[dict(enabled=False) for _ in range(13)]
 for f in effects or []:groups[int(f['id'][2:4])-1]['enabled']=True
 m['layers'][0]['fx_stack']=dict(attached=True,group_states=groups,effects=effects or [])
 n.request(m);time.sleep(.2)
def shot(name):
 data=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=1920,imageHeight=1080));raw=base64.b64decode(data['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(raw);return Image.open(io.BytesIO(raw)).convert('RGBA')
try:
 put();plain=shot('original')
 for mode in [0,1]:
  p=dict(mode=mode,radius=100,threshold=.2,gain=4)
  put([fx('FX04.0',p,color=(0,1,0))]);green=shot('glow-'+str(mode)+'-green')
  alpha=max(green.getpixel((x,530))[3] for x in range(725,759))
  check(alpha>2,'glow spills beyond source rectangle mode '+str(mode),alpha)
  put([fx('FX04.0',p,color=(1,0,0))]);red=shot('glow-'+str(mode)+'-red')
  check(delta(green,red)>.1,'glow tint is independently visible mode '+str(mode),delta(green,red))
 # Shadows must honor their own angle; the angle is independent of stage rig.
 p=dict(mode=0,distance=80,radius=20,opacity=1,angle=0,spread=8,auxiliary=0)
 put([fx('FX05.5',p,color2=(0,0,0))]);right=shot('shadow-angle0')
 put([fx('FX05.5',dict(p,angle=180),color2=(0,0,0))]);left=shot('shadow-angle180')
 check(delta(right,left)>.1,'local shadow angle changes silhouette direction',delta(right,left))
 for rays in range(4):
  p=dict(length=140,threshold=.05,rays=rays,angle=0,gain=4,tolerance=.12)
  put([fx('FX04.1',p,color=(0,1,0),color2=(1,35/255,35/255))]);red=shot('star-red-target-'+str(rays))
  put([fx('FX04.1',p,color=(0,1,0),color2=(35/255,35/255,1))]);blue=shot('star-blue-target-'+str(rays))
  check(delta(red,blue)>.1,'star color selection changes emitting regions rays index '+str(rays),delta(red,blue))
  # Outside bottom of the chosen emitter, near the ray at zero degrees.
  signal=max(red.getpixel((x,530))[1] for x in range(1010,1050))
  check(signal>10,'star rays visible outside selected red emitter '+str(rays),signal)
 write('scene.json',dict(scene=scene,stage=stage,base=base))
 check(n.get()['_metrics']['failures']==0,'no GPU shader failures',n.get()['_metrics'])
 if '--full' in sys.argv:
  # Exact neutral endpoints must retain source layout and alpha.
  for mode in [0,1]:
   for key,value in [('gain',0),('threshold',1)]:
    put([fx('FX04.0',dict({'mode':mode,'radius':160,'threshold':.2,'gain':3},**{key:value}))])
    a=shot('neutral-glow-'+str(mode)+'-'+key)
    check(delta(plain,a)<.08,'neutral glow preserves source RGBA '+str(mode)+' '+key,delta(plain,a))
  # Every local-shadow control has a visible role in both modes.
  for mode in [0,1]:
   p=dict(mode=mode,distance=45,radius=35,opacity=.8,angle=35,spread=4,auxiliary=0)
   for key,lo,hi in [('distance',0,100),('radius',0,100),('angle',-90,90),('spread',0,60),('opacity',0,1)]:
    put([fx('FX05.5',dict(p,**{key:lo}),color=(1,1,0),color2=(0,0,0))]);a=shot('shadow-'+str(mode)+'-'+key+'-low')
    put([fx('FX05.5',dict(p,**{key:hi}),color=(1,1,0),color2=(0,0,0))]);b=shot('shadow-'+str(mode)+'-'+key+'-high')
    check(delta(a,b)>.01,'shadow control responds '+str(mode)+' '+key,delta(a,b))
   effect=fx('FX05.5',p,color2=(0,0,0));put([effect]);before=shot('independent-shadow-'+str(mode))
   put([effect],fx_enabled=True,light_enabled=True,light_x=-1600,light_y=800,light_z=-500,light_intensity=4)
   after=shot('stage-light-toggle-'+str(mode));check(delta(before,after)<.05,'stage lamp cannot steer local shadow '+str(mode),delta(before,after))
   effect['strength']=0;put([effect]);check(delta(plain,shot('shadow-blend-zero-'+str(mode)))<.05,'zero blend disables shadow '+str(mode))
  # Broad blurred edge must be continuous, not several sharp copied masks.
  put([fx('FX05.5',dict(mode=0,distance=140,radius=100,opacity=1,angle=90,spread=0),color2=(0,0,0))]);a=shot('shadow-soft-profile')
  profile=[a.getpixel((805,y))[3] for y in range(635,900)]
  check(max(profile)>20 and max(abs(x-y) for x,y in zip(profile,profile[1:]))<12,'outer shadow has one continuous soft edge',dict(peak=max(profile),maxStep=max(abs(x-y) for x,y in zip(profile,profile[1:]))))
  write('shadow-profile.json',profile)
  # Tiny round emitter exposes the actual ray count; a large rectangle naturally
  # produces a broad streak and cannot measure lens-star topology.
  point=out/'point.png';pi=Image.new('RGBA',(400,300));ImageDraw.Draw(pi).ellipse((197,147,203,153),fill='white');pi.save(point)
  o.call('SetInputSettings',dict(inputName=actor,inputSettings={'file':str(point)}));time.sleep(.2)
  for index,count in enumerate([2,4,5,6]):
   put([fx('FX04.1',dict(length=120,threshold=.1,rays=index,angle=0,gain=2,tolerance=.05),color=(0,1,0),color2=(1,1,1))]);a=shot('point-star-'+str(count))
   def energy(x,y):
    r,g,b,alpha=a.getpixel((round(x),round(y)));return g*alpha/255
   peaks=[];valleys=[]
   for ray in range(count):
    theta=ray*2*math.pi/count;mid=theta+math.pi/count
    peaks.append(energy(960+math.cos(theta)*55,540+math.sin(theta)*55))
    valleys.append(energy(960+math.cos(mid)*55,540+math.sin(mid)*55))
   check(min(peaks)>2 and min(peaks)>max(valleys)+2,'actual point star has exactly '+str(count)+' visible directions',dict(peaks=peaks,valleys=valleys))
  p=dict(length=120,threshold=.1,rays=1,angle=0,gain=2,tolerance=.05)
  put([fx('FX04.1',p,color=(1,0,0),color2=(1,1,1))]);red=shot('star-red-light')
  put([fx('FX04.1',p,color=(0,0,1),color2=(1,1,1))]);blue=shot('star-blue-light')
  check(delta(red,blue)>.005,'star effect tint is independent from target selection',delta(red,blue))
  put();pointplain=shot('point-original')
  put([fx('FX04.1',p,color=(1,1,1),color2=(1,0,0))]);excluded=shot('star-nonmatching')
  check(delta(pointplain,excluded)<.05,'nonmatching source pixels never seed rays',delta(pointplain,excluded))
  put([fx('FX04.1',dict(p,tolerance=1),color=(1,1,1),color2=(1,0,0))]);allcolors=shot('star-tolerance-one')
  check(delta(pointplain,allcolors)>.005,'tolerance one accepts all highlight colors',delta(pointplain,allcolors))
  # Reuse original larger fixture for UI, coordinate and compositor checks.
  o.call('SetInputSettings',dict(inputName=actor,inputSettings={'file':str(path)}));time.sleep(.2)
  effects=[fx('FX04.0',dict(radius=80,threshold=.3,gain=1.2),color=(.4,.8,1)),fx('FX04.1',dict(length=90,rays=2,tolerance=.2),color=(1,.6,.2),color2=(1,1,1)),fx('FX05.5',dict(mode=0,angle=115,radius=40,spread=12,auxiliary=.2),color2=(.2,.05,.1))]
  put(effects);n.command('ui-refresh');n.command('ui-tab',value=2);controls=n.inspect('fx-panel-all.png')
  prefix='spatial.fx34.optics-actor.'
  check(all(prefix+key in controls for key in ['FX05.5.angle','FX05.5.spread','FX04.0.color','FX04.1.color','FX04.1.color2','FX04.1.tolerance']),'all new controls present in real Qt Dock')
  raycontrol=controls.get(prefix+'FX04.1.rays',{})
  check([x['text'] for x in raycontrol.get('items',[])]==['二射线','四射线','五射线','六射线'],'real Qt ray selector has requested four choices',raycontrol.get('items'))
  for key in ['FX04.0.color','FX04.1.color','FX04.1.color2','FX05.5.color2']:
   before=shot('ui-'+key+'-before');n.command('fx-color-open',widget=prefix+key);n.command('fx-color-set',color='#cc15ef');time.sleep(.15)
   preview=shot('ui-'+key+'-live');check(delta(before,preview)>.005,'live stage color preview '+key,delta(before,preview))
   n.command('fx-color-cancel');restored=shot('ui-'+key+'-cancel');check(delta(before,restored)<.05,'color cancel restores original '+key,delta(before,restored))
  n.command('fx-color-open',widget=prefix+'FX04.0.color');n.command('fx-color-set',color='#32cc80');accepted=shot('ui-color-before-accept');n.command('fx-color-accept')
  check(delta(accepted,shot('ui-color-accepted'))<.05,'accepted color persists after dialog close')
  for label,patch in [('rotated',dict(rx=18,ry=25,rz=23)),('mirrored',dict(sx=-scale)),('cropped',dict(crop_left=30,crop_right=20,crop_top=10,crop_bottom=15))]:
   saved=copy.deepcopy(base);base['layers'][0].update(patch)
   put();reference=shot(label+'-original')
   put([fx('FX04.0',dict(radius=120,threshold=1,gain=2))]);neutral=shot(label+'-padded-neutral')
   check(delta(reference,neutral)<.2,'padding preserves geometry '+label,delta(reference,neutral))
   put(effects);a=shot(label+'-optics');check(delta(reference,a)>.1,'optics remain visible '+label,delta(reference,a));base=saved
  # Mask coordinates operate on the same world surface even with output padding.
  mask=dict(instance_id='optical-mask',shape=0,mode=1,width=200*scale,height=300*scale,local_mask=True,receivers=[{'instance_id':'optics-actor'}],softness=1,enabled=True,visible=True)
  put(effects,presets_geometries=[mask]);a=shot('optics-with-mask')
  check(a.getchannel('A').getextrema()[1]>0 and max(a.getpixel((x,540))[3] for x in range(700,850))==0,'local geometric mask clips extended optical surface in correct coordinates')
  # Changed common shader/blur paths: one real representative per direct caller.
  for identifier,p in [('FX01.0',dict(radius=15)),('FX03.0',dict(exposure=.5)),('FX05.1',dict(radius=50)),('FX08.4',dict(mode=1))]:
   put([fx(identifier,p)]);a=shot('related-'+identifier)
   check(delta(plain,a)>.001,'related surface path remains effective '+identifier,delta(plain,a))
  # Completely opaque rectangular inputs still need feathered inner edges and
  # unclipped outer halos. Transparent screenshots preserve all added coverage.
  opaque=out/'opaque.png';Image.new('RGBA',(400,300),(220,220,220,255)).save(opaque)
  o.call('SetInputSettings',dict(inputName=actor,inputSettings={'file':str(opaque)}));time.sleep(.2)
  put([fx('FX04.0',dict(radius=100,threshold=.2,gain=2),color=(0,1,0))]);a=shot('opaque-glow')
  alphas=[a.getpixel((x,540))[3] for x in range(660,760)]
  check(alphas[-1]>50 and alphas[0]==0 and max(abs(x-y) for x,y in zip(alphas,alphas[1:]))<12,'opaque source edge glows with continuous external falloff',dict(first=alphas[0],last=alphas[-1],maxStep=max(abs(x-y) for x,y in zip(alphas,alphas[1:]))))
  put([fx('FX05.5',dict(mode=1,angle=0,distance=25,radius=35,opacity=1),color2=(0,0,0))]);a=shot('opaque-inner-shadow')
  check(a.getpixel((762,540))[0]<a.getpixel((950,540))[0]-25,'opaque rectangle has visible independent inner shadow')
  o.call('SetInputSettings',dict(inputName=actor,inputSettings={'file':str(path)}));time.sleep(.2)
  # Save the new values before fresh process restart.
  put(effects);n.command('ui-refresh');n.command('ui-tab',value=2);n.command('ui-save')
  saved=n.get();write('restart-expected.json',saved);write('restart-scene.json',dict(scene=scene,stage=stage));shot('restart-expected')
  write('runtime-info.json',dict(version=o.call('GetVersion'),video=o.call('GetVideoSettings')))
  # Largest source and effects exercise real 1080p60 resource usage.
  large=out/'large.png';im.resize((1920,1080)).save(large);o.call('SetInputSettings',dict(inputName=actor,inputSettings={'file':str(large)}));time.sleep(.2)
  workloads=[]
  for label,selected in [('baseline',[]),('combined',[fx('FX04.0',dict(radius=160,threshold=.4,gain=1.5)),fx('FX04.1',dict(length=300,rays=3,tolerance=1,gain=1)),fx('FX05.5',dict(mode=0,radius=100,spread=60,distance=200))])]:
   put(selected);time.sleep(2);warm=n.get()['_metrics'];before=o.call('GetStats');samples=[];start=time.monotonic()
   for _ in range(30):time.sleep(1);samples.append(o.call('GetStats'))
   after=n.get()['_metrics'];elapsed=time.monotonic()-start
   result=dict(label=label,elapsedSeconds=elapsed,warm=warm,after=after,before=before,samples=samples)
   workloads.append(result);write('performance.json',workloads)
   check(warm['allocations']==after['allocations'] and after['failures']==0,'stable cached GPU allocations '+label,dict(warm=warm,after=after))
   check(min(s['activeFps'] for s in samples)>59 and samples[-1]['renderSkippedFrames']==before['renderSkippedFrames'],'1080p60 with zero added render skips '+label,dict(minFps=min(s['activeFps'] for s in samples),maxRenderMs=max(s['averageFrameRenderTime'] for s in samples)))
  o.call('SetInputSettings',dict(inputName=actor,inputSettings={'file':str(path)}));n.request(saved);n.command('ui-save')
except Exception:check(False,'probe exception',traceback.format_exc())
finally:write('optics-tests.json',dict(checks=checks,failed=sum(not c['pass'] for c in checks)));o.close()
raise SystemExit(any(not c['pass'] for c in checks))
