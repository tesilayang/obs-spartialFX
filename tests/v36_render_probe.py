"""Actual OBS/D3D11 coverage assertions for the native v3.6 geometry renderer.

Run only against the isolated OBS QA instance, passing evidence and bridge dirs.
All screenshots are synthetic test evidence, not generated user media.
"""
import base64, io, json, math, sys, time, traceback
from pathlib import Path
from PIL import Image, ImageChops, ImageStat
from obs_client import OBS
from native_control import Native

out = Path(sys.argv[1]).resolve(); out.mkdir(parents=True, exist_ok=True)
bridge = Path(sys.argv[2]).resolve() if len(sys.argv) > 2 else out
checks = []
def check(ok, name, value=None):
    checks.append(dict(status='PASS' if ok else 'FAIL', name=name, value=value))
    print(checks[-1]['status'], name, value or '', flush=True)
def delta(a,b): return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
def alpha(image,x,y): return image.getpixel((x,y))[3]
def geom(**kw):
    value=dict(instance_id='gpu-geometry',shape=0,mode=0,width=960,height=540,
               opacity=1,color_r=1,color_g=0,color_b=0,enabled=True,visible=True,
               thickness=35,softness=1,spacing=100,sides=6,shear=.35)
    value.update(kw);return value

o=OBS(); stamp=str(time.time_ns()); scene='V36 geometry QA '+stamp; stage='V36 geometry stage '+stamp
o.call('CreateScene', {'sceneName':scene})
Image.new('RGBA',(1920,1080),(0,255,0,255)).save(out/'green-fixture.png')
name='V36 green fixture '+stamp
o.call('CreateInput',dict(sceneName=scene,inputName=name,inputKind='image_source',inputSettings={'file':str(out/'green-fixture.png')},sceneItemEnabled=False))
uuid=next(i['inputUuid'] for i in o.call('GetInputList')['inputs'] if i['inputName']==name)
scale=1500/(1080/(2*math.tan(math.radians(20))))
layer=dict(uuid=uuid,instance_id='gpu-receiver',sx=scale,sy=scale,visible=True,enabled=True)
base=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,
          fx_enabled=False,playing=False,timeline_preview=False,layers=[],presets_geometries=[])
o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=base,sceneItemEnabled=True))
o.call('SetCurrentProgramScene',{'sceneName':scene});time.sleep(.6)
control=Native(bridge,stage)
def update(**kw): control.update(kw);time.sleep(.12)
def shot(label):
    data=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=640,imageHeight=360))
    raw=base64.b64decode(data['imageData'].split(',',1)[1]);(out/(label+'.png')).write_bytes(raw)
    return Image.open(io.BytesIO(raw)).convert('RGBA')
