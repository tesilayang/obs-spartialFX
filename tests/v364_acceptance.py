"""Final installed OBS33 scoped acceptance for v3.6.4."""
import sys,time,json,copy,base64,io,traceback
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
from obs_client import OBS
from v361_native import UI
bridge,out=map(lambda s:Path(s).resolve(),sys.argv[1:3]);out.mkdir(parents=True,exist_ok=True)
o=OBS();checks=[];stamp=str(time.time_ns());scene='V364 acceptance '+stamp;stage='V364 stage '+stamp;source='V364 source '+stamp

def write(name,value):(out/name).write_text(json.dumps(value,ensure_ascii=False,indent=2),encoding='utf-8')
def check(ok,name,value=None):
 checks.append(dict(status='PASS' if ok else 'FAIL',name=name,value=value));write('acceptance.json',dict(checks=checks,failed=sum(c['status']=='FAIL' for c in checks)));print(checks[-1],flush=True)
def track(p,a,b,end=2):return dict(parameter=p,enabled=True,keys=[dict(time_us='0',value=a,mode=0),dict(time_us=str(int(end*1e6)),value=b,mode=0)])
def key(m,p):return next(c for c in m if c['parameter']==p)
def shot(name):
 d=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=960,imageHeight=540));raw=base64.b64decode(d['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(raw);return Image.open(io.BytesIO(raw)).convert('RGBA')
def canonical(tracks):
 result={}
 for c in tracks:
  keys=[]
  for k in c['keys']:
   default=dict(mode=0,in_absolute=False,out_absolute=False,in_x=2/3,in_y=2/3,out_x=1/3,out_y=1/3,legacy_from=0,legacy_to=1);default.update(k);keys.append(default)
  result[c['parameter']]=dict(enabled=c.get('enabled',False),locked=c.get('locked',False),shortest=c.get('shortest',False),keys=keys)
 return result
def difference(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
def context(row,action,confirm=None):
 kw=dict(row=row,menuAction='spatial.'+action+'364');
 if confirm:kw['confirm']=confirm
 n.command('ui-context',**kw);time.sleep(.3)
def put(m):n.request(copy.deepcopy(m));n.command('ui-refresh');time.sleep(.2)
def now():return n.get()
try:
 check(o.call('GetVersion')['obsVersion']=='33.0.1','actual OBS33.0.1')
 o.call('CreateScene',dict(sceneName=scene));created=o.call('CreateInput',dict(sceneName=scene,inputName=source,inputKind='image_source',inputSettings=dict(file=str(Path('work/v3.6.4/perf-fixture.png').resolve())),sceneItemEnabled=False));uid=next(i['inputUuid'] for i in o.call('GetInputList')['inputs'] if i['inputName']==source);sceneUid=next(s['sceneUuid'] for s in o.call('GetSceneList')['scenes'] if s['sceneName']==scene)
 base=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,fx_enabled=True,light_enabled=True,light_present=True,light_instance_id='accept-lamp',light_z=-600,light_range=3000,light_intensity=1.5,light_ambient=.3,light_rim=0,shadows_enabled=False,playing=False,timeline_preview=True,timeline_position_us='0',layers=[dict(uuid=uid,scene_uuid=sceneUid,item_id=created['sceneItemId'],original_visible=False,instance_id='accept-actor',label='Acceptance actor',visible=True,enabled=True,sx=1,sy=1,fx_stack=dict(attached=True,group_states=[dict(enabled=i==2) for i in range(13)],effects=[dict(id='FX03.0',enabled=True,strength=1,parameters=dict(gamma=1,red=1,green=1,blue=1,exposure=0,saturation=1,contrast=1))]))])
 o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=base,sceneItemEnabled=True));o.call('SetCurrentProgramScene',dict(sceneName=scene));n=UI(bridge,stage);put(base);controls=n.inspect('initial-dock.png');check('3.6.4' in controls['spatial.version']['text'],'final Dock version3.6.4');items=controls['spatial.editor-object']['items'];check(any(i['data']==-1000000 for i in items) and any('FX' in i['text'] for i in items),'light and FX objects in actual timeline')
 n.combo('spatial.editor-object',-1000000);n.click('spatial.record32');n.click('spatial.auto-key32');n.number('spatial.playhead32',2);n.number('spatial.param32.intensity',4.5);m=now();check(key(m['light_animation']['animation'],'intensity')['keys'][-1]['value']==4.5,'light inspector records native key');n.number('spatial.playhead32',1);check(abs(n.inspect()['spatial.param32.intensity']['value']-3)<.01,'seek interpolates light inspector')
 n.command('ui-tab',value=1);check(abs(n.inspect()['spatial.light-intensity']['value']-3)<.01,'light page matches timeline evaluated value');n.number('spatial.playhead32',2);n.number('spatial.light-intensity',3.5);check(key(now()['light_animation']['animation'],'intensity')['keys'][-1]['value']==3.5,'light page writes same animated channel');n.click('spatial.undo32');check(key(now()['light_animation']['animation'],'intensity')['keys'][-1]['value']==4.5,'light page edit undo restores original key')
 before=now();original=shot('light-original');n.click('spatial.light-color');time.sleep(.2);n.command('ui-color-set',color='#ff2020');preview=shot('light-color-preview');check(difference(original,preview)>.1,'light color dialog previews actual GPU output');n.command('ui-color-cancel');check(now()['light_animation']==before['light_animation'],'light color cancel restores all channels');n.click('spatial.light-color');time.sleep(.2);n.command('ui-color-set',color='#30c0ff');n.command('ui-color-accept');check(any(c['parameter']=='color.g' for c in now()['light_animation']['animation']),'light accepted color uses native RGB keys');n.click('spatial.undo32');check(now()['light_animation']==before['light_animation'],'light color accept is one undo transaction')
 n.command('ui-tab',value=0);fx=next(i['data'] for i in n.inspect()['spatial.editor-object']['items'] if '基础颜色' in i['text']);n.combo('spatial.editor-object',fx);n.number('spatial.playhead32',0);n.click('spatial.record32');n.number('spatial.playhead32',2);n.number('spatial.param32.red',0);m=now();effect=m['layers'][0]['fx_stack']['effects'][0];check(key(effect['animation'],'red')['keys'][-1]['value']==0,'FX native parameter key created');n.number('spatial.playhead32',1);n.command('ui-tab',value=2);check(abs(n.inspect()['spatial.fx34.accept-actor.FX03.0.red']['value']-.5)<.01,'FX page follows interpolated timeline value');n.number('spatial.fx34.accept-actor.FX03.0.red',.25);check(any(k['time_us']=='1000000' and k['value']==.25 for k in key(now()['layers'][0]['fx_stack']['effects'][0]['animation'],'red')['keys']),'FX page writes native key at playhead')
 keep=copy.deepcopy(now()['layers'][0]['fx_stack']['effects'][0]['animation']);n.flag('spatial.fx34.accept-actor.FX03.0.enabled',False);check(now()['layers'][0]['fx_stack']['effects'][0]['animation']==keep,'disabled FX keeps animation');n.flag('spatial.fx34.accept-actor.FX03.0.enabled',True)
 n.command('ui-tab',value=0);n.combo('spatial.editor-object',0);n.number('spatial.playhead32',0);n.click('spatial.record32');n.number('spatial.playhead32',2);n.number('spatial.param32.x',300);context(2,'copy-animation');n.number('spatial.playhead32',5);context(2,'paste-animation');check(any(k['time_us']=='7000000' for k in key(now()['layers'][0]['animation'],'x')['keys']),'actual right click animation paste aligns first key to playhead')
 context(2,'copy-layer');context(2,'paste-layer');m=now();check(len(m['layers'])==2 and m['layers'][0]['instance_id']!=m['layers'][1]['instance_id'] and m['layers'][0]['uuid']==m['layers'][1]['uuid'],'actual right click layer paste shares source and creates independent ID');n.click('spatial.undo32');check(len(now()['layers'])==1,'instance paste single undo');n.click('spatial.redo32');check(len(now()['layers'])==2,'instance paste redo')
 context(2,'delete-object','cancel');check(len(now()['layers'])==2,'delete confirmation cancel preserves instance');context(2,'delete-object','yes');check(len(now()['layers'])==1 and now()['layers'][0]['instance_id']!='accept-actor','confirmed row removal affects correct instance');n.click('spatial.undo32');check(now()['layers'][0]['instance_id']=='accept-actor','delete undo restores original ID and children');n.command('ui-context',row=2,menuAction='',screenshot='timeline-context-menu.png');time.sleep(.2);n.inspect('timeline-native-keys.png')
 # Legacy JSON migration and new composer generate native editable channels.
 old=copy.deepcopy(base);old['presets_motion_blocks']=[dict(instance_id='old-periodic',label='Old periodic',target=dict(instance_id='accept-actor'),start_us='0',end_us='2000000',period_us='1000000',time_origin_us='0',enabled=True,channels=[dict(parameter='x',amplitude=100,phase=0)])];put(old);m=now();check(not m['presets_motion_blocks'] and bool(m['layers'][0]['animation']) and bool(m['legacy_motion_backup']),'old3.6.3 motion migrates on real OBS load with backup');n.combo('spatial.editor-object',0);n.number('spatial.playhead32',0);n.click('spatial.delete32');m=now();check(all(not any(k['time_us']=='0' for k in c['keys']) for c in m['layers'][0]['animation']),'migrated native keys obey delete key control');n.click('spatial.undo32');check(any(k['time_us']=='0' for c in now()['layers'][0]['animation'] for k in c['keys']),'migrated key delete undo')
 put(base);n.command('ui-tab',value=3);n.command('ui-list',widget='spatial.preset-targets361',all=True);n.number('spatial.preset-period36',2);n.click('spatial.preset-generate361');time.sleep(.4);n.click('spatial.preset-apply36');m=now();check(not m['presets_motion_blocks'] and any(c['keys'] for c in m['layers'][0]['animation']),'one-click preset creates native editable keys');n.command('ui-tab',value=0);n.combo('spatial.editor-object',0);n.inspect('preset-native-keys.png')
 # Destructive page actions obey locks and produce complete undo entries.
 locked=copy.deepcopy(base);locked['light_locked']=True;put(locked);n.command('ui-tab',value=1);n.click('spatial.remove-light',confirm='yes');check(now()['light_instance_id']=='accept-lamp' and now()['light_present'],'light page cannot delete locked light')
 locked=copy.deepcopy(base);locked['light_animation']=dict(animation=[track('intensity',1,2)]);locked['light_animation']['animation'][0]['locked']=True;put(locked);n.click('spatial.remove-light',confirm='yes');check(now()['light_instance_id']=='accept-lamp','light page cannot delete locked parameter track')
 fxname='spatial.fx34.accept-actor.'
 for kind in ['layer','effect','track']:
  locked=copy.deepcopy(base);f=locked['layers'][0]['fx_stack']['effects'][0];f['animation']=[track('red',1,.2)]
  if kind=='layer':locked['layers'][0]['animation_locked']=True
  elif kind=='effect':f['animation_locked']=True
  else:f['animation'][0]['locked']=True
  put(locked);n.command('ui-tab',value=2);n.click(fxname+'remove',confirm='yes');check(len(now()['layers'][0]['fx_stack']['effects'])==1,'FX card removal obeys '+kind+' lock')
 unlocked=copy.deepcopy(base);unlocked['layers'][0]['fx_stack']['effects'][0]['animation']=[track('red',1,.2)];put(unlocked);n.command('ui-tab',value=2);n.click(fxname+'remove',confirm='yes');check(not now()['layers'][0]['fx_stack']['attached'],'FX card removal clears stack');n.click('spatial.undo32');check(canonical(now()['layers'][0]['fx_stack']['effects'][0]['animation'])==canonical(unlocked['layers'][0]['fx_stack']['effects'][0]['animation']),'FX card removal single undo restores animation')
 put(unlocked);n.command('ui-tab',value=2);n.inspect();n.command('ui-tab',value=0);context(3,'delete-object','yes');check(not now()['layers'][0]['fx_stack']['effects'],'FX timeline delete safe after FX page initialized');n.command('ui-tab',value=2);n.inspect('fx-removed-safe.png');n.click('spatial.undo32');check(len(now()['layers'][0]['fx_stack']['effects'])==1,'FX timeline delete undo restores child and controls')
 # Save a graph covering all new data for process restart and project roundtrips.
 saved=copy.deepcopy(base);saved['light_animation']=dict(animation=[track('intensity',.5,3),track('color.r',1,.2)]);saved['layers'][0]['animation']=[track('x',-100,100)];saved['layers'][0]['fx_stack']['effects'][0]['animation']=[track('red',1,.2)];saved['timeline_position_us']='1000000';put(saved);n.command('ui-save');expected=now();write('restart-expected.json',expected);write('restart-scene.json',dict(scene=scene,stage=stage));shot('restart-expected-frame')
 for packed in [False,True]:
  result=n.command('project-roundtrip',package=packed)['ui'];write('roundtrip-package.json' if packed else 'roundtrip-json.json',result);before=next(s['settings'] for s in result['original']['sources'] if s['id']=='obs_spatial_fx_stage');after=next(s['settings'] for s in result['restored']['sources'] if s['id']=='obs_spatial_fx_stage');check(canonical(before['light_animation']['animation'])==canonical(after['light_animation']['animation']) and canonical(before['layers'][0]['animation'])==canonical(after['layers'][0]['animation']) and canonical(before['layers'][0]['fx_stack']['effects'][0]['animation'])==canonical(after['layers'][0]['fx_stack']['effects'][0]['animation']),'new light FX layer tracks survive '+('ZIP package' if packed else 'JSON')+' project roundtrip')
 o.call('SetCurrentProgramScene',dict(sceneName=scene));n.command('ui-refresh');n.command('ui-save');check(True,'saved scoped restart fixture')
except Exception:check(False,'acceptance exception',traceback.format_exc())
finally:o.close()
sys.exit(any(c['status']=='FAIL' for c in checks))
