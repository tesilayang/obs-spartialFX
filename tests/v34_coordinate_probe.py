"""Real GPU coordinate regressions using the broad probe's synthetic scene."""
import base64,copy,io,json,math,sys,time
from pathlib import Path
from PIL import Image,ImageChops,ImageStat
from obs_client import OBS
from native_control import Native
root=Path(sys.argv[1]).resolve();bridge=Path(sys.argv[2]);out=root/'coordinates';out.mkdir(exist_ok=True)
report=json.loads((root/'render-tests.json').read_text(encoding='utf-8'))
catalog=json.loads(Path('build/fx-runtime-catalog.json').read_text(encoding='utf-8'))['effects']
o=OBS();n=Native(bridge,report['stage']);saved=n.get();checks=[]
layer=copy.deepcopy(saved['layers'][0]);layer.pop('fx_stack',None)
actor=next(x['inputName'] for x in o.call('GetInputList')['inputs'] if x['inputUuid']==layer['uuid'])
def check(ok,label,value=None):
 checks.append({'name':label,'pass':bool(ok),'value':value});print(('PASS ' if ok else 'FAIL ')+label+' '+str(value),flush=True)
def shot(name):
 data=o.call('GetSourceScreenshot',{'sourceName':report['stage'],'imageFormat':'png','imageWidth':640,'imageHeight':360})
 p=out/(name+'.png');p.write_bytes(base64.b64decode(data['imageData'].split(',',1)[1]));return Image.open(p).convert('RGBA')
def diff(a,b):return sum(ImageStat.Stat(ImageChops.difference(a,b)).mean)
def update(p,wait=.12):n.update(p);time.sleep(wait)
def fx(identifier,params={}):
 d=next(x for x in catalog if x['id']==identifier);assert set(params)<=set(d['parameters'])
 return {'attached':True,'group_states':[{'enabled':i==d['group']-1} for i in range(13)],'effects':[{'id':identifier,'enabled':True,'strength':1,'parameters':dict(d['parameters'],**params)}]}
def imagefile(path):o.call('SetInputSettings',{'inputName':actor,'inputSettings':{'file':str(path)}});time.sleep(.2)
try:
 imagefile(root/'fixture.png')
 update({'layers':[layer],'animation':[],'timeline_preview':False,'camera_x':0,'camera_y':0,'camera_z':-1500,'camera_rx':0,'camera_ry':0,'camera_rz':0})
 original=shot('original');update({'layers':[dict(layer,fx_stack=fx('FX03.8'))]});inverse=shot('negative')
 a,b=original.getpixel((65,90)),inverse.getpixel((65,90));check(a[3]==255 and b[3]==255 and max(abs(a[i]+b[i]-255) for i in range(3))<=2,'selected child performs actual RGB inversion',{'original':a,'inverse':b})
 white=out/'white.png';Image.new('RGBA',(1920,1080),'white').save(white);imagefile(white)
 update({'layers':[dict(layer,fx_stack=fx('FX04.4',{'roundness':0,'feather':.2,'darkness':.75,'scale':1}))]});rect=shot('vignette-rectangle')
 update({'layers':[dict(layer,fx_stack=fx('FX04.4',{'roundness':1,'feather':.2,'darkness':.75,'scale':1}))]});ellipse=shot('vignette-canvas-ellipse')
 check(rect.getpixel((576,324))[0]>245 and ellipse.getpixel((576,324))[0]<80 and ellipse.getpixel((320,180))[0]>245,'roundness0 rectangle and1 canvas ellipse have correct corner falloff',{'rectangle':rect.getpixel((576,324)),'ellipse':ellipse.getpixel((576,324))})
 grey=out/'grey.png';Image.new('RGBA',(1920,1080),(65,65,65,255)).save(grey);imagefile(grey)
 mat=fx('FX05.4',{'roughness':.6,'gain':2});lit=dict(layer,fx_stack=mat)
 update({'layers':[lit],'light_x':-300,'light_y':-180,'light_z':-400});front=shot('material-front')
 a=math.radians(20);rot=lambda x,z:(x*math.cos(a)+z*math.sin(a),-x*math.sin(a)+z*math.cos(a))
 cx,cz=rot(0,-1500);lx,lz=rot(-300,-400)
 update({'layers':[dict(lit,ry=20)],'camera_x':cx,'camera_z':cz,'camera_ry':20,'light_x':lx,'light_z':lz});rotated=shot('material-world-rotation')
 check(diff(front,rotated)<.1,'rotating camera layer and lamp together preserves material pixels',diff(front,rotated))
 update({'layers':[dict(lit,crop_left=480)],'camera_x':0,'camera_z':-1500,'camera_ry':0,'light_x':-300,'light_z':-400});cropped=shot('material-asymmetric-crop')
 small=out/'grey-cropped.png';Image.new('RGBA',(1440,1080),(65,65,65,255)).save(small);imagefile(small)
 update({'layers':[lit]});natural=shot('material-equivalent-source')
 check(diff(cropped,natural)<.15,'asymmetric crop matches equivalent source in shared world coordinates',diff(cropped,natural))
 imagefile(root/'fixture.png')
 # Constant-speed keyframes: blur must remain horizontal after a90degree layer rotation.
 def track():return {'parameter':'x','enabled':True,'keys':[{'time_us':'0','value':-300,'mode':0},{'time_us':'2000000','value':300,'mode':0}]}
 motion=fx('FX01.6',{'shutter':3,'bias':.5})
 for scale,angle in ((.45,0),(.7,90)):
  moving=dict(layer,sx=scale,sy=scale,rz=angle,animation=[track()])
  update({'layers':[moving],'timeline_preview':True,'playing':False,'timeline_position_us':'1000000'});sharp=shot(f'motion-sharp-{angle}')
  update({'layers':[dict(moving,fx_stack=motion)],'playing':True,'loop':False,'timeline_position_us':'200000'},.35);blurred=shot(f'motion-playing-{angle}')
  state=n.get();at=state['timeline_position_us'];state['layers'][0].pop('fx_stack',None)
  update({'layers':state['layers'],'playing':False,'timeline_preview':True,'timeline_position_us':at});static=shot(f'motion-static-{angle}')
  check(blurred.getchannel('A').getextrema()[1]>0 and diff(blurred,static)>.01,f'known layer motion produces GPU blur at scale{scale} rotation{angle}',diff(blurred,static))
  update({'layers':[dict(moving,fx_stack=motion)],'playing':False,'timeline_preview':False});stopped=shot(f'motion-stopped-{angle}')
  update({'layers':[moving]});reference=shot(f'motion-stopped-reference-{angle}')
  check(diff(stopped,reference)<.01,f'stationary layer blur returns identity at rotation{angle}',diff(stopped,reference))
finally:
 imagefile(root/'fixture.png');n.update(saved);o.close()
 (out/'coordinate-tests.json').write_text(json.dumps({'checks':checks,'failed':sum(not c['pass'] for c in checks)},indent=2),encoding='utf-8')
raise SystemExit(any(not c['pass'] for c in checks))
