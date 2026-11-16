"""Scoped real OBS33 UI and rendering acceptance for v3.6.5."""
import sys,time,json,copy,base64,io,traceback
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
from obs_client import OBS
from v361_native import UI
bridge,out=map(lambda p:Path(p).resolve(),sys.argv[1:3]);out.mkdir(parents=True,exist_ok=True)
checks=[];o=OBS();stamp=str(time.time_ns());scene='V365 UI '+stamp;stage='V365 stage '+stamp

def write(name,data):(out/name).write_text(json.dumps(data,ensure_ascii=False,indent=2),encoding='utf-8')
def check(ok,name,data=None):
 checks.append(dict(status='PASS' if ok else 'FAIL',name=name,data=data));write('acceptance.json',dict(checks=checks,failed=sum(c['status']=='FAIL' for c in checks)));print(checks[-1],flush=True)
def inspect(name=None):
 r=n.command('ui-inspect',**(dict(screenshot=name+'.png',wholeWindow=True) if name else {}))['ui']
 if name:write(name+'.json',r)
 return r,{c['name']:c for c in r['controls']}
def shot(name):
 r=o.call('GetSourceScreenshot',dict(sourceName=stage,imageFormat='png',imageWidth=960,imageHeight=540));raw=base64.b64decode(r['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(raw);return Image.open(io.BytesIO(raw)).convert('RGB')
try:
 check(o.call('GetVersion')['obsVersion']=='33.0.1','actual OBS33.0.1')
 o.call('CreateScene',dict(sceneName=scene));s=o.call('CreateInput',dict(sceneName=scene,inputName='V365 fixture '+stamp,inputKind='image_source',inputSettings=dict(file=str(Path('tests/fixtures/fx-ui.png').resolve())),sceneItemEnabled=False));uid=s['inputUuid'];sid=next(s['sceneUuid'] for s in o.call('GetSceneList')['scenes'] if s['sceneName']==scene)
 layer=dict(uuid=uid,scene_uuid=sid,item_id=s['sceneItemId'],original_visible=False,instance_id='ui-owner',label='FX UI regression',visible=True,enabled=True,sx=1,sy=1,fx_stack=dict(attached=True,group_states=[dict(enabled=True) for i in range(13)],effects=[dict(id='FX03.0',enabled=True,strength=1)]))
 model=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,fx_enabled=False,playing=False,layers=[layer])
 o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings=model,sceneItemEnabled=True));o.call('SetCurrentProgramScene',dict(sceneName=scene));n=UI(bridge,stage);n.request(model);n.command('ui-refresh');n.command('ui-tab',value=2);time.sleep(.4)
 _,controls=inspect('initial');check('3.6.5' in controls['spatial.version']['text'],'installed Dock version3.6.5')
 scroll='spatial.fx-scroll34';prefix='spatial.fx34.ui-owner.';target=prefix+'FX12.9.enabled'
 n.command('ui-scroll',widget=scroll,target=target);_,controls=inspect('before-lower-toggle');before=controls[scroll]['scroll'];check(before>600,'long FX page scrolled to lower groups',before)
 for i in range(8):
  n.flag(target,i%2==0);_,controls=inspect();check(abs(controls[scroll]['scroll']-before)<4,'lower effect toggle retains position '+str(i),[before,controls[scroll]['scroll']])
 n.flag(target,True);_,controls=inspect('after-lower-toggle');s=controls[scroll];c=controls[target];check(s['y']<=c['y']<=s['y']+s['height'],'clicked effect stays in viewport',[c['y'],s['y'],s['height']])
 # Rebuild requested by default reset must also retain the viewport.
 n.click(prefix+'FX12.9.reset');_,controls=inspect('after-effect-reset');check(abs(controls[scroll]['scroll']-before)<4,'effect reset rebuild retains scroll',[before,controls[scroll]['scroll']])
 n.flag(target,False);n.command('ui-scroll',widget=scroll,target=prefix+'FX03.0.red');n.number(prefix+'FX03.0.red',.05);colored=shot('effect-color');n.number(prefix+'FX03.0.red',1);original=shot('original-color');difference=sum(ImageStat.Stat(ImageChops.difference(original,colored)).mean);check(difference>.5,'FX parameters still change actual GPU output',difference)
 n.command('ui-scroll',widget=scroll,target=target);n.command('ui-help-hover',widget=target);time.sleep(1.15);r,_=inspect('hover-help');check(r.get('help',{}).get('visible') and r['help']['inside'] and not r['help']['native'],'delayed help is visible inside OBS window',r.get('help'));check(not any(w['tooltip'] for w in r['surfaces']),'OBS has no visible native tooltip surface',r['surfaces'])
 n.command('ui-tab',value=0);n.command('ui-scroll',widget='spatial.editor-scroll33',target='spatial.record32');n.command('ui-help-hover',widget='spatial.record32');time.sleep(1.15);r,_=inspect('timeline-help');check(r.get('help',{}).get('visible') and r['help']['inside'],'shared timeline tooltip still works')
 n.command('ui-tab',value=1);n.command('ui-help-hover',widget='spatial.light-enabled');time.sleep(1.15);r,_=inspect('light-help');check(r.get('help',{}).get('visible') and r['help']['inside'],'shared light tooltip still works')
 n.command('ui-tab',value=3);n.command('ui-help-hover',widget='spatial.preset-generate361');time.sleep(1.15);r,_=inspect('preset-help');check(r.get('help',{}).get('visible') and r['help']['inside'],'shared preset tooltip still works')
 n.command('ui-tab',value=2);n.command('ui-help-hover',widget=target);time.sleep(1.15)
 n.command('ui-help-deactivate');r,_=inspect('deactivated');check(not r.get('help',{}).get('visible',False),'application deactivate hides help')
 n.command('ui-help-hover',widget=target);time.sleep(.2);n.command('ui-help-deactivate');time.sleep(1.15);r,_=inspect();check(not r.get('help',{}).get('visible',False),'deactivate cancels pending tooltip')
 n.command('ui-tab',value=2);write('scene.json',dict(scene=scene,stage=stage));check(n.get()['_metrics']['failures']==0,'FX render reports no failures')
except Exception:check(False,'acceptance exception',traceback.format_exc())
finally:o.close()
sys.exit(any(c['status']=='FAIL' for c in checks))
