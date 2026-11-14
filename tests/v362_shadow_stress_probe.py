"""Edge padding/crop checks and actual four-light/four-caster 1080p60 workload."""
import base64,copy,io,json,statistics,sys,time,traceback
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
from obs_client import OBS
from v361_native import UI
out,bridge,previous=map(lambda v:Path(v).resolve(),sys.argv[1:4]);out.mkdir(parents=True,exist_ok=True)
info=json.loads((previous/'scene.json').read_text(encoding='utf-8'));base=info['base'];checks=[];workloads=[]
o=OBS();n=UI(bridge,info['stage'])
def write(name,data):(out/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
def check(ok,name,value=None):
    checks.append(dict(name=name,**{'pass':bool(ok)},value=value));print(('PASS ' if ok else 'FAIL ')+name+' '+str(value or ''),flush=True)
def put(**patch):m=copy.deepcopy(base);m.update(patch);n.request(m);time.sleep(.2)
def shot(name):
    data=o.call('GetSourceScreenshot',dict(sourceName=info['stage'],imageFormat='png',imageWidth=1920,imageHeight=1080))
    raw=base64.b64decode(data['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(raw);return Image.open(io.BytesIO(raw)).convert('RGBA')
try:
    o.call('SetCurrentProgramScene',dict(sceneName=info['scene']));time.sleep(.5)
    receiver,caster=base['layers'];put(shadow_opacity=0);plain=shot('plain')
    put(layers=[receiver,dict(caster,crop_left=170)],light_softness=160);cropped=shot('fully-cropped-alpha')
    change=sum(ImageStat.Stat(ImageChops.difference(plain.crop((1000,300,1750,800)),cropped.crop((1000,300,1750,800)))).mean)
    check(change<1,'cropping away opaque caster removes its shadow',change)
    pair=[receiver,dict(caster,x=62.5)]
    put(layers=pair,light_softness=0);hard=shot('outside-hard')
    put(layers=pair,light_softness=300);soft=shot('outside-soft')
    roi=(1800,450,1890,630)
    hardStrength=ImageStat.Stat(ImageChops.difference(plain,hard).crop(roi)).mean[0]
    softStrength=ImageStat.Stat(ImageChops.difference(plain,soft).crop(roi)).mean[0]
    check(softStrength>hardStrength+5,'outside receiver silhouette blurs into visible receiver edge',dict(hard=hardStrength,soft=softStrength))
    mixed=[receiver,dict(caster,instance_id='near-caster',z=500),dict(caster,instance_id='far-caster',z=0,x=-80)]
    put(layers=mixed,light_softness=300);time.sleep(2);warm=n.get()['_metrics'];time.sleep(3);after=n.get()['_metrics']
    check(after['allocations']==warm['allocations'] and after['renders']-warm['renders']>=150 and after['failures']==0,'mixed-depth anisotropic casters reuse each resolution without resize churn',dict(warm=warm,after=after))
    if '--cache-only' in sys.argv[4:]:raise SystemExit(any(not c['pass'] for c in checks))
    casters=[dict(caster,instance_id='stress-'+str(i),x=x) for i,x in enumerate([-500,-280,-60,160])]
    lamps=[dict(light_enabled=True,light_x=x,light_y=0,light_z=-500,light_intensity=0,light_ambient=.25,light_softness=300,light_size=0,shadows_enabled=True,shadow_opacity=1,receiver_instance='receiver',receiver_uuid=receiver['uuid']) for x in [-700,-400,-100]]
    put(layers=[receiver]+casters,light_ambient=.25,light_softness=300,extra_lights=lamps);time.sleep(2);shot('four-lights-four-casters')
    video=o.call('GetVideoSettings');check(video['baseWidth']==1920 and video['baseHeight']==1080 and video['fpsNumerator']/video['fpsDenominator']==60,'stress workload actual1080p60',video)
    warm=n.get()['_metrics'];before=o.call('GetStats');start=time.monotonic();samples=[]
    for _ in range(30):time.sleep(1);samples.append(o.call('GetStats'))
    after=n.get()['_metrics'];row=dict(name='four-lights-four-casters-radius300',elapsedSeconds=time.monotonic()-start,meanFPS=statistics.mean(s['activeFps'] for s in samples),meanCPU=statistics.mean(s['cpuUsage'] for s in samples),meanRenderMs=statistics.mean(s['averageFrameRenderTime'] for s in samples),addedSkippedFrames=samples[-1]['renderSkippedFrames']-before['renderSkippedFrames'],memoryDeltaMiB=samples[-1]['memoryUsage']-before['memoryUsage'],warm=warm,after=after,samples=samples);workloads.append(row)
    check(row['meanFPS']>59 and row['addedSkippedFrames']==0 and row['elapsedSeconds']>=30,'four-by-four soft shadows real30second performance',dict(fps=row['meanFPS'],cpu=row['meanCPU'],renderMs=row['meanRenderMs'],skipped=row['addedSkippedFrames']))
    check(after['allocations']==warm['allocations'] and after['failures']==warm['failures']==0 and after['renders']-warm['renders']>=1500,'four-by-four soft shadows stable cached GPU resources',dict(warm=warm,after=after))
except Exception:check(False,'shadow stress probe exception',traceback.format_exc())
finally:write('stress-tests.json',dict(checks=checks,failed=sum(not c['pass'] for c in checks),workloads=workloads));o.close()
raise SystemExit(any(not c['pass'] for c in checks))
