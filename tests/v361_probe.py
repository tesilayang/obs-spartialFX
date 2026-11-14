"""Installed OBS 33 native UI/algorithm evidence. All media are QA fixtures."""
import base64, copy, io, json, math, re, statistics, sys, time, traceback
from pathlib import Path
from PIL import Image, ImageDraw, ImageChops, ImageStat
from obs_client import OBS
from v361_native import UI

out, bridge = map(lambda s:Path(s).resolve(),sys.argv[1:3])
out.mkdir(parents=True,exist_ok=True)
checks=[];workloads=[];pictures=[];snapshots={}

def check(ok,name,value=None):
    row=dict(name=name,**{'pass':bool(ok)},value=value);checks.append(row)
    print(('PASS ' if ok else 'FAIL ')+name+((' '+str(value)) if value is not None else ''),flush=True)
    return bool(ok)

def write(name,data): (out/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
def diff(a,b): return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
def stage_settings(doc):return next(s['settings'] for s in doc['sources'] if s['id']=='obs_spatial_fx_stage')
def saved_recipe(m):
    value=m['presets_instances'][-1]['definition_snapshot']
    return json.loads(value) if isinstance(value,str) else value

def part_graph(m,kind,colors=True):
    """Ignore generated identities, compare the actual unrelated resolved output."""
    geometry_ids={g['instance_id']:'geometry-'+str(i) for i,g in enumerate(m['presets_geometries'])}
    def normalized(value):
        if isinstance(value,list):return [normalized(v) for v in value]
        if not isinstance(value,dict):return value
        result={}
        for k,v in value.items():
            if k in ('owner','label'):continue
            if not colors and k in ('color_r','color_g','color_b'):continue
            if k=='instance_id':
                if v in geometry_ids:result[k]=geometry_ids[v]
                elif v.startswith('v361-'):result[k]=v
                continue
            result[k]=normalized(v)
        return result
    blocks=[b for b in m['presets_motion_blocks'] if (b['target'].get('instance_id') in geometry_ids)==(kind=='decoration')]
    geometry=m['presets_geometries'] if kind=='decoration' else []
    return normalized(dict(geometries=geometry,blocks=blocks,layers=[l['animation'] for l in m['layers']] if kind=='motion' else []))

o=OBS();stamp=str(time.time_ns());scene='V361 native QA '+stamp;stage='V361 stage '+stamp
o.call('CreateScene',{'sceneName':scene})
layers=[]
for i,(label,col) in enumerate((('A','#36baeb'),('B','#e58d3e'),('C','#9b70d9'))):
    image=Image.new('RGBA',(640,360));draw=ImageDraw.Draw(image)
    draw.rounded_rectangle((20,20,620,340),radius=50,fill=col,outline='white',width=8)
    draw.text((290,160),label,fill='white',stroke_width=1)
    for x in range(40,620,40):draw.line((x,45,x,315),fill=(255,255,255,45),width=1)
    path=out/('fixture-'+label+'.png');image.save(path)
    name='V361 source '+label+' '+stamp
    o.call('CreateInput',dict(sceneName=scene,inputName=name,inputKind='image_source',inputSettings={'file':str(path)},sceneItemEnabled=False))
    source=next(s for s in o.call('GetInputList')['inputs'] if s['inputName']==name)
    layers.append(dict(uuid=source['inputUuid'],instance_id='v361-'+label,label=label,x=(i-1)*420,y=(i-1)*80,z=i*180,sx=1,sy=1,visible=True,enabled=True,receive_light=True,import_width=640,import_height=360))
base=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,fx_enabled=False,playing=False,timeline_preview=True,timeline_position_us='0',layers=layers,presets_geometries=[],presets_motion_blocks=[],presets_instances=[])
o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=base,sceneItemEnabled=True))
o.call('SetCurrentProgramScene',{'sceneName':scene});time.sleep(1)
n=UI(bridge,stage)

