"""Verify persisted final composition against a fresh installed OBS process."""
import json,sys,time
from pathlib import Path
from obs_client import OBS
from v361_native import UI

out,bridge,previous=map(lambda s:Path(s).resolve(),sys.argv[1:4]);out.mkdir(parents=True,exist_ok=True)
info=json.loads((previous/'restart-scene.json').read_text(encoding='utf-8'))
expected=json.loads((previous/'restart-expected.json').read_text(encoding='utf-8'))
o=OBS();n=UI(bridge,info['stage']);checks=[]
def check(ok,name):checks.append(dict(name=name,**{'pass':bool(ok)}));print(('PASS ' if ok else 'FAIL ')+name,flush=True)
try:
    scenes=o.call('GetSceneList');check(any(s['sceneName']==info['scene'] for s in scenes['scenes']),'scene retained after normal restart')
    o.call('SetCurrentProgramScene',dict(sceneName=info['scene']));time.sleep(.6);n.command('ui-refresh');n.command('ui-tab',value=3)
    m=n.get()
    for key in ('layers','presets_geometries','presets_motion_blocks','presets_instances','animation','loop'):
        check(m[key]==expected[key],key+' exactly retained across process restart')
    check(m['presets_instances'][0]['seed']==expected['presets_instances'][0]['seed'],'seed retained without rerandomizing')
    controls=n.inspect('preset-restart.png');check('3.6.1' in controls['spatial.version']['text'],'loaded Dock reports v3.6.1')
    check('spatial.preset-name' not in controls,'removed old preset UI stays removed after restart')
    check(all(l['uuid'] in {s['inputUuid'] for s in o.call('GetInputList')['inputs']} for l in m['layers']),'all imported real source identities resolve')
finally:o.close()
(out/'restart-tests.json').write_text(json.dumps(dict(checks=checks,failed=sum(not c['pass'] for c in checks)),ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(any(not c['pass'] for c in checks))
