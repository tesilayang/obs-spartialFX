"""Measure actual OBS penumbra profiles; separated sharp copies must fail."""
import base64,copy,io,json,math,sys,time,traceback
from pathlib import Path
from PIL import Image,ImageDraw,ImageChops,ImageStat
from obs_client import OBS
from v361_native import UI
out,bridge=map(lambda v:Path(v).resolve(),sys.argv[1:3]);out.mkdir(parents=True,exist_ok=True)
o=OBS();checks=[];profiles=[];stamp=str(time.time_ns());scene='Soft shadow QA '+stamp;stage='Soft shadow stage '+stamp
def check(ok,name,value=None):
    checks.append(dict(name=name,**{'pass':bool(ok)},value=value));print(('PASS ' if ok else 'FAIL ')+name+' '+str(value or ''),flush=True)
def write(name,data):(out/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
o.call('CreateScene',dict(sceneName=scene));ids=[]
for label,size in [('receiver',(1920,1080)),('caster',(240,500))]:
    path=out/(label+'.png');im=Image.new('RGBA',size,(180,180,180,255) if label=='receiver' else (0,0,0,0))
    if label=='caster':ImageDraw.Draw(im).rectangle((70,20,169,479),fill=(90,120,170,255))
    im.save(path);name=label+' '+stamp;o.call('CreateInput',dict(sceneName=scene,inputName=name,inputKind='image_source',inputSettings={'file':str(path)},sceneItemEnabled=False));ids.append(next(s['inputUuid'] for s in o.call('GetInputList')['inputs'] if s['inputName']==name))
layers=[dict(uuid=ids[0],instance_id='receiver',z=700,sx=1.5,sy=1.5,cast_shadow=False,rim_light=False),dict(uuid=ids[1],instance_id='caster',x=-280,z=0,cast_shadow=True,rim_light=False)]
base=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,fx_enabled=True,playing=False,timeline_preview=True,layers=layers,light_enabled=True,light_x=-1000,light_y=0,light_z=-500,light_intensity=0,light_ambient=1,light_size=0,light_rim=0,light_softness=0,shadow_opacity=1,shadows_enabled=True,receiver_instance='receiver',receiver_uuid=ids[0])
o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=base,sceneItemEnabled=True));o.call('SetCurrentProgramScene',dict(sceneName=scene));time.sleep(.6);n=UI(bridge,stage)
def put(**patch):m=copy.deepcopy(base);m.update(patch);n.request(m);time.sleep(.15);return n.get()
def shot(name):
    data=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=1920,imageHeight=1080));raw=base64.b64decode(data['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(raw);return Image.open(io.BytesIO(raw)).convert('RGBA')
try:
    put(shadow_opacity=0);plain=shot('unshadowed')
    for radius in [0,15,80,160,300]:
        put(light_softness=radius);image=shot('radius-'+str(radius));row=[]
        # Shadow is displaced to the right of the visible foreground; source-free row.
        for x in range(1000,1750):row.append(max(0,plain.getpixel((x,540))[0]-image.getpixel((x,540))[0]))
        peak=max(row);edge=[abs(row[i+1]-row[i]) for i in range(len(row)-1)]
        # A Gaussian projection has one lobe; sparse copies create several peaks or hard steps.
        peaks=sum(row[i]>row[i-1]+2 and row[i]>=row[i+1]+2 for i in range(1,len(row)-1))
        profiles.append(dict(radius=radius,profile=row,maxStep=max(edge),peak=peak,peaks=peaks))
        check(peak>8,'shadow remains visible at radius'+str(radius),peak)
        if radius>=80:check(max(edge)<=10,'continuous penumbra without sharp duplicate edges radius'+str(radius),dict(maxStep=max(edge),peaks=peaks))
    write('profiles.json',profiles)
    # A single thin occluder must remain one broad lobe at maximum softness.
    row=profiles[-1]['profile'];smooth=[sum(row[max(0,i-3):min(len(row),i+4)])/len(row[max(0,i-3):min(len(row),i+4)]) for i in range(len(row))]
    maximum=max(smooth);segments=0;inside=False
    for value in smooth:
        above=value>maximum*.7
        if above and not inside:segments+=1
        inside=above
    check(segments==1,'maximum radius produces one continuous shadow lobe',segments)
    check(sum(0<v<max(row)*.95 for v in row)>80,'large radius contains broad graded penumbra',sum(0<v<max(row)*.95 for v in row))
    original=out.parents[0]/'red'
    if (original/'radius-0.png').exists():
        put(light_softness=0);hard=shot('hard-regression')
        delta=sum(ImageStat.Stat(ImageChops.difference(hard,Image.open(original/'radius-0.png').convert('RGBA'))).mean)
        check(delta<1,'zero radius preserves original hard silhouette',delta)
        delta=sum(ImageStat.Stat(ImageChops.difference(plain,Image.open(original/'unshadowed.png').convert('RGBA'))).mean)
        check(delta<1,'no-shadow output preserves original image',delta)
    for size in [600,2000]:
        put(light_type=1,light_size=size,light_softness=0);image=shot('area-size-'+str(size))
        row=[max(0,plain.getpixel((x,540))[0]-image.getpixel((x,540))[0]) for x in range(1000,1750)]
        check(max(row)>3 and max(abs(a-b) for a,b in zip(row,row[1:]))<=10,'area emitter size gives continuous visible penumbra '+str(size),dict(peak=max(row),maxStep=max(abs(a-b) for a,b in zip(row,row[1:]))))
    put(light_softness=80);opaque=shot('opaque-caster')
    put(light_softness=80,layers=[layers[0],dict(layers[1],opacity=.4)]);faint=shot('translucent-caster')
    strength=lambda image:sum(max(0,plain.getpixel((x,540))[0]-image.getpixel((x,540))[0]) for x in range(1000,1750))
    ratio=strength(faint)/max(1,strength(opaque))
    check(.3<ratio<.5,'caster opacity attenuates shadow continuously',ratio)
    multi=[layers[0],layers[1],dict(layers[1],instance_id='caster-two',x=-80)]
    put(light_softness=80,layers=multi);image=shot('two-casters')
    check(strength(image)>strength(opaque)*1.2,'independent caster masks both contribute',dict(single=strength(opaque),multiple=strength(image)))
    narrow=[dict(layers[0],sx=10000/1920,sy=100/1080),layers[1]]
    put(layers=narrow,shadow_opacity=0);narrowplain=shot('narrow-receiver-unshadowed')
    put(layers=narrow,light_softness=300);image=shot('narrow-receiver-soft')
    row=[max(0,narrowplain.getpixel((x,540))[0]-image.getpixel((x,540))[0]) for x in range(1000,1900)]
    span=sum(v>max(row)*.5 for v in row);step=max(abs(a-b) for a,b in zip(row,row[1:]))
    check(max(row)>80 and 100<span<400 and step<=10,'anisotropic receiver preserves narrow-axis shadow detail',dict(peak=max(row),halfMaximumWidth=span,maxStep=step))
    for label,receiver in [('mirrored',dict(layers[0],sx=-1.5)),('rotated',dict(layers[0],rx=10,ry=15))]:
        pair=[receiver,layers[1]];put(layers=pair,shadow_opacity=0);off=shot(label+'-off')
        put(layers=pair,light_softness=160);on=shot(label+'-soft')
        difference=sum(ImageStat.Stat(ImageChops.difference(off,on)).mean)
        check(difference>1,'soft shadow remains valid on '+label+' receiver',difference)
    check(n.get()['_metrics']['failures']==0,'shadow edge cases leave renderer healthy',n.get()['_metrics'])
    write('scene.json',dict(scene=scene,stage=stage,base=base))
except Exception:check(False,'soft-shadow probe exception',traceback.format_exc())
finally:write('softness-tests.json',dict(checks=checks,failed=sum(not c['pass'] for c in checks)));o.close()
raise SystemExit(any(not c['pass'] for c in checks))
