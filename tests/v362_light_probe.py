"""Actual installed OBS lighting regressions; synthetic assets, never user scenes."""
import base64,copy,io,json,math,statistics,sys,time,traceback
from pathlib import Path
from PIL import Image,ImageDraw,ImageChops,ImageStat
from obs_client import OBS
from v361_native import UI

out,bridge=map(lambda v:Path(v).resolve(),sys.argv[1:3]);out.mkdir(parents=True,exist_ok=True)
checks=[];workloads=[]
def check(ok,name,value=None):
    checks.append(dict(name=name,**{'pass':bool(ok)},value=value));print(('PASS ' if ok else 'FAIL ')+name+' '+str(value or ''),flush=True)
def diff(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
def write(name,data):(out/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
o=OBS();stamp=str(time.time_ns());scene='V362 light QA '+stamp;stage='V362 light stage '+stamp
o.call('CreateScene',dict(sceneName=scene));ids=[]
for label,size in [('wall',(1920,1080)),('actor',(320,420))]:
    p=out/(label+'.png');im=Image.new('RGBA',size,(175,185,195,255) if label=='wall' else (0,0,0,0));d=ImageDraw.Draw(im)
    if label=='actor':d.rounded_rectangle((35,30,285,390),radius=75,fill=(95,135,190,255));d.ellipse((110,140,210,240),fill=(0,0,0,0))
    im.save(p);name='V362 '+label+' '+stamp
    o.call('CreateInput',dict(sceneName=scene,inputName=name,inputKind='image_source',inputSettings={'file':str(p)},sceneItemEnabled=False))
    ids.append(next(s['inputUuid'] for s in o.call('GetInputList')['inputs'] if s['inputName']==name))
layers=[dict(uuid=ids[0],instance_id='wall',label='wall',z=500,sx=1.35,sy=1.35,cast_shadow=False,rim_light=False),dict(uuid=ids[1],instance_id='actor',label='actor',x=-120,y=-40,z=0,cast_shadow=True,rim_light=False)]
base=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,fx_enabled=True,playing=False,timeline_preview=True,layers=layers,light_enabled=True,light_visible=False,light_x=-600,light_y=-350,light_z=-650,light_intensity=1.5,light_range=4000,light_ambient=.25,light_red=1,light_green=1,light_blue=1,light_size=60,light_rim=0,shadows_enabled=True,shadow_opacity=.85,light_softness=15,receiver_instance='wall',receiver_uuid=ids[0])
o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=base,sceneItemEnabled=True));o.call('SetCurrentProgramScene',dict(sceneName=scene));time.sleep(.8)
n=UI(bridge,stage)
def put(**patch):
    m=copy.deepcopy(base);m.update(patch);n.request(m);time.sleep(.22);return n.get()
