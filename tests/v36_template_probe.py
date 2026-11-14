"""Installed OBS output and 1080p60 workload evidence from real UI-created presets."""
import base64, copy, io, json, statistics, sys, time, traceback
from pathlib import Path
from PIL import Image, ImageChops, ImageStat, ImageDraw
from obs_client import OBS
from native_control import Native

out, bridge, ui = map(lambda p: Path(p).resolve(), sys.argv[1:4])
out.mkdir(parents=True, exist_ok=True)
checks, workloads = [], []
def check(ok, name, value=None):
    checks.append(dict(name=name, pass_=bool(ok), value=value))
    print(('PASS ' if ok else 'FAIL ') + name + ' ' + str(value or ''), flush=True)
def diff(a, b): return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
def read(name): return json.loads((ui/name).read_text(encoding='utf-8-sig'))
o = OBS(); stamp = str(time.time_ns()); scene = 'V36 template output '+stamp; stage = 'V36 template output stage '+stamp
o.call('CreateScene', {'sceneName':scene})
o.call('CreateInput', dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage', inputSettings={},sceneItemEnabled=True))
o.call('SetCurrentProgramScene', {'sceneName':scene}); time.sleep(.3)
n = Native(bridge, stage)
def put(state, t=0, playing=False):
    state=copy.deepcopy(state); state.update(playing=playing,timeline_preview=True,timeline_position=t,timeline_position_us=str(round(t*1e6)),play_token=time.time_ns()//1000)
    n.current=state; n.request(state); time.sleep(.14)
def shot(label):
    d=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=640,imageHeight=360))
    raw=base64.b64decode(d['imageData'].split(',',1)[1]); (out/(label+'.png')).write_bytes(raw)
    return Image.open(io.BytesIO(raw)).convert('RGBA')
def metrics(): return n.get()['_metrics']
try:
    ids=('blocks','cross','iris','float','cards','pop','shutter','minimal')
    pictures=[]
    for identifier in ids:
        for finite in (False,True):
            key=('finite-' if finite else '')+identifier
            m=read('template-'+key+'.json'); frames=[]
            for t in (0,1.5,3,6):
                put(m,t); frames.append(shot(key+'-'+str(t)))
            change=max(diff(frames[0],p) for p in frames[1:])
            check(change>.02,key+' has visible motion at deterministic playhead times',round(change,4))
            put(m,1.5); repeat=shot(key+'-repeat')
            check(diff(frames[1],repeat)<.001,key+' seeking reproduces exact pixels')
            if identifier!='shutter': check(diff(frames[0],frames[-1])<.001,key+' closes its finite interval',round(diff(frames[0],frames[-1]),4))
            pictures.append((key,frames[2]))
    # Show all original UI-produced templates side-by-side for human inspection.
    sheet=Image.new('RGB',(1280,8*208),'#12151f'); draw=ImageDraw.Draw(sheet)
    for i,(name,picture) in enumerate(pictures):
        x=(i%2)*640; y=(i//2)*208
        sheet.paste(picture.resize((320,180)).convert('RGB'),(x,y+22)); draw.text((x+4,y+4),name,fill='white')
    sheet.save(out/'template-contact-sheet.png')
    # Same three layers; 30 seconds per workload, with no screenshot reads during sampling.
    loop=read('template-blocks.json'); finite=read('template-finite-cross.json')
    empty=copy.deepcopy(loop); empty.update(presets_instances=[],presets_geometries=[],presets_motion_blocks=[])
    for layer in empty['layers']: layer['animation']=[]
    decoration=copy.deepcopy(loop); geometry_ids={g['instance_id'] for g in decoration['presets_geometries']}
    decoration['presets_motion_blocks']=[b for b in decoration['presets_motion_blocks'] if b['target']['instance_id'] in geometry_ids]
    for owner in decoration['presets_instances']: owner['end_us']='600000000'
    for block in decoration['presets_motion_blocks']: block['end_us']='600000000'
    single=copy.deepcopy(empty); receiver=single['layers'][0]['instance_id']
    mask=dict(instance_id='perf-mask',shape=2,mode=1,width=800,height=650,z=-100,enabled=True,visible=True,opacity=1,receivers=[dict(instance_id=receiver)])
    single['presets_geometries']=[mask]
    combined=copy.deepcopy(decoration); combined['presets_geometries'].extend(dict(mask,instance_id='perf-mask-'+str(i),receivers=[dict(instance_id=l['instance_id'])]) for i,l in enumerate(combined['layers']))
    video=o.call('GetVideoSettings'); check(video['baseWidth']==1920 and video['baseHeight']==1080 and video['fpsNumerator']/video['fpsDenominator']==60,'performance actual OBS setting is 1080p60',video)
    for label,m in [('disabled',empty),('finite-motion',finite),('decoration',decoration),('single-mask',single),('combined-three-masks',combined)]:
        m['loop']=True; put(m,0,True); time.sleep(2); warm=metrics(); before=o.call('GetStats'); start=time.monotonic(); samples=[]
        for i in range(30):
            time.sleep(1); samples.append(o.call('GetStats'))
        elapsed=time.monotonic()-start; after=metrics(); end=samples[-1]
        row=dict(name=label,elapsedSeconds=elapsed,layerCount=len(m['layers']),geometryCount=len(m['presets_geometries']),before=before,samples=samples,warm=warm,after=after,
                 meanCPU=statistics.mean(s['cpuUsage'] for s in samples),meanFPS=statistics.mean(s['activeFps'] for s in samples),meanOBSRenderMs=statistics.mean(s['averageFrameRenderTime'] for s in samples),
                 addedSkippedFrames=end['renderSkippedFrames']-before['renderSkippedFrames'],memoryDeltaMiB=end['memoryUsage']-before['memoryUsage'])
        workloads.append(row); check(elapsed>=30 and row['meanFPS']>59 and row['addedSkippedFrames']==0,label+' 30sec1080p60 with zero added render skips',dict(fps=row['meanFPS'],cpu=row['meanCPU'],renderMs=row['meanOBSRenderMs'],memoryDeltaMiB=row['memoryDeltaMiB']))
        check(after['allocations']==warm['allocations'] and after['failures']==warm['failures']==0,label+' uses stable cached GPU resources')
        if label=='disabled': check(after['passes']==warm['passes'],'disabled presets add zero passes')
        shot('performance-'+label)
except Exception:
    check(False,'template or performance probe exception',traceback.format_exc())
finally:
    try: n.request(dict(schema_version=6,layers=[],presets_geometries=[],presets_motion_blocks=[],presets_instances=[]))
    except Exception: pass
    o.close()
for c in checks: c['pass']=c.pop('pass_')
report=dict(checks=checks,failed=sum(not c['pass'] for c in checks),workloads=workloads,scene=scene,stage=stage,hardwareGPUTiming='NOT RUN',powerMeasurement='NOT RUN')
(out/'template-tests.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(bool(report['failed']))
