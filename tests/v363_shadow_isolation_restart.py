"""Verify removal of legacy auxiliary data survives a real OBS process restart."""
import base64,io,json,sys,time,traceback
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
from obs_client import OBS
from v361_native import UI

out,bridge,previous=[Path(p).resolve() for p in sys.argv[1:4]]
out.mkdir(parents=True,exist_ok=True);o=OBS();checks=[]
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def check(ok,name,value=None):
    checks.append(dict(name=name,**{'pass':bool(ok)},value=value));print(('PASS ' if ok else 'FAIL ')+name+' '+str(value or ''),flush=True)
try:
    info=read(previous/'restart-scene.json');expected=read(previous/'restart-expected.json')
    n=UI(bridge,info['stage']);actual=n.get()
    check(actual['layers'][0]['fx_stack']==expected['layers'][0]['fx_stack'],'shadow settings survive fresh-process reload')
    f=actual['layers'][0]['fx_stack']['effects'][0]
    check('auxiliary' not in f['parameters'] and 'color' not in f,'obsolete auxiliary fields remain absent after restart')
    o.call('SetCurrentProgramScene',dict(sceneName=info['scene']));time.sleep(.3)
    n.command('ui-refresh');n.command('ui-tab',value=2);controls=n.inspect('restarted-dock.png')
    check('spatial.fx34.shadow-actor.FX05.5.color2' in controls and not any('FX05.5.auxiliary' in k or ('FX05.5.color' in k and 'FX05.5.color2' not in k) for k in controls),'native Dock retains only shadow color after restart')
    data=o.call('GetSourceScreenshot',dict(sourceName=info['stage'],imageFormat='png',imageWidth=1920,imageHeight=1080))
    raw=base64.b64decode(data['imageData'].split(',',1)[1]);(out/'restored.png').write_bytes(raw)
    a=Image.open(previous/'restart-expected.png').convert('RGBA');b=Image.open(io.BytesIO(raw)).convert('RGBA')
    difference=sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
    check(difference<.05,'saved shadow reproduces same pixels after restart',difference)
    check(n.get()['_metrics']['failures']==0,'restarted final shaders have no GPU failures',n.get()['_metrics'])
except Exception:check(False,'restart exception',traceback.format_exc())
finally:
    (out/'restart-tests.json').write_text(json.dumps(dict(checks=checks,failed=sum(not c['pass'] for c in checks)),ensure_ascii=False,indent=2),encoding='utf-8');o.close()
raise SystemExit(any(not c['pass'] for c in checks))
