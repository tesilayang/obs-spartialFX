"""Real window-capture lifecycle/render regression in isolated OBS 33.0.1."""
import base64, json, sys, time, subprocess
from pathlib import Path
from PIL import Image, ImageStat
from obs_client import OBS
from native_control import Native

root=Path(__file__).resolve().parents[1]
out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True)
o=OBS();checks=[]
def shot(name,label):
    data=o.call('GetSourceScreenshot',dict(sourceName=name,imageFormat='png'))
    p=out/(label+'.png');p.write_bytes(base64.b64decode(data['imageData'].split(',',1)[1]))
    im=Image.open(p).convert('RGBA'); stats=ImageStat.Stat(im)
    return dict(size=im.size,mean=stats.mean,bounds=im.getbbox())
scene='Window regression '+str(time.time_ns())
o.call('CreateScene',dict(sceneName=scene));o.call('SetCurrentProgramScene',dict(sceneName=scene))
stage='Window stage '+str(time.time_ns())
o.call('CreateInput',dict(sceneName=scene,inputName=stage,inputKind='obs_spatial_fx_stage',inputSettings={},sceneItemEnabled=True))
n=Native(out/'bridge',stage)
fixture=subprocess.Popen([str(root/'build/spatial-capture-fixture.exe')])
try:
    time.sleep(2)
    for method in (1,2):
        name='Capture method '+str(method)+' '+str(time.time_ns())
        created=o.call('CreateInput',dict(sceneName=scene,inputName=name,inputKind='window_capture',inputSettings=dict(window='Spatial FX Capture Fixture:SpatialFXCaptureFixture:spatial-capture-fixture.exe',method=method,cursor=False),sceneItemEnabled=True))
        time.sleep(5)
        before=shot(name,f'method{method}-source-visible')
        model=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,fx_enabled=False,playing=False,
            layers=[dict(uuid=created['inputUuid'],scene_uuid=o.call('GetCurrentProgramScene')['currentProgramSceneUuid'],item_id=created['sceneItemId'],original_visible=True,instance_id='capture-test',label='Capture test',visible=True,enabled=True,sx=1,sy=1)])
        n.request(model)
        o.call('SetSceneItemEnabled',dict(sceneName=scene,sceneItemId=created['sceneItemId'],sceneItemEnabled=False));time.sleep(3)
        source=shot(name,f'method{method}-source-hidden');render=shot(stage,f'method{method}-stage')
        active=o.call('GetSourceActive',dict(sourceName=name))
        checks.append(dict(method=method,before=before,source=source,stage=render,active=active,pass_=render['mean'][3]>5))
        (out/'state.json').write_text(json.dumps(n.get(),ensure_ascii=False,indent=2),encoding='utf-8')
        n.request(dict(model,layers=[]));o.call('RemoveInput',dict(inputName=name))
finally:
    # Let the native fixture close itself; never terminate OBS.
    o.close()
(out/'results.json').write_text(json.dumps(checks,indent=2),encoding='utf-8')
print(json.dumps(checks,indent=2))
sys.exit(0 if all(x['pass_'] for x in checks) else 1)
