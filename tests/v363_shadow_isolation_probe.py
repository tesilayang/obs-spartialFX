"""Actual OBS output: stage rims must follow the surface, never local FX shadows."""
import base64, copy, io, json, math, sys, time, traceback
from pathlib import Path
from PIL import Image, ImageDraw, ImageChops, ImageStat
from obs_client import OBS
from v361_native import UI

out, bridge = [Path(p).resolve() for p in sys.argv[1:3]]
out.mkdir(parents=True, exist_ok=True)
o = OBS(); checks = []; stamp = str(time.time_ns())
scene = 'Shadow isolation ' + stamp; stage = 'Shadow isolation stage ' + stamp

def check(ok, name, value=None):
    checks.append(dict(name=name, **{'pass': bool(ok)}, value=value))
    print(('PASS ' if ok else 'FAIL ') + name + ' ' + str(value or ''), flush=True)

def delta(a, b):
    return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean)

o.call('CreateScene', dict(sceneName=scene))
path = out/'fixture.png'
im = Image.new('RGBA', (400, 300))
ImageDraw.Draw(im).rounded_rectangle((120, 65, 230, 235), 24, fill=(70, 85, 100, 255))
im.save(path)
actor = 'Shadow fixture ' + stamp
o.call('CreateInput', dict(sceneName=scene, inputName=actor, inputKind='image_source', inputSettings={'file':str(path)}, sceneItemEnabled=False))
uuid = next(s['inputUuid'] for s in o.call('GetInputList')['inputs'] if s['inputName']==actor)
scale = 1500/(1080/(2*math.tan(math.radians(20))))
layer = dict(uuid=uuid, instance_id='shadow-actor', label=actor, sx=scale, sy=scale,
             visible=True, enabled=True, receive_light=True, receive_shadow=False,
             cast_shadow=False, rim_light=True)
base = dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,
            fx_enabled=True,light_enabled=True,light_x=1100,light_y=-300,light_z=-600,
            light_intensity=1,light_range=4000,light_falloff=1,light_ambient=.8,
            light_rim=6,light_rim_width=14,light_rim_softness=.65,shadows_enabled=False,
            timeline_preview=False,playing=False,layers=[layer])
o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=base,sceneItemEnabled=True))
o.call('SetCurrentProgramScene',dict(sceneName=scene));time.sleep(.6)
n = UI(bridge,stage)

def fx(mode, **parameters):
    return dict(id='FX05.5',enabled=True,strength=1,optics_revision=1,
                color=dict(r=1,g=1,b=0), color2=dict(r=.12,g=.18,b=.35),
                parameters=dict(mode=mode,distance=130,radius=18,angle=0,spread=6,opacity=.85,**parameters))

