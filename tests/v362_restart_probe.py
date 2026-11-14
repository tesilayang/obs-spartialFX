"""Persisted light state and rendered output across normal OBS process restart."""
import base64,io,json,sys,time
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
from obs_client import OBS
from v361_native import UI
out,bridge,previous=map(lambda v:Path(v).resolve(),sys.argv[1:4]);out.mkdir(parents=True,exist_ok=True)
info=json.loads((previous/'restart-scene.json').read_text(encoding='utf-8'))
expected=json.loads((previous/'restart-expected.json').read_text(encoding='utf-8'));checks=[]
def check(ok,name):
    checks.append(dict(name=name,**{'pass':bool(ok)}));print(('PASS ' if ok else 'FAIL ')+name,flush=True)
o=OBS();n=UI(bridge,info['stage'])
try:
    check(o.call('GetVersion')['obsVersion']=='33.0.1','acceptance OBS version33.0.1')
    o.call('SetCurrentProgramScene',dict(sceneName=info['scene']));time.sleep(.7);n.command('ui-refresh');n.command('ui-tab',value=1);state=n.get()
    for key in ('layers','extra_lights','light_x','light_y','light_z','light_softness','shadow_red','shadow_opacity','shadows_enabled','fx_enabled'):
        check(state[key]==expected[key],key+' exactly retained after restart')
    controls=n.inspect('lights-restart.png');check('3.6.2' in controls['spatial.version']['text'],'fresh installed Dock version3.6.2')
    data=o.call('GetSourceScreenshot',dict(sourceName=info['stage'],imageFormat='png',imageWidth=960,imageHeight=540))
    raw=base64.b64decode(data['imageData'].split(',',1)[1]);(out/'shadow-restart.png').write_bytes(raw)
    if (previous/'shadow-persisted.png').exists():
        a=Image.open(io.BytesIO(raw)).convert('RGBA');b=Image.open(previous/'shadow-persisted.png').convert('RGBA')
        check(sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)<2,'saved shadow output stable across restart')
    check(state['_metrics']['failures']==0,'restarted actual renderer reports no failures')
finally:o.close()
(out/'restart-tests.json').write_text(json.dumps(dict(checks=checks,failed=sum(not c['pass'] for c in checks)),ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(any(not c['pass'] for c in checks))