def put(m,t=0,playing=False):
    m=copy.deepcopy(m);m.update(playing=playing,timeline_preview=True,timeline_position_us=str(round(t*1e6)),play_token=time.time_ns()//1000)
    n.request(m);time.sleep(.13)

def refresh():n.command('ui-refresh');time.sleep(.14)
def reset():
    c=n.inspect()
    if c.get('spatial.preset-cancel36',{}).get('enabled'):n.click('spatial.preset-cancel36',ifEnabled=True)
    put(base);refresh();n.command('ui-tab',value=3)
    n.command('ui-list',widget='spatial.preset-targets361',all=True)

def shot(name):
    data=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=640,imageHeight=360))
    raw=base64.b64decode(data['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(raw)
    return Image.open(io.BytesIO(raw)).convert('RGBA')

def generate():n.click('spatial.preset-generate361');time.sleep(.18);return n.get()
def parts(motion='none',mask='none',decor='none'):
    n.combo('spatial.preset-motion-part361',motion);n.combo('spatial.preset-mask-part361',mask);n.combo('spatial.preset-decor-part361',decor)

try:
    refresh();n.command('ui-tab',value=3)
    c=n.inspect('preset-simple.png');write('initial-controls.json',c)
    check(not any(key in c for key in ('spatial.preset-choice','spatial.preset-name','spatial.save-preset','spatial.preset-legacy-toggle36')),'removed whole-stage preset interface')
    check(not c['spatial.preset-details-toggle361']['checked'],'detailed control initially folded')
    target=c['spatial.preset-targets361']['items']
    check(len(target)==3 and all(i['checked'] for i in target),'existing imported layers selected without role assignment')
    check(not c['spatial.preset-camera36']['checked'],'camera default off')
    style_controls=[name for name in c if name.startswith('spatial.preset-style-')]
    check(len(style_controls)==8,'eight beginner style cards')
    n.command('ui-list',widget='spatial.preset-targets361',ids=[])
    check(not n.inspect()['spatial.preset-generate361']['enabled'],'empty target selection disables generation')
    n.command('ui-list',widget='spatial.preset-targets361',all=True)
    original=n.get()
    m=generate();check(len(m['presets_instances'])==1,'one-click generate produces preview')
    time.sleep(2);check(len(n.get()['presets_instances'])==1 and n.inspect()['spatial.preset-cancel36']['enabled'],'generated preview persists without spontaneous cancellation')
    a=n.command('ui-inspect')['ui']['authoritative'];check(not a['presets_instances'],'preview absent from authoritative saved data')
    seed=m['presets_instances'][0]['seed'];n.click('spatial.preset-reroll361');time.sleep(.15);rerolled=n.get()
    check(len(rerolled['presets_instances'])==1 and rerolled['presets_instances'][0]['seed']!=seed,'reroll replaces preview and changes seed')
    n.click('spatial.preset-cancel36');check(not n.get()['presets_instances'],'cancel restores original scene')
    # Real finite playback must restart after completion and after a canceled preview.
    n.number('spatial.preset-period36',2);n.flag('spatial.preset-loop36',False)
    timed=generate();time.sleep(2.7)
    ended=n.inspect()['spatial.performance']['text'];first_token=n.get()['play_token']
    n.click('spatial.preset-reroll361');time.sleep(.95)
    resumed=n.inspect()['spatial.performance']['text'];second_token=n.get()['play_token']
    clock_match=re.search(r'动画 ([\d.]+)s',resumed)
    clock_ok=second_token!=first_token and clock_match and float(clock_match[1])<1.9
    check(clock_ok,'finished finite preview restarts when rerolled',dict(before=ended,after=resumed,beforeToken=first_token,afterToken=second_token))
    n.click('spatial.preset-cancel36')
    boundary=copy.deepcopy(base);boundary['play_token']=9007199254740991
    n.request(boundary);refresh();first_boundary=generate()['play_token']
    n.click('spatial.preset-cancel36');second_boundary=generate()['play_token']
    boundary_ok=first_boundary!=second_boundary and 0<=second_boundary<=9007199254740991
    check(boundary_ok,'cancel then regenerate uses distinct tokens at maximum serialized token',dict(first=first_boundary,second=second_boundary))
    write('preview-clock-green.json',dict(before=ended,after=resumed,beforeToken=first_token,afterToken=second_token,boundaryTokens=[first_boundary,second_boundary],**{'pass':bool(clock_ok and boundary_ok)}))
    n.click('spatial.preset-cancel36');n.number('spatial.preset-period36',6);n.flag('spatial.preset-loop36',True)
    put(base);refresh()
    m=generate();n.click('spatial.preset-apply36');committed=n.get()
    check(len(committed['presets_instances'])==1 and len(n.command('ui-inspect')['ui']['authoritative']['presets_instances'])==1,'apply commits generated composition')
    n.command('ui-tab',value=0);n.click('spatial.undo32');check(not n.get()['presets_instances'],'one undo removes complete generated composition')
    n.click('spatial.redo32');check(len(n.get()['presets_instances'])==1,'redo restores composition')
    n.command('ui-tab',value=3)
    for style in style_controls:
        reset();n.click(style);m=generate()
        check(len(m['presets_instances'])==1,'style generates: '+style)
        check(m['camera_x']==base.get('camera_x',0) and not m['animation'],'style leaves camera stationary: '+style)
        check(all(m['layers'][i][axis]==layers[i].get(axis,0) for i in range(3) for axis in ('x','y','z')),'style preserves base layout: '+style)
        snapshots[style]=m;write(style+'.json',m)
    reset();n.click('spatial.preset-details-toggle361');c=n.inspect('preset-details.png')
    for name,count in [('motion',18),('mask',22),('decor',20)]:
        items=c['spatial.preset-'+name+'-part361']['items'];real=[i for i in items if i['data'] not in ('none','')]
        check(len(real)==count,name+' library has actual implemented parts',len(real))
    catalogs={name:[i['data'] for i in c['spatial.preset-'+name+'-part361']['items'] if i['data'] not in ('none','')] for name in ('motion','mask','decor')}
    write('catalog.json',catalogs)
    reset();parts(motion='',mask='T09',decor='');n.flag('spatial.preset-loop36',True);prior=generate()
    n.click('spatial.preset-reroll-motion361');time.sleep(.15);current=n.get()
    check(part_graph(prior,'decoration')==part_graph(current,'decoration'),'only-motion reroll preserves exact mask and decoration layout/animation')
    prior=current;n.click('spatial.preset-reroll-decor361');time.sleep(.15);current=n.get()
    check(part_graph(prior,'motion')==part_graph(current,'motion'),'only-decoration reroll preserves exact source animation')
    prior=current;n.click('spatial.preset-reroll-colors361');time.sleep(.15);current=n.get()
    check(part_graph(prior,'motion')==part_graph(current,'motion') and part_graph(prior,'decoration',False)==part_graph(current,'decoration',False),'only-color reroll preserves geometry and all motion')
    recipe=saved_recipe(current)
    check(all(isinstance(recipe[k],dict) and len(recipe[k])>=3 for k in ('color_a','color_b','plane')),'resolved palette and plane survive OBS JSON canonicalization')
    check(all(recipe.get(k,0)>0 for k in ('motion_seed','mask_seed','decoration_seed')),'independent resolved stream seeds are persisted')
    n.flag('spatial.preset-loop36',True)
    for kind,ids in catalogs.items():
        for part in ids:
            reset();parts(**{kind if kind!='decor' else 'decor':part});n.flag('spatial.preset-loop36',True)
            m=generate();ok=len(m['presets_instances'])==1
            check(ok,kind+' manual compilation '+part)
            if ok:snapshots[part]=m;write(part+'.json',m)
    reset();parts(motion='M17',decor='D01');m=generate()
    before=copy.deepcopy(m);n.click('spatial.preset-color-a36');n.command('ui-color-set',color='#E53245');time.sleep(.2)
    colored=n.get();c=n.inspect('color-preview.png')
    check(c['spatial.preset-color-preview-a361']['text'].upper()=='#FFE53245','palette preview displays selected hex color')
    check(colored['presets_geometries']!=before['presets_geometries'],'palette changes temporary rendered geometry')
    n.command('ui-color-cancel');time.sleep(.15)
    restored=n.get();check(restored['presets_geometries']==before['presets_geometries'] and restored['presets_motion_blocks']==before['presets_motion_blocks'],'palette cancel restores exact previous preview')
    n.click('spatial.preset-cancel36')
    # All library samples come from the real native UI, then evaluate fixed clocks.
    for part,m in list(snapshots.items()):
        if part.startswith('spatial.'):continue
        frames=[]
        for j,t in enumerate((0,.55,1.5,3,4.5,5.45,6)):
            put(m,t);frames.append(shot(part+'-'+str(j)))
        variations=[diff(frames[0],f) for f in frames[1:-1]]
        check(max(variations)>.002,part+' changes actual OBS pixels over timeline',round(max(variations),4))
        check(all(f.getchannel('A').getextrema()[1]>0 for f in frames[1:-1]),part+' has visible intermediate result')
        check(diff(frames[0],frames[-1])<.5,part+' loop seam returns reproducibly',round(diff(frames[0],frames[-1]),4))
        image=Image.new('RGB',(640*3,360*2),'#17202b')
        for j,f in enumerate(frames[:6]):image.paste(f.convert('RGB'),((j%3)*640,(j//3)*360))
        image.resize((960,360)).save(out/(part+'-sequence.jpg'));pictures.append((part,frames[3]))
    a,b=snapshots['D05'],snapshots['D06'];differences=[]
    for t in (.55,1.5,3,4.5):
        put(a,t);left=shot('D05-compare-'+str(t));put(b,t);right=shot('D06-compare-'+str(t));differences.append(diff(left,right))
    check(max(differences)>.1,'D05 edge sweep and D06 crossed double bands have distinct actual output',differences)
    reset();parts(mask='T09');n.flag('spatial.preset-loop36',False);n.number('spatial.preset-cycles36',2);finite=generate()
    put(finite,3);first=shot('finite-mask-cycle1');put(finite,9);second=shot('finite-mask-cycle2')
    check(diff(first,second)<.01 and ImageStat.Stat(second.getchannel('A')).mean[0]>1,'finite two-cycle mask opens in both cycles')
    n.click('spatial.preset-cancel36');n.number('spatial.preset-cycles36',1)
    # UI local mask must remain in the receiver's local coordinates under a moved camera.
    reset();translated=copy.deepcopy(base);translated.update(camera_x=700,camera_ry=8)
    for layer in translated['layers']:layer['x']+=700
    put(translated,3);refresh();unmasked=shot('translated-camera-original')
    parts(mask='T09');n.flag('spatial.preset-loop36',False);local=generate();put(local,3);masked=shot('translated-camera-local-open')
    check(diff(unmasked,masked)<.5,'UI local mask fully open preserves layers under translated and rotated camera',diff(unmasked,masked))
    # All color rows, including stage lights, use live temporary snapshots.
    reset();lit=copy.deepcopy(base);lit.update(fx_enabled=True,light_enabled=True,light_intensity=2,light_ambient=.05,light_x=0,light_y=0,light_z=-350)
    put(lit);refresh();n.command('ui-tab',value=1)
    before=n.get();authoritative_before=n.command('ui-inspect')['ui']['authoritative'];original=shot('light-color-before');n.click('spatial.light-color');n.command('ui-color-set',color='#E52A3E');time.sleep(.2)
    live=n.get();controls=n.inspect('light-color-dialog.png');preview=shot('light-color-live')
    check(controls['spatial.light-color-preview33']['text']=='#FFE52A3E','light uses full-width hex color preview')
    check(abs(live['light_red']-229/255)<.001 and abs(live['light_green']-42/255)<.001,'light color live render snapshot updates before confirm')
    check(n.command('ui-inspect')['ui']['authoritative']==authoritative_before,'light color preview does not persist before confirm')
    check(diff(original,preview)>.01,'light palette adjustment changes actual stage pixels')
    n.command('ui-color-cancel');time.sleep(.1)
    check(abs(n.get()['light_green']-before['light_green'])<.001,'cancel light palette restores authoritative color')
    check(diff(original,shot('light-color-canceled'))<.01,'cancel light palette restores actual original pixels')
    n.click('spatial.shadow-color34');n.command('ui-color-set',color='#35C847');n.command('ui-color-cancel')
    check(abs(n.get()['shadow_green']-before['shadow_green'])<.001,'cancel shadow palette restores published color')
    n.click('spatial.shadow-color34');n.command('ui-color-set',color='#3547C8');live=n.get()
    check(abs(live['shadow_blue']-200/255)<.001,'shadow palette also previews live')
    n.command('ui-color-accept');check(abs(n.command('ui-inspect')['ui']['authoritative']['shadow_blue']-200/255)<.001,'shadow color accept persists selected value')
    n.command('ui-tab',value=0);n.click('spatial.undo32');check(abs(n.get()['shadow_blue']-before['shadow_blue'])<.001,'one undo restores accepted shadow color')
    n.command('ui-tab',value=3)
    # Lock invalid selection and key conflict must not partially mutate the graph.
    reset();locked=copy.deepcopy(base);locked['layers'][0]['animation_locked']=True;put(locked);refresh()
    check(n.get()['layers'][0]['animation_locked'],'lock fixture reaches serialized animation lock')
    c=n.inspect();write('locked-targets.json',c['spatial.preset-targets361']);target=c['spatial.preset-targets361']['items'];check(not next(i for i in target if i['id']=='v361-A')['enabled'],'locked layer excluded from simple selection')
    # Reconstruct scene JSON and an actual ZIP package through existing project code.
    reset();parts(motion='M17',mask='T09',decor='D01');n.flag('spatial.preset-loop36',True);m=generate();n.click('spatial.preset-apply36')
    for packaged in (False,True):
        result=n.command('project-roundtrip',package=packaged)['ui'];old,new=map(stage_settings,(result['original'],result['restored']))
        write('package-roundtrip.json' if packaged else 'json-roundtrip.json',result)
        check(len(new['presets_instances'])==1 and len(new['presets_geometries'])==len(old['presets_geometries']),('package' if packaged else 'json')+' retains complete generated graph')
        ids={l['instance_id'] for l in new['layers']};recipe=saved_recipe(new)
        check(all(i['instance_id'] in ids for i in recipe['selected']) and not ids.intersection(l['instance_id'] for l in old['layers']),'project rebinds recipe instance references '+str(packaged))
    n.command('ui-save');write('restart-expected.json',n.get())
    write('restart-scene.json',dict(scene=scene,stage=stage))
    # Keep fixed current-scene graph saved; performance temporarily renders copies.
    video=o.call('GetVideoSettings');check(video['baseWidth']==1920 and video['baseHeight']==1080 and video['fpsNumerator']/video['fpsDenominator']==60,'actual OBS benchmark 1080p60')
    for name,state in [('disabled',base),('motion',snapshots['M17']),('decoration',snapshots['D19']),('mask',snapshots['T35']),('combined',m)]:
        put(state,0,True);time.sleep(1);warm=n.get()['_metrics'];before=o.call('GetStats');started=time.monotonic();samples=[]
        for i in range(30):time.sleep(1);samples.append(o.call('GetStats'))
        after=n.get()['_metrics'];row=dict(name=name,elapsedSeconds=time.monotonic()-started,meanCPU=statistics.mean(s['cpuUsage'] for s in samples),meanFPS=statistics.mean(s['activeFps'] for s in samples),meanRenderMs=statistics.mean(s['averageFrameRenderTime'] for s in samples),addedSkippedFrames=samples[-1]['renderSkippedFrames']-before['renderSkippedFrames'],memoryDeltaMiB=samples[-1]['memoryUsage']-before['memoryUsage'],warm=warm,after=after,geometryCount=len(state.get('presets_geometries',[])),layerCount=3,samples=samples)
        workloads.append(row);check(row['meanFPS']>59 and row['addedSkippedFrames']==0 and row['elapsedSeconds']>=30,name+' actual30s workload',dict(fps=row['meanFPS'],cpu=row['meanCPU'],renderMs=row['meanRenderMs']))
        check(warm['allocations']==after['allocations'] and after['failures']==warm['failures']==0,name+' cached resources remain stable')
    put(m);refresh();n.command('ui-save')
except Exception:
    check(False,'native composition QA exception',traceback.format_exc())
finally:
    try:
        sheet=Image.new('RGB',(4*320,math.ceil(len(pictures)/4)*200),'#1d2430')
        draw=ImageDraw.Draw(sheet)
        for i,(name,pic) in enumerate(pictures):
            x=(i%4)*320;y=(i//4)*200;sheet.paste(pic.resize((320,180)).convert('RGB'),(x,y));draw.text((x+4,y+182),name,fill='white')
        if pictures:sheet.save(out/'library-contact-sheet.jpg')
    except Exception:pass
    write('native-tests.json',dict(checks=checks,failed=sum(not c['pass'] for c in checks),workloads=workloads,scene=scene,stage=stage))
    o.close()
raise SystemExit(any(not c['pass'] for c in checks))
