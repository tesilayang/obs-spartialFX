"""Compare installed builds using the same self-drawn five-instance fixture and fixed seeds."""
import argparse, copy, time
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from v363_probe import Probe, OBS, UI, STYLES, write_json, recipe

def assets(out):
    def image(size):
        im=Image.new('RGBA',size); return im,ImageDraw.Draw(im)
    def save(name,im): im.save(out/(name+'.png'))
    im,d=image((1920,1080)); d.rectangle((0,0,1920,1080),fill='#263849')
    for y in range(1080):
        t=y/1080;d.line((0,y,1920,y),fill=(int(37+33*t),int(56+39*t),int(77+36*t),255))
    d.ellipse((850,-250,1730,630),fill='#617788'); d.polygon([(0,720),(570,335),(970,620),(1540,360),(1920,640),(1920,1080),(0,1080)],fill='#415b67');save('backdrop',im)
    im,d=image((460,700));d.ellipse((46,100,407,512),fill='#283947');d.polygon([(43,700),(69,539),(175,455),(289,455),(391,540),(448,700)],fill='#cc826c');d.polygon([(151,477),(227,627),(305,476)],fill='#ffddb0');d.rounded_rectangle((177,381,285,507),32,fill='#d89a7c');d.ellipse((93,130,361,431),fill='#f1bd98');d.pieslice((75,96,377,364),180,347,fill='#283947');d.polygon([(81,195),(164,116),(321,137),(342,193),(223,239),(249,191),(147,257)],fill='#283947');d.arc((135,278,185,310),187,345,fill='#3f3a42',width=7);d.arc((267,278,317,310),187,345,fill='#3f3a42',width=7);d.arc((202,325,260,364),12,155,fill='#925a5c',width=5);save('actor',im)
    im,d=image((1220,120));d.rounded_rectangle((0,5,1220,120),27,fill='#223849');d.line((23,12,1196,12),fill='#c4a28b',width=7);save('desk',im)
    im,d=image((540,135));font=ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf',58);d.text((1,0),'Night Studio',font=font,fill='#ffebcc');d.text((5,85),'ON AIR    /    SESSION 04',font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',25),fill='#c1d6d6');save('title',im)

def fixture(p):
    stamp=str(time.time_ns());p.scene='366 studio '+stamp;p.stage='366 stage '+stamp;p.o.call('CreateScene',dict(sceneName=p.scene));assets(p.out);layers=[];sources={}
    for name,asset,x,y,z,s in [('Backdrop','backdrop',0,0,420,1.32),('Host','actor',75,86,0,1),('Desk','desk',0,395,-140,1),('Title','title',-485,-225,-82,1),('Echo','actor',470,92,205,.48)]:
        if asset not in sources:
            source='366 '+asset+stamp;p.o.call('CreateInput',dict(sceneName=p.scene,inputName=source,inputKind='image_source',inputSettings={'file':str(p.out/(asset+'.png'))},sceneItemEnabled=False));sources[asset]=next(i['inputUuid'] for i in p.o.call('GetInputList')['inputs'] if i['inputName']==source)
        w,h=Image.open(p.out/(asset+'.png')).size;layers.append(dict(uuid=sources[asset],label="366 "+asset+stamp,instance_id=name,x=x,y=y,z=z,sx=s,sy=s,enabled=True,visible=True,has_default=True,import_width=w,import_height=h))
    p.base=dict(schema_version=6,width=1920,height=1080,camera_z=-1500,camera_fov=40,playing=False,loop=False,timeline_preview=True,fx_enabled=False,light_enabled=False,layers=layers)
    p.o.call('CreateInput',dict(sceneName=p.scene,inputName=p.stage,inputKind='obs_spatial_fx_stage',inputSettings=p.base,sceneItemEnabled=True));p.o.call('SetCurrentProgramScene',dict(sceneName=p.scene));time.sleep(.7);p.n=UI(p.bridge,p.stage);p.refresh();p.base=p.n.get();p.base.pop('_metrics',None);p.put(p.base);p.shot('clean');write_json(p.out/'base.json',p.base);write_json(p.out/'session.json',dict(scene=p.scene,stage=p.stage))

def compare(p,style_ids=STYLES):
    n=p.n;controls=n.inspect();print(controls['spatial.version']['text'],flush=True)
    if not controls['spatial.preset-details-toggle361']['checked']:n.click('spatial.preset-details-toggle361')
    for style in style_ids:
        for seed in ((17,) if getattr(p.args,"rich",False) else (17,73,419)):
            p.cancel();p.put(p.base);p.refresh();n.command('ui-list',widget='spatial.preset-targets361',ids=['Host']);n.click('spatial.preset-style-'+style);n.command('ui-combo',widget='spatial.preset-amount361',value=2 if getattr(p.args,'rich',False) else 1);n.combo('spatial.preset-motion-part361','none');n.combo('spatial.preset-mask-part361','none');n.combo('spatial.preset-decor-part361','');n.number('spatial.preset-period36',6);n.number('spatial.preset-seed361',seed);n.flag('spatial.preset-loop36',False);model=p.generate();assert len(model['presets_instances'])==1,(style,seed,n.inspect());p.cancel();write_json(p.out/f'{style}-{seed}.json',model)
            for name,sec in [('entry',.55),('mid',3),('exit',5.48)]:p.put(model,sec);p.shot(f'{style}-{seed}-{name}',768,432)
            print(style,seed,len(model['presets_geometries']),flush=True)
    p.put(p.base);p.refresh()

if __name__=='__main__':
    a=argparse.ArgumentParser();a.add_argument('bridge',type=Path);a.add_argument('out',type=Path);a.add_argument('--style',nargs='+');a.add_argument('--rich',action='store_true');a=a.parse_args();p=Probe(a);p.o=OBS();fixture(p);compare(p,a.style or STYLES)
