"""Load the actual v3.6.5 fixture snapshot in the installed new plugin and compare identical stage pixels."""
import argparse,json
from pathlib import Path
from PIL import Image
from v363_probe import Probe,UI,OBS,write_json,graph
from v366_acceptance import delta
p=argparse.ArgumentParser();p.add_argument('bridge',type=Path);p.add_argument('out',type=Path);p.add_argument('baseline',type=Path);a=p.parse_args();q=Probe(a);q.o=OBS();session=json.loads((a.baseline/'session.json').read_text(encoding='utf-8-sig'));q.o.call('SetCurrentProgramScene',dict(sceneName=session['scene']));q.stage=session['stage'];q.n=UI(q.bridge,q.stage)
checks=[]
for style in ('paper','tech','rhythm'):
    old=json.loads((a.baseline/f'{style}-17.json').read_text(encoding='utf-8-sig'));q.put(old,3);frame=q.shot(style,768,432);change=delta(frame,Image.open(a.baseline/f'{style}-17-mid.png'));actual=q.n.get();unchanged=all(actual[k]==old[k] for k in ('presets_geometries','presets_instances','presets_motion_blocks'));checks.append(dict(style=style,pixelDifference=change,savedDataMatches=unchanged,status='PASS' if change<.05 and unchanged else 'FAIL'))
write_json(q.out/'checks.json',checks);print(checks);q.o.close();raise SystemExit(any(x['status']=='FAIL' for x in checks))