def shot(name):
    data=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=960,imageHeight=540));raw=base64.b64decode(data['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(raw);return Image.open(io.BytesIO(raw)).convert('RGBA')
try:
    put(shadow_opacity=0);unshadowed=shot('shadow-off')
    put();shadowed=shot('shadow-on')
    # 180-degree area hidden directly behind the actor is excluded from this ROI.
    roi=(500,270,700,450);change=diff(unshadowed.crop(roi),shadowed.crop(roi))
    check(change>2,'foreground alpha casts visible shadow on actual receiver',change)
    put(light_softness=0,light_size=0);hard=shot('hard-shadow')
    put(light_softness=100,light_size=200);soft=shot('soft-shadow')
    check(diff(hard.crop(roi),soft.crop(roi))>.3,'soft radius changes actual shadow edge',diff(hard.crop(roi),soft.crop(roi)))
    put(light_intensity=.12,light_ambient=.83,shadow_opacity=0);bright_off=shot('bright-ambient-shadow-off')
    put(light_intensity=.12,light_ambient=.83,shadow_opacity=1);bright_on=shot('bright-ambient-shadow-on')
    change=diff(bright_off.crop(roi),bright_on.crop(roi))
    check(change>20,'shadow attenuates owning lamp ambient fill instead of disappearing',change)
    baseline_path=Path(__file__).resolve().parents[1]/'work/v3.6.2/red-repro/shadow-off.png'
    if baseline_path.exists():check(diff(unshadowed,Image.open(baseline_path).convert('RGBA'))<3,'unshadowed composition retains baseline output',diff(unshadowed,Image.open(baseline_path).convert('RGBA')))
    for name,patch in [('shadow toggle',{'shadows_enabled':False}),('caster toggle',{'layers':[layers[0],dict(layers[1],cast_shadow=False)]}),('receiver shadow toggle',{'layers':[dict(layers[0],receive_shadow=False),layers[1]]}),('receiver identity',{'receiver_instance':'actor','receiver_uuid':ids[1]})]:
        put(**patch);im=shot(name.replace(' ','-'));check(diff(im.crop(roi),unshadowed.crop(roi))<2,name+' disables actual receiver shadow',diff(im.crop(roi),unshadowed.crop(roi)))
    put(layers=[dict(layers[0],receive_light=False),layers[1]],shadow_opacity=0);no_receive_off=shot('receive-light-off-unshadowed')
    put(layers=[dict(layers[0],receive_light=False),layers[1]]);no_receive_on=shot('receive-light-off-shadowed')
    check(diff(no_receive_off.crop(roi),no_receive_on.crop(roi))<1,'receiveLight disabled bypasses both direct and ambient shadow')
    behind=[layers[0],dict(layers[1],z=800)]
    put(layers=behind,shadow_opacity=0);behind_off=shot('caster-behind-off')
    put(layers=behind);behind_on=shot('caster-behind-on')
    check(diff(behind_off,behind_on)<1,'caster behind receiver cannot project backwards',diff(behind_off,behind_on))
    second=dict(light_enabled=True,light_intensity=0,light_ambient=.3,light_x=-600,light_y=-350,light_z=-650,shadows_enabled=False)
    put(light_intensity=0,light_ambient=.6,extra_lights=[second],shadow_opacity=0);multi_off=shot('multi-off')
    put(light_intensity=0,light_ambient=.6,extra_lights=[second],shadow_opacity=1);multi_on=shot('multi-on')
    check(diff(multi_off.crop(roi),multi_on.crop(roi))>20,'owning lamp ambient is occluded with multiple lamps')
    put(light_intensity=0,light_ambient=.6,extra_lights=[],shadow_opacity=1);single_on=shot('single-ambient-on')
    check(sum(ImageStat.Stat(multi_on.crop(roi)).mean[:3])>sum(ImageStat.Stat(single_on.crop(roi)).mean[:3])+80,'other lamp unshadowed fill remains independent')
    put(light_intensity=0,light_ambient=2,extra_lights=[dict(second,light_ambient=2)],shadow_opacity=0);cap_multi=shot('ambient-cap-multi')
    put(light_intensity=0,light_ambient=2,extra_lights=[],shadow_opacity=0);cap_single=shot('ambient-cap-single')
    check(diff(cap_multi,cap_single)<2,'ambient sum above two retains previous total ceiling',diff(cap_multi,cap_single))
    put(shadow_red=1,shadow_green=0,shadow_blue=0,shadow_opacity=1);red_shadow=shot('colored-shadow')
    means=ImageStat.Stat(red_shadow.crop(roi)).mean;check(means[0]>means[1]+15,'shadow color affects actual occlusion tint',means)
    put(light_type=1,light_size=250,light_softness=80);area_shadow=shot('area-shadow');check(diff(area_shadow.crop(roi),unshadowed.crop(roi))>10,'area light projects actual soft shadow')
    put(light_x=600);moved_shadow=shot('moved-light-shadow');check(diff(moved_shadow,shadowed)>5,'light movement updates projected shadow')
    put();n.command('ui-refresh');n.command('ui-tab',value=1);controls=n.inspect('light-before.png');write('controls-before.json',controls)
    n.click('spatial.add-point-light');time.sleep(.2);state=n.get();light=state['extra_lights'][-1];write('added-light.json',light)
    check(abs(light['light_x'])<1 and abs(light['light_y'])<1,'new light is at imported composition center',light)
    check(-1500<light['light_z']<0,'new light starts in front of imported foreground and camera',light['light_z'])
    controls=n.inspect('light-after.png');write('controls-after.json',controls)
    pitch=controls['spatial.light-y']['y']-controls['spatial.light-x']['y']
    check(27<=pitch<=31,'light numeric row pitch approximately30percent below baseline42px',pitch)
    check(all(controls[k]['step']==.01 for k in ['spatial.light-x','spatial.light-y','spatial.light-z','spatial.light-intensity','spatial.light-softness']),'compact controls retain .01 value step')
    check('3.6.2' in controls['spatial.version']['text'],'actual installed Dock displays3.6.2')
    n.number('spatial.light-x',123.45);check(abs(n.get()['extra_lights'][-1]['light_x']-123.45)<.001,'compact light field writes exact value')
    # New area lights follow a rotated camera rather than fixed world XY.
    rotated=[dict(layers[0],x=500,y=0,z=-633.9745962155614,ry=30,sx=.2,sy=.2)]
    put(layers=rotated,camera_ry=30);n.command('ui-refresh');n.click('spatial.add-area-light');state=n.get();lamp=state['extra_lights'][-1]
    localx=math.cos(math.pi/6)*lamp['light_x']-math.sin(math.pi/6)*(lamp['light_z']+1500)
    localz=math.sin(math.pi/6)*lamp['light_x']+math.cos(math.pi/6)*(lamp['light_z']+1500)
    check(lamp['light_type']==1 and abs(localx)<1 and abs(lamp['light_y'])<1 and 10<localz<1000,'area light centers ahead of composition in rotated camera',lamp)
    # Save a reproducible light/shadow document for a fresh-process restart check.
    put(extra_lights=[dict(second,light_label='Persisted QA lamp',light_x=123.45,light_z=-800)],light_softness=73.21,shadow_red=.2)
    n.command('ui-save');write('restart-expected.json',n.get());write('restart-scene.json',dict(scene=scene,stage=stage))
    video=o.call('GetVideoSettings');write('runtime-info.json',dict(version=o.call('GetVersion'),video=video))
    check(video['baseWidth']==1920 and video['baseHeight']==1080 and video['fpsNumerator']/video['fpsDenominator']==60,'actual light benchmark1080p60')
    for label,patch in [('disabled',dict(fx_enabled=False)),('one-soft-light',{}),('four-soft-lights',dict(extra_lights=[dict(second,light_intensity=.35,light_ambient=.1,shadows_enabled=True,light_x=x,light_softness=100,shadow_opacity=.8) for x in [-600,0,600]]))]:
        put(**patch);time.sleep(1);warm=n.get()['_metrics'];before=o.call('GetStats');started=time.monotonic();samples=[]
        for _ in range(30):time.sleep(1);samples.append(o.call('GetStats'))
        after=n.get()['_metrics'];row=dict(name=label,elapsedSeconds=time.monotonic()-started,meanCPU=statistics.mean(s['cpuUsage'] for s in samples),meanFPS=statistics.mean(s['activeFps'] for s in samples),meanRenderMs=statistics.mean(s['averageFrameRenderTime'] for s in samples),addedSkippedFrames=samples[-1]['renderSkippedFrames']-before['renderSkippedFrames'],memoryDeltaMiB=samples[-1]['memoryUsage']-before['memoryUsage'],warm=warm,after=after,samples=samples)
        workloads.append(row);write('light-workloads.json',workloads)
        check(row['meanFPS']>59 and row['addedSkippedFrames']==0 and row['elapsedSeconds']>=30,label+' real30second performance',dict(fps=row['meanFPS'],cpu=row['meanCPU'],renderMs=row['meanRenderMs']))
        check(after['renders']-warm['renders']>=1500 and after['allocations']==warm['allocations'] and after['failures']==warm['failures']==0,label+' renders continuously with stable cached resources')
    for i in range(80):
        put(light_x=-800+i*20,light_y=math.sin(i)*400,light_softness=(i%30)*10)
    check(n.get()['_metrics']['failures']==0,'eighty light position and softness changes leave renderer healthy')
    n.request(json.loads((out/'restart-expected.json').read_text(encoding='utf-8')));time.sleep(.4);shot('shadow-persisted');n.command('ui-refresh');n.command('ui-save')
except Exception:check(False,'light probe exception',traceback.format_exc())
finally:
    write('light-tests.json',dict(checks=checks,failed=sum(not c['pass'] for c in checks),workloads=workloads,scene=scene,stage=stage));o.close()
raise SystemExit(any(not c['pass'] for c in checks))
