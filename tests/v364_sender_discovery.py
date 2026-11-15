import sys,time,json
from pathlib import Path
sys.path.insert(0,str(Path.cwd()/'tests'))
from obs_client import OBS
o=OBS();scene='Alpha protocol discovery';o.call('CreateScene',dict(sceneName=scene));result={}
try:
 for kind,prop in [('spout_capture','spoutsenders'),('ndi_source','ndi_source_name')]:
  name='QA discovery '+kind;o.call('CreateInput',dict(sceneName=scene,inputName=name,inputKind=kind,inputSettings={},sceneItemEnabled=False))
  try:
   time.sleep(2);items=o.call('GetInputPropertiesListPropertyItems',dict(inputName=name,propertyName=prop))['propertyItems'];result[kind]=[x for x in items if 'vtube' in str(x).lower()]
  finally:o.call('RemoveInput',dict(inputName=name))
finally:o.call('RemoveScene',dict(sceneName=scene))
Path('work/v3.6.4/restart-sender-discovery.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
