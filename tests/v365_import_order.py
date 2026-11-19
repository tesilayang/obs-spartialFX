"""Import in separate batches after OBS source-list changes; keep foreground visible."""
import base64,json,sys,time,subprocess,io
from pathlib import Path
from PIL import Image
from obs_client import OBS
from v361_native import UI
bridge,out=map(lambda p:Path(p).resolve(),sys.argv[1:3]);out.mkdir(parents=True,exist_ok=True)
o=OBS();stamp=str(time.time_ns());scene='Import order '+stamp;stage='Import stage '+stamp
checks=[]
def check(ok,name,data=None):
 checks.append(dict(status='PASS' if ok else 'FAIL',name=name,data=data));print(checks[-1],flush=True)
 (out/'checks.json').write_text(json.dumps(checks,ensure_ascii=False,indent=2),encoding='utf-8')
def shot(label,source=None):
 r=o.call('GetSourceScreenshot',dict(sourceName=source or stage,imageFormat='png'))
 raw=base64.b64decode(r['imageData'].split(',',1)[1]);(out/(label+'.png')).write_bytes(raw)
 return Image.open(io.BytesIO(raw)).convert('RGBA')
def select(name):
 n.command('ui-refresh');n.command('ui-tab',value=0)
 c=n.inspect()['spatial.scene-sources'];row=next(i for i,s in enumerate(c['items']) if s['text'].startswith(name))
 n.command('ui-list',widget='spatial.scene-sources',row=row);n.click('spatial.import-selected');time.sleep(.8)

fixture=subprocess.Popen([str(Path('build/spatial-capture-fixture.exe').resolve())]);time.sleep(2)
o.call('CreateScene',dict(sceneName=scene));o.call('SetCurrentProgramScene',dict(sceneName=scene))
image=out/'background.png';Image.new('RGBA',(1920,1080),(170,20,110,255)).save(image)
bg=o.call('CreateInput',dict(sceneName=scene,inputName='Background '+stamp,inputKind='image_source',inputSettings=dict(file=str(image)),sceneItemEnabled=True))
win=o.call('CreateInput',dict(sceneName=scene,inputName='Window '+stamp,inputKind='window_capture',inputSettings=dict(window='Spatial FX Capture Fixture:SpatialFXCaptureFixture:spatial-capture-fixture.exe',method=2,cursor=False),sceneItemEnabled=True))
extras=[]
for i in range(2):
 name='Unused '+str(i)+' '+stamp;extras.append(name)
 o.call('CreateInput',dict(sceneName=scene,inputName=name,inputKind='image_source',inputSettings=dict(file=str(image)),sceneItemEnabled=False))
o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings={},sceneItemEnabled=True))
n=UI(bridge,stage);n.request(dict(schema_version=6,width=1920,height=1080,layers=[],fx_enabled=False))
time.sleep(4)
select('Window '+stamp)
for name in extras:o.call('RemoveInput',dict(inputName=name))
select('Background '+stamp)
state=n.get();(out/'state.json').write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
layers=state['layers'];window=next(l for l in layers if l['uuid']==win['inputUuid']);background=next(l for l in layers if l['uuid']==bg['inputUuid'])
check(window['z']<background['z'],'separate import preserves OBS foreground/background depth',dict(window=window['z'],background=background['z']))
check(o.call('GetSourceActive',dict(sourceUuid=win['inputUuid']))['videoShowing'],'hidden original window stays showing through stage')
render=shot('imported-stage');source=shot('captured-window','Window '+stamp)
pixels=[(20,20),(100,100),(600,340)]
error=max(abs(render.getpixel(p)[c]-source.getpixel(p)[c]) for p in pixels for c in range(4))
check(error<=3,'window content retains original screen position and color above background',dict(maxPixelError=error))
select('Window '+stamp);duplicate=n.get()['layers'][-1]
check(duplicate['instance_id']!=window['instance_id'] and abs(duplicate['z']-window['z'])<1e-6,'duplicate window has independent identity and matching depth')
check(shot('duplicate-stage').getpixel((100,100))==render.getpixel((100,100)),'duplicate import retains window footprint')
stageItem=next(s['sceneItemId'] for s in o.call('GetSceneItemList',dict(sceneName=scene))['sceneItems'] if s['sourceName']==stage)
o.call('SetSceneItemEnabled',dict(sceneName=scene,sceneItemId=stageItem,sceneItemEnabled=False));time.sleep(.4)
o.call('SetSceneItemEnabled',dict(sceneName=scene,sceneItemId=stageItem,sceneItemEnabled=True));time.sleep(1)
check(shot('stage-reactivated').getpixel((100,100))==render.getpixel((100,100)),'window survives stage hide and show')
n.request(dict(state,layers=[background]));time.sleep(.5)
check(shot('window-removed').getpixel((100,100))==(170,20,110,255),'removing window instances reveals background')
check(not o.call('GetSourceActive',dict(sourceUuid=win['inputUuid']))['videoShowing'],'removing last managed window releases showing state')
n.request(state);time.sleep(1)
check(shot('window-restored').getpixel((100,100))==render.getpixel((100,100)),'restoring managed window reacquires capture')
n.request(dict(state,layers=[]))
for item in (win,bg):o.call('SetSceneItemEnabled',dict(sceneName=scene,sceneItemId=item['sceneItemId'],sceneItemEnabled=True))
n.command('ui-refresh');n.click('spatial.import-all');time.sleep(1)
batch=n.get()['layers'];check(len(batch)==2 and batch[0]['z']>batch[1]['z'],'empty-stage import-all preserves OBS source stack')
check(shot('batch-import-stage').getpixel((100,100))==render.getpixel((100,100)),'initial batch import preserves window image placement')
n.command('ui-inspect',screenshot='imported-dock.png',wholeWindow=True)
o.close();sys.exit(0 if all(c['status']=='PASS' for c in checks) else 1)
