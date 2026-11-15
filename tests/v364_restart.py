"""Verify persisted animation state and pixels after normal OBS restart."""
import sys,json,base64,io
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
from obs_client import OBS
from native_control import Native
bridge,fixture,out=map(Path,sys.argv[1:4]);out.mkdir(parents=True,exist_ok=True)
info=json.loads((fixture/'restart-scene.json').read_text(encoding='utf-8'));expected=json.loads((fixture/'restart-expected.json').read_text(encoding='utf-8'));o=OBS();actual=Native(bridge,info['stage']).get();checks=[]
def check(ok,name,value=None):
 checks.append(dict(status='PASS' if ok else 'FAIL',name=name,value=value));print(checks[-1],flush=True)
def animation(m):return dict(light=m['light_animation'],layers=[dict(id=l['instance_id'],animation=l['animation'],fx=l['fx_stack']) for l in m['layers']])
check(animation(actual)==animation(expected),'restart restores layer light FX tracks and stable identities')
frame=o.call('GetSourceScreenshot',dict(sourceName=info['stage'],imageFormat='png',imageWidth=960,imageHeight=540));raw=base64.b64decode(frame['imageData'].split(',',1)[1]);(out/'restarted-frame.png').write_bytes(raw)
a=Image.open(io.BytesIO(raw)).convert('RGBA');b=Image.open(fixture/'restart-expected-frame.png').convert('RGBA');delta=sum(ImageStat.Stat(ImageChops.difference(a,b)).mean);check(delta<.01,'restart restores evaluated GPU pixels',delta);o.close()
(out/'results.json').write_text(json.dumps(checks,indent=2));sys.exit(any(c['status']=='FAIL' for c in checks))
