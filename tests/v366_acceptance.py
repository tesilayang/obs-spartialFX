"""Installed OBS acceptance for independent targets, depth motion, saved state and performance."""
import argparse, json, time, copy, statistics, sys
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
from v366_styles import fixture
from v363_probe import Probe,OBS,UI,write_json,recipe,graph,ref_id,existing_animation_preserved

def delta(a,b):return sum(ImageStat.Stat(ImageChops.difference(a.convert('RGB'),b.convert('RGB'))).mean)
def main(a):
    p=Probe(a);p.o=OBS();checks=[]
    def check(title,result,details=None):
        checks.append(dict(name=title,status='PASS' if result else 'FAIL',details=details));write_json(p.out/'checks.json',checks);print(checks[-1],flush=True)
    if a.restart:
        session=json.loads((a.restart/'session.json').read_text(encoding='utf-8-sig'));p.stage=session['stage'];p.o.call('SetCurrentProgramScene',{'sceneName':session['scene']});time.sleep(.5);p.n=UI(p.bridge,p.stage);p.refresh();m=p.n.get();expected=json.loads((a.restart/'expected.json').read_text(encoding='utf-8-sig'));check('saved graph reopens',graph(m)==graph(expected));p.put(m,3);check('saved frame reopens',delta(p.shot('reopened'),Image.open(a.restart/'saved.png'))<.02);return checks
    fixture(p);n=p.n
    if not n.inspect()['spatial.preset-details-toggle361']['checked']:n.click('spatial.preset-details-toggle361')
    def select(m,t):
        for column,ids in enumerate((m,t),1):n.command('ui-list',widget='spatial.preset-targets361',column=column,ids=ids)
    def setup(m,t,move,mask,amount):
        p.cancel();p.put(p.base);p.refresh();p.controls('paper',amount,move,'',loop=False);n.combo('spatial.preset-mask-part361',mask);select(m,t)
    def recipients(m):return [ref_id(i) for g in m['presets_geometries'] if g['mode']==1 for i in g['receivers']]
    def moving(m):return {ref_id(t['target']) for t in m['presets_instances'][-1]['generated_tracks']}
    setup([],['Title'],'none','T01',0);mask=p.generate();check('mask-only targets',recipients(mask)==['Title'] and not moving(mask));p.cancel();p.put(mask,.35);masked=p.shot('mask-title');p.put(p.base,.35);clean=p.shot('clean-stage');difference=ImageChops.difference(masked.convert('RGB'),clean.convert('RGB')).getbbox();check('mask changes only title region',bool(difference) and difference[2]<550 and difference[3]<250,difference)
    setup(['Host'],[],'M02','none',0);motion=p.generate();check('only selected duplicate moves',moving(motion)=={'Host'} and not recipients(motion));p.cancel();
    # Measure the standard dolly with physical headroom. The multi-depth fixture also exercises near neighbors.
    spaced=copy.deepcopy(p.base)
    for layer in spaced['layers']:
        if layer['instance_id'] in ('Title','Desk'):layer['z']=-450
    p.put(spaced);p.refresh();select(['Host'],[]);standard=p.generate();p.cancel();widths=[]
    for sec in (0,3,6):
        solo=copy.deepcopy(standard)
        for l in solo['layers']:l['visible']=l['instance_id']=='Host'
        p.put(solo,sec);im=p.shot('depth-'+str(sec));box=im.convert('RGB').getbbox();widths.append(box[2]-box[0] if box else 0)
    check('projected width at start midpoint end',0.05<=(widths[1]/max(1,widths[0])-1)<=.12 and abs(widths[2]-widths[0])<=1,widths)
    setup(['Host'],['Title'],'M02','T02',1);original=n.get();authoritative=n.command('ui-inspect')['ui']['authoritative'];write_json(p.out/'before-preview.json',original);combo=p.generate();check('mixed selection independent',moving(combo)=={'Host'} and recipients(combo)==['Title']);q=recipe(combo);before=[t for t in combo['presets_instances'][-1]['generated_tracks']];n.click('spatial.preset-reroll-decor361');time.sleep(.25);other=n.get();check('reroll decoration retains motion tracks and mask',before==other['presets_instances'][-1]['generated_tracks'] and recipients(other)==['Title'] and recipe(other)['mask_seed']==q['mask_seed']);p.cancel();time.sleep(.20);restored=n.get();write_json(p.out/'after-cancel.json',restored);check('cancel restores base',graph(restored)==graph(authoritative) and graph(restored)==graph(original),[key for key in graph(original) if graph(original)[key]!=graph(restored)[key]]);n.click('spatial.preset-generate361');time.sleep(.22);n.click('spatial.preset-apply36');applied=n.get();check('apply writes editable keys',len(applied['presets_instances'])==1 and bool(moving(applied)));p.refresh();n.click('spatial.preset-generate361');time.sleep(.28);n.click('spatial.preset-append363');appended=n.get();check('append adds after previous keys',len(appended['presets_instances'])==2 and existing_animation_preserved(applied,appended) and int(appended['presets_instances'][-1]['start_us'])>=6000000);n.command('ui-tab',value=0);n.click('spatial.undo32');check('undo restores applied graph',graph(n.get())==graph(applied));n.click('spatial.redo32');check('redo restores append',graph(n.get())==graph(appended))
    perf=[]; video=p.o.call('GetVideoSettings');check('1080p60',video['baseWidth']==1920 and video['baseHeight']==1080 and video['fpsNumerator']==60*video['fpsDenominator'],video)
    for name,m in ([] if a.skip_perf else [('off',p.base),('motion',motion),('all',combo)]):
        state=copy.deepcopy(m);state['loop']=True;p.put(state,0,True);time.sleep(1.5);s=p.o.call('GetStats');samples=[]
        for i in range(30):time.sleep(1);samples.append(p.o.call('GetStats'))
        row=dict(name=name,seconds=30,meanRenderMs=statistics.mean(v['averageFrameRenderTime'] for v in samples),meanCPU=statistics.mean(v['cpuUsage'] for v in samples),meanFPS=statistics.mean(v['activeFps'] for v in samples),skipped=samples[-1]['renderSkippedFrames']-s['renderSkippedFrames'],frames=samples[-1]['renderTotalFrames']-s['renderTotalFrames'],memoryDeltaMiB=samples[-1]['memoryUsage']-s['memoryUsage'],entities=len(m['presets_geometries']));perf.append(row);write_json(p.out/'performance.json',perf);check('1080p60 '+name,row['skipped']==0 and row['meanFPS']>59,row)
    p.put(appended,3);p.refresh();n.command('ui-save');p.shot('saved');write_json(p.out/'expected.json',n.get());p.o.close();return checks
if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('bridge',type=Path);ap.add_argument('out',type=Path);ap.add_argument('--restart',type=Path);ap.add_argument('--skip-perf',action='store_true');sys.exit(any(r['status']!='PASS' for r in main(ap.parse_args())))
