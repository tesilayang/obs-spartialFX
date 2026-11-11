import sys,pathlib,time,json
from obs_client import OBS
from native_control import Native
out=pathlib.Path(sys.argv[1]);o=OBS();scene=o.call('GetCurrentProgramScene')['currentProgramSceneName'];stage=next(i['sourceName'] for i in o.call('GetSceneItemList',{'sceneName':scene})['sceneItems'] if i.get('inputKind')=='obs_spatial_fx_stage');c=Native(out,stage);state=c.get();lights=[]
for i in range(3):lights.append({'light_enabled':True,'light_visible':True,'light_type':i%2,'light_label':'QA emitter '+str(i+2),'light_x':-500+i*480,'light_y':-200+i*100,'light_z':-700,'light_intensity':.7,'light_range':3000,'light_ambient':0,'light_size':120,'shadows_enabled':True,'shadow_opacity':.5,'light_softness':15})
c.update({'extra_lights':lights});(out/'performance-workload.json').write_text(json.dumps({'layers':len(state['layers']),'emitters':4,'shadows':True,'animation':True,'source':stage},indent=2));o.close()
