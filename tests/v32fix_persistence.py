"""Capture real private schema4 state, close QA OBS, then compare after restart."""
from pathlib import Path
import json,sys,time,uuid,os
from native_control import Native
out=Path(sys.argv[2]).resolve();mode=sys.argv[1]
if mode=='capture':
 fixture=json.loads((out/'camera-render-tests.json').read_text(encoding='utf-8'))
 n=Native(out,fixture['stage']);n.update({'playing':False,'auto_key':True,'key_positions_locked':True,'timeline_position_us':'1125000','animation':[{'parameter':'x','enabled':True,'shortest':False,'keys':[{'time_us':'0','value':-120,'mode':7,'out_x':.25,'out_y':-20,'out_absolute':True},{'time_us':'2000000','value':120,'mode':0,'in_x':.75,'in_y':40,'in_absolute':True}]}]})
 time.sleep(1);state=n.get();(out.parent/'persistence-before.json').write_text(json.dumps(state,indent=2),encoding='utf-8')
 temp=out/'native-command.tmp';temp.write_text(json.dumps({'id':uuid.uuid4().hex,'action':'finish'}),encoding='utf-8');os.replace(temp,out/'native-command.json')
elif mode=='compare':
 before=json.loads((out/'persistence-before.json').read_text(encoding='utf-8'))
 after=json.loads((out/'persist33-final/dock-smoke.json').read_text(encoding='utf-8'))['stageSettings']
 differences=[key for key in set(before)|set(after) if before.get(key)!=after.get(key)]
 report={'passed':not differences,'differences':sorted(differences),'fieldCount':len(before),'autoKey':after.get('auto_key'),'positionLock':after.get('key_positions_locked'),'bezierKeys':after.get('animation')}
 (out/'persistence-compare.json').write_text(json.dumps(report,indent=2),encoding='utf-8');print(json.dumps(report,indent=2));raise SystemExit(not report['passed'])
else:raise SystemExit('capture or compare required')