def metric(): return control.get().get('_metrics',{})
try:
    # RED baseline on v3.5: no red rectangle is drawn, so this assertion fails.
    update(layers=[],presets_geometries=[geom(screen_fixed=True)])
    rectangle=shot('rectangle');check(rectangle.getpixel((320,180))[0]>245 and alpha(rectangle,100,100)<5,'rectangle has opaque red center and transparent exterior')
    update(presets_geometries=[geom(shape=2,screen_fixed=True)])
    circle=shot('ellipse');check(alpha(circle,320,180)>245 and alpha(circle,165,95)<10,'ellipse clips corners without erasing center')
    update(presets_geometries=[geom(shape=3,screen_fixed=True)])
    ring=shot('ring');check(alpha(ring,320,180)<5 and max(alpha(ring,x,180) for x in range(160,180))>150,'ring has transparent hole and visible edge')
    for shape in (1,4,5,6,7):
        update(presets_geometries=[geom(shape=shape,screen_fixed=True)]);picture=shot('shape-'+str(shape))
        check(picture.getchannel('A').getextrema()[1]>150 and delta(rectangle,picture)>.01,'shape '+str(shape)+' renders its own coverage')
    update(presets_geometries=[geom(opacity=.5,screen_fixed=True)])
    half=shot('half-opacity');check(120<=alpha(half,320,180)<=135,'geometry opacity affects output alpha')
    # Native world geometry uses the same camera projection and depth order as source planes.
    update(layers=[layer],presets_geometries=[geom(width=1200,height=700,z=100)])
    back=shot('geometry-behind');update(presets_geometries=[geom(width=1200,height=700,z=-100)])
    front=shot('geometry-front');check(back.getpixel((320,180))[1]>240 and front.getpixel((320,180))[0]>240,'world geometry interleaves behind and in front of source')
    mask=geom(shape=2,mode=1,screen_fixed=True,receivers=[{'instance_id':'gpu-receiver'}])
    update(presets_geometries=[mask]);clipped=shot('receiver-ellipse')
    check(alpha(clipped,320,180)>240 and alpha(clipped,100,100)<5 and clipped.getpixel((320,180))[1]>240,'mask clips receiver RGBA and adds no colored overlay')
    update(presets_geometries=[dict(mask,receivers=[{'instance_id':'missing'}])]);untargeted=shot('unbound-mask')
    check(alpha(untargeted,100,100)>240,'missing receiver never clips another source')
    update(presets_geometries=[dict(mask,invert=True)]);inverse=shot('inverted-mask')
    check(alpha(inverse,320,180)<5 and alpha(inverse,100,100)>240,'inverse mask keeps exterior')
    update(presets_geometries=[dict(mask,opacity=.5)]);partial=shot('partial-mask')
    check(120<=alpha(partial,320,180)<=135 and alpha(partial,100,100)<5,'mask opacity multiplies coverage')
    update(presets_geometries=[dict(mask,opacity=0)]);zero=shot('zero-mask')
    check(zero.getchannel('A').getextrema()[1]<5,'active zero opacity mask fully clips receiver')
    update(presets_geometries=[dict(mask,sx=0)]);collapsed=shot('collapsed-mask')
    check(collapsed.getchannel('A').getextrema()[1]<5,'collapsed active mask fully clips receiver')
    update(presets_geometries=[dict(mask,enabled=False)]);disabled=shot('disabled-mask')
    check(alpha(disabled,100,100)>240,'disabled mask restores source')
    local=geom(shape=0,mode=1,local_mask=True,width=960,height=540,receivers=[{'instance_id':'gpu-receiver'}])
    update(presets_geometries=[local]);localpic=shot('local-mask')
    check(alpha(localpic,320,180)>240 and alpha(localpic,100,100)<5,'receiver local mask uses centered cropped content pixels')
    update(presets_geometries=[mask,dict(mask,instance_id='second-mask',x=300)])
    intersection=shot('mask-intersection');check(alpha(intersection,180,180)<5 and alpha(intersection,360,180)>240,'multiple receiver masks intersect their coverage')
    left=dict(layer,instance_id='gpu-left',x=-480*scale,sx=scale/2,sy=scale/2)
    right=dict(layer,instance_id='gpu-right',x=480*scale,sx=scale/2,sy=scale/2)
    twinmask=dict(mask,width=720,x=-480,receivers=[{'instance_id':'gpu-left'}])
    update(layers=[left,right],presets_geometries=[twinmask]);twins=shot('same-source-independent-mask')
    check(alpha(twins,10,180)<5 and alpha(twins,630,180)>240,'same-source receiver instances mask independently')
    update(layers=[layer])
    update(presets_geometries=[dict(mask,mode=2,opacity=.4)])
    both=shot('both-mode');check(delta(both,partial)>1,'combined mode draws decoration and clips receiver')
    responsive=geom(screen_fixed=True,x=-480,width=960,height=540,reference_width=1920,reference_height=1080,layout_pivot_x=0,layout_pivot_y=0)
    update(layers=[],presets_geometries=[responsive]);full=shot('responsive-full')
    update(width=960,height=540);small=shot('responsive-half')
    check(delta(full,small)<.1,'canvas resize preserves normalized geometry layout about its authoring pivot',delta(full,small))
    update(width=1920,height=1080,layers=[layer],presets_geometries=[dict(mask,mode=2,opacity=.4)])
    warm=metric();time.sleep(1);steady=metric()
    check(steady.get('allocations')==warm.get('allocations'),'steady geometry and mask reuse cached GPU resources',dict(warm=warm,steady=steady))
    update(presets_geometries=[]);before=metric();time.sleep(.3);after=metric()
    check(after.get('passes')==before.get('passes'),'removing geometry adds zero geometry/mask passes')
except Exception:
    check(False,'GPU probe exception',traceback.format_exc())
finally:
    try: control.update({'layers':[],'presets_geometries':[]})
    except Exception: pass
    o.close()
report=dict(checks=checks,failed=sum(c['status']=='FAIL' for c in checks),scene=scene,stage=stage)
(out/'geometry-tests.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(bool(report['failed']))
