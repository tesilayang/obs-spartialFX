import sys,time,json,pathlib,base64
from PIL import Image,ImageChops,ImageStat
from obs_client import OBS
out=pathlib.Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
for i in range(100):
 try:o=OBS();break
 except OSError:time.sleep(.3)
time.sleep(12)
scene=o.call('GetCurrentProgramScene')['currentProgramSceneName'];items=o.call('GetSceneItemList',{'sceneName':scene})['sceneItems'];stage=next(i['sourceName'] for i in items if i.get('inputKind')=='obs_spatial_fx_stage');window=next(i['sourceName'] for i in items if i.get('inputKind')=='window_capture')
def shot(name,file):
 data=o.call('GetSourceScreenshot',{'sourceName':name,'imageFormat':'png'});p=out/file;p.write_bytes(base64.b64decode(data['imageData'].split(',',1)[1]));return Image.open(p).convert('RGBA')
a=shot(stage,'pending-stage.png');w=shot(window,'ready-window.png');expected=Image.new('RGBA',a.size);resized=w.resize((round(w.width*.6),round(w.height*.7)),Image.Resampling.BILINEAR);expected.alpha_composite(resized,(120,80));expected.save(out/'expected-window-anchor.png')
bbox=a.getbbox();target=expected.getbbox();error=max(abs(x-y) for x,y in zip(bbox,target)) if bbox and target else 999
report={'scene':scene,'actualBounds':bbox,'expectedBounds':target,'maxAnchorErrorPx':error,'checks':[{'name':'pending no-bounds window render matches original anchor within 2px','pass':error<=2}],'failed':int(error>2)}
(out/'pending-anchor-tests.json').write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2),flush=True);o.close();sys.exit(bool(report['failed']))