def shot(name,effects=(),rim=6,**patch):
    m=copy.deepcopy(base);m.update(patch);m['light_rim']=rim
    groups=[dict(enabled=False) for _ in range(13)]
    for f in effects:groups[int(f['id'][2:4])-1]['enabled']=True
    m['layers'][0]['fx_stack']=dict(attached=True,group_states=groups,effects=list(effects))
    n.request(m);time.sleep(.23)
    data=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=1920,imageHeight=1080))
    raw=base64.b64decode(data['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(raw)
    return Image.open(io.BytesIO(raw)).convert('RGBA')

try:
    off=shot('surface-no-rim',rim=0); on=shot('surface-rim')
    check(delta(on,off)>.02,'original surface still receives visible stage rim',delta(on,off))
    source_mask=off.getchannel('A').point(lambda a:255 if a>0 else 0)
    outside=source_mask.point(lambda a:255-a)
    for mode in [0,1]:
        f=fx(mode,auxiliary=0)
        a=shot(f'shadow-{mode}-rim-off',[f],rim=0)
        b=shot(f'shadow-{mode}-rim-on',[f])
        change=ImageChops.difference(a,b)
        rgb_peak=max(max(channel.getextrema()) for channel in change.convert('RGB').split())
        outside_peak=max(max(channel.getextrema()) for channel in ImageChops.multiply(change.convert('RGB'),Image.merge('RGB',[outside]*3)).split())
        check(outside_peak<=2,'stage rim never illuminates FX shadow exterior mode '+str(mode),outside_peak)
        check(rgb_peak>10,'stage rim remains enabled on original contour mode '+str(mode),rgb_peak)
        # Old saved auxiliary data must have no visual effect after removal.
        c=shot(f'shadow-{mode}-legacy-auxiliary',[fx(mode,auxiliary=1)])
        check(delta(b,c)<.001,'legacy auxiliary intensity and color are inert mode '+str(mode),delta(b,c))
    n.command('ui-refresh');n.command('ui-tab',value=2)
    controls=n.inspect('shadow-panel.png')
    keys=list(controls)
    check(not any('FX05.5.auxiliary' in k for k in keys),'auxiliary strength control removed')
    check(not any('FX05.5.color' in k and 'FX05.5.color2' not in k for k in keys),'auxiliary color control removed')
    check(any('FX05.5.color2' in k for k in keys),'shadow color control retained')
    state=n.get(); saved=state['layers'][0]['fx_stack']['effects'][0]
    check('auxiliary' not in saved['parameters'],'saved shadow drops obsolete auxiliary parameter')
    check('color' not in saved,'saved shadow drops obsolete auxiliary color')
    if '--extended' in sys.argv:
        for label, patch in [('cropped',dict(crop_left=45,crop_top=20,crop_right=30,crop_bottom=15)),
                             ('crop-through-surface',dict(crop_left=180)),
                             ('rotated',dict(rx=22,ry=-28,rz=31)),
                             ('mirrored',dict(sx=-scale)),
                             ('nonuniform',dict(sx=scale*1.4,sy=scale*.7))]:
            original=copy.deepcopy(base['layers'][0]);base['layers'][0].update(patch)
            selected=fx(0);light_x=base['light_x']
            if label=='crop-through-surface':
                selected['parameters'].update(angle=180,distance=80,radius=0,spread=0)
                base['light_x']=-1100
            a=shot(label+'-original',rim=0)
            a0=shot(label+'-shadow-no-rim',[selected],rim=0)
            a1=shot(label+'-shadow-rim',[selected])
            mask=a.getchannel('A').point(lambda a:0 if a else 255)
            change=ImageChops.difference(a0,a1).convert('RGB')
            exterior=ImageChops.multiply(change,Image.merge('RGB',[mask]*3))
            peak=max(c.getextrema()[1] for c in exterior.split())
            check(peak<=3,'rim excludes shadow with '+label,peak)
            check(delta(a0,a1)>.01,'original rim still visible with '+label,delta(a0,a1))
            base['layers'][0]=original;base['light_x']=light_x
        mask=dict(instance_id='rim-mask',shape=0,mode=1,width=170*scale,height=300*scale,
                  x=-35*scale,local_mask=True,receivers=[{'instance_id':'shadow-actor'}],
                  softness=1,enabled=True,visible=True)
        a=shot('masked-surface',rim=0,presets_geometries=[mask])
        b=shot('masked-shadow-no-rim',[fx(0)],rim=0,presets_geometries=[mask])
        c=shot('masked-shadow-rim',[fx(0)],presets_geometries=[mask])
        check(a.getchannel('A').getextrema()[1]>0 and delta(b,c)>.01,'masked original surface and its rim remain visible',delta(b,c))
        outside=a.getchannel('A').point(lambda a:0 if a else 255)
        peak=max(ch.getextrema()[1] for ch in ImageChops.multiply(ImageChops.difference(b,c).convert('RGB'),Image.merge('RGB',[outside]*3)).split())
        check(peak<=3,'geometry mask preserved in independent rim surface',peak)
        # Exercise real shadow color picker, including cancellation rollback.
        shot('color-original',[fx(0)]);n.command('ui-refresh');n.command('ui-tab',value=2)
        prefix='spatial.fx34.shadow-actor.FX05.5.color2'
        def live(name):
            d=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=1920,imageHeight=1080))
            raw=base64.b64decode(d['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(raw)
            return Image.open(io.BytesIO(raw)).convert('RGBA')
        original=live('color-before')
        n.command('fx-color-open',widget=prefix);n.command('fx-color-set',color='#cf1eab');time.sleep(.2)
        preview=live('color-live');check(delta(original,preview)>.01,'retained shadow color previews on real stage',delta(original,preview))
        n.command('fx-color-cancel');restored=live('color-cancel')
        check(delta(original,restored)<.01,'shadow color cancellation restores output',delta(original,restored))
        # Translucent source must not create a rim on the added shadow either.
        semi=out/'fixture-semitransparent.png';a=im.copy();a.putalpha(a.getchannel('A').point(lambda v:v//2));a.save(semi)
        o.call('SetInputSettings',dict(inputName=actor,inputSettings={'file':str(semi)}));time.sleep(.2)
        surface=shot('semi-surface',rim=0)
        a=shot('semi-no-rim',[fx(0)],rim=0);b=shot('semi-rim',[fx(0)])
        mask=surface.getchannel('A').point(lambda a:0 if a else 255)
        peak=max(ch.getextrema()[1] for ch in ImageChops.multiply(ImageChops.difference(a,b).convert('RGB'),Image.merge('RGB',[mask]*3)).split())
        check(peak<=3 and delta(a,b)>.01,'translucent source retains rim only on original contour',peak)
        base['layers'][0]['opacity']=.5
        half=shot('semi-half-opacity',[fx(0)])
        ratio=sum(ImageStat.Stat(half.getchannel('A')).sum)/sum(ImageStat.Stat(b.getchannel('A')).sum)
        check(abs(ratio-.5)<.02,'layer opacity scales coverage of rim and shadow together',ratio)
        base['layers'][0].pop('opacity')
        o.call('SetInputSettings',dict(inputName=actor,inputSettings={'file':str(path)}));time.sleep(.2)
        # Old data is removed on save, and must remain removed after a process restart.
        shot('restart-expected',[fx(0,auxiliary=1)])
        n.command('ui-refresh');n.command('ui-save')
        (out/'restart-expected.json').write_text(json.dumps(n.get(),ensure_ascii=False,indent=2),encoding='utf-8')
        (out/'restart-scene.json').write_text(json.dumps(dict(scene=scene,stage=stage)),encoding='utf-8')
    if '--performance' in sys.argv:
        big=out/'fixture-1080.png';im.resize((1920,1080)).save(big)
        o.call('SetInputSettings',dict(inputName=actor,inputSettings={'file':str(big)}));time.sleep(.3)
        workloads=[]
        for label,effects in [('stage-rim',[]),('stage-rim-and-outer-shadow',[fx(0)])]:
            shot('perf-'+label,effects);time.sleep(2)
            warm=n.get()['_metrics'];before=o.call('GetStats');samples=[];start=time.monotonic()
            for _ in range(30):time.sleep(1);samples.append(o.call('GetStats'))
            after=n.get()['_metrics']
            workloads.append(dict(label=label,elapsedSeconds=time.monotonic()-start,warm=warm,after=after,before=before,samples=samples))
            (out/'performance.json').write_text(json.dumps(workloads,indent=2),encoding='utf-8')
            check(warm['allocations']==after['allocations'] and after['failures']==0,'stable GPU resources '+label,dict(warm=warm,after=after))
            check(min(s['activeFps'] for s in samples)>59 and samples[-1]['renderSkippedFrames']==before['renderSkippedFrames'],'1080p60 without added render skips '+label,dict(minFps=min(s['activeFps'] for s in samples),maxRenderMs=max(s['averageFrameRenderTime'] for s in samples)))
        o.call('SetInputSettings',dict(inputName=actor,inputSettings={'file':str(path)}))
        if '--extended' in sys.argv:
            n.request(json.loads((out/'restart-expected.json').read_text(encoding='utf-8')));n.command('ui-save')
    check(state['_metrics']['failures']==0,'no GPU render failures',state['_metrics'])
    (out/'runtime-info.json').write_text(json.dumps(dict(version=o.call('GetVersion'),video=o.call('GetVideoSettings')),indent=2),encoding='utf-8')
    (out/'scene.json').write_text(json.dumps(dict(stage=stage,scene=scene,base=base),ensure_ascii=False,indent=2),encoding='utf-8')
except Exception:
    check(False,'shadow isolation probe exception',traceback.format_exc())
finally:
    (out/'isolation-tests.json').write_text(json.dumps(dict(checks=checks,failed=sum(not c['pass'] for c in checks)),ensure_ascii=False,indent=2),encoding='utf-8')
    o.close()
raise SystemExit(any(not c['pass'] for c in checks))
