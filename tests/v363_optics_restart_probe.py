"""Fresh-process persistence for local shadows and color-selected optics."""
import base64,io,json,sys,time,traceback
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
from obs_client import OBS
from v361_native import UI
out,bridge,previous=map(lambda x:Path(x).resolve(),sys.argv[1:4]);out.mkdir(parents=True,exist_ok=True)
o=OBS();checks=[]
def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def check(ok,name,value=None):
 checks.append(dict(name=name,**{'pass':bool(ok)},value=value));print(('PASS ' if ok else 'FAIL ')+name+' '+str(value or ''),flush=True)
try:
 info=read(previous/'restart-scene.json');expected=read(previous/'restart-expected.json');n=UI(bridge,info['stage']);actual=n.get()
 check(actual['layers'][0]['fx_stack']==expected['layers'][0]['fx_stack'],'all new effect colors and controls restored from disk')
 check(actual['layers'][0]['instance_id']==expected['layers'][0]['instance_id'],'layer instance association survives restart')
 o.call('SetCurrentProgramScene',dict(sceneName=info['scene']));time.sleep(.3);n.command('ui-refresh');n.command('ui-tab',value=2);controls=n.inspect('fx-restarted.png')
 check('spatial.fx34.optics-actor.FX05.5.angle' in controls and 'spatial.fx34.optics-actor.FX04.1.tolerance' in controls,'new controls restored in native Dock')
 data=o.call('GetSourceScreenshot',dict(sourceName=info['stage'],imageFormat='png',imageWidth=1920,imageHeight=1080));raw=base64.b64decode(data['imageData'].split(',',1)[1]);(out/'restored.png').write_bytes(raw)
 original=Image.open(previous/'restart-expected.png').convert('RGBA');restored=Image.open(io.BytesIO(raw)).convert('RGBA');difference=sum(ImageStat.Stat(ImageChops.difference(original,restored)).mean)
 check(difference<.1,'saved controls reproduce same OBS output after restart',difference)
 check(n.get()['_metrics']['failures']==0,'restarted shaders compile without GPU failures',n.get()['_metrics'])
except Exception:check(False,'restart exception',traceback.format_exc())
finally:(out/'restart-tests.json').write_text(json.dumps(dict(checks=checks,failed=sum(not c['pass'] for c in checks)),ensure_ascii=False,indent=2),encoding='utf-8');o.close()
raise SystemExit(any(not c['pass'] for c in checks))
