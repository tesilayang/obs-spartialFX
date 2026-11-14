"""Actual GPU unshadowed high-saturation lighting preserves separate grading."""
import base64,copy,io,json,sys,time
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
from obs_client import OBS
from v361_native import UI
out,bridge,previous=map(lambda v:Path(v).resolve(),sys.argv[1:4]);out.mkdir(parents=True,exist_ok=True)
info=json.loads((previous/'restart-scene.json').read_text(encoding='utf-8'));saved=json.loads((previous/'restart-expected.json').read_text(encoding='utf-8'))
o=OBS();n=UI(bridge,info['stage']);checks=[]
def check(ok,name,value):
    checks.append(dict(name=name,**{'pass':bool(ok)},value=value));print(('PASS ' if ok else 'FAIL ')+name+' '+str(value),flush=True)
def shot(name):
    d=o.call('GetSourceScreenshot',dict(sourceName=info['stage'],imageFormat='png',imageWidth=960,imageHeight=540));raw=base64.b64decode(d['imageData'].split(',',1)[1]);(out/(name+'.png')).write_bytes(raw);return Image.open(io.BytesIO(raw)).convert('RGBA')
try:
    o.call('SetCurrentProgramScene',dict(sceneName=info['scene']));time.sleep(.5)
    m=copy.deepcopy(saved);m.update(extra_lights=[],fx_saturation=3,shadows_enabled=False,light_red=1,light_green=0,light_blue=0,light_ambient=.2,light_intensity=.5)
    for name,ambient,intensity in [('fill',.2,0),('direct',0,.5),('combined',.2,.5)]:
        m.update(light_ambient=ambient,light_intensity=intensity);n.request(m);time.sleep(.25);shot(name)
    a=ImageChops.add(Image.open(out/'fill.png').convert('RGBA'),Image.open(out/'direct.png').convert('RGBA'));b=Image.open(out/'combined.png').convert('RGBA');delta=sum(ImageStat.Stat(ImageChops.difference(a,b)).mean[:3])
    check(delta<2,'saturation3 colored lighting retains separately graded fill and direct',delta)
finally:n.request(saved);n.command('ui-save');o.close()
(out/'grade-tests.json').write_text(json.dumps(dict(checks=checks,failed=sum(not c['pass'] for c in checks)),indent=2),encoding='utf-8')
raise SystemExit(any(not c['pass'] for c in checks))
