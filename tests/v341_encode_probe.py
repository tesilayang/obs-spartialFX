"""Real local x264 1080p60 encode exercise, no network streaming."""
import json,math,sys,time,traceback
from pathlib import Path
from obs_client import OBS
from native_control import Native
out=Path(sys.argv[1]).resolve();out.mkdir(parents=True,exist_ok=True);bridge=Path(sys.argv[2]).resolve()
checks=[];samples=[];o=OBS();recording=False
def check(ok,name,value=None):
 checks.append({'name':name,'pass':bool(ok),'value':value});print(('PASS ' if ok else 'FAIL ')+name+' '+str(value or ''),flush=True)
try:
 stamp=str(time.time_ns());scene='V341 Encode '+stamp;stage='V341 Encode Stage '+stamp;name='V341 Encode Image '+stamp
 fixture=Path(sys.argv[3]).resolve();o.call('CreateScene',{'sceneName':scene})
 o.call('CreateInput',{'sceneName':scene,'inputName':name,'inputKind':'image_source','inputSettings':{'file':str(fixture)},'sceneItemEnabled':False})
 uid=next(x['inputUuid'] for x in o.call('GetInputList')['inputs'] if x['inputName']==name)
 scale=1500/(1080/(2*math.tan(math.radians(20))))
 fx={'attached':True,'group_states':[{'enabled':i==1} for i in range(13)],'effects':[{'id':'FX02.2','enabled':True,'parameters':{'quality':0,'threshold':.15}},{'id':'FX02.4','enabled':True,'parameters':{'amount':.25,'denoise':1}}]}
 curve={'parameter':'rz','enabled':True,'keys':[{'time_us':'0','value':-12,'mode':0},{'time_us':'15000000','value':12,'mode':0},{'time_us':'30000000','value':-12,'mode':0}]}
 layer={'uuid':uid,'instance_id':'encode-actor','sx':scale,'sy':scale,'visible':True,'enabled':True,'animation':[curve],'fx_stack':fx}
 settings={'schema_version':5,'width':1920,'height':1080,'camera_z':-1500,'camera_fov':40,'fx_enabled':False,'light_enabled':False,'layers':[layer],'playing':True,'loop':True,'timeline_preview':True,'play_token':int(time.time())}
 o.call('CreateInput',{'sceneName':scene,'inputName':stage,'inputKind':'obs_spatial_fx_stage','inputSettings':settings,'sceneItemEnabled':True})
 o.call('SetCurrentProgramScene',{'sceneName':scene});time.sleep(1);control=Native(bridge,stage)
 for category,name,value in [('Output','Mode','Simple'),('SimpleOutput','FilePath',str(out)),('SimpleOutput','RecFormat2','mkv'),('SimpleOutput','RecQuality','Stream'),('SimpleOutput','StreamEncoder','x264'),('SimpleOutput','Preset','veryfast'),('SimpleOutput','VBitrate','6000'),('SimpleOutput','ABitrate','160'),('SimpleOutput','UseAdvanced','false')]:
  o.call('SetProfileParameter',{'parameterCategory':category,'parameterName':name,'parameterValue':value})
 o.call('StartRecord');recording=True;time.sleep(3);start=o.call('GetStats');warm=control.get()['_metrics']
 check(o.call('GetRecordStatus')['outputActive'],'real OBS recording encoder started')
 for i in range(30):time.sleep(1);samples.append({'stats':o.call('GetStats'),'record':o.call('GetRecordStatus')})
 after=control.get()['_metrics'];stop=o.call('StopRecord');recording=False
 path=Path(stop['outputPath']);previous=-1;stable=0
 for i in range(40):
  size=path.stat().st_size if path.exists() else 0;stable=stable+1 if size==previous and size>0 else 0;previous=size
  if stable>=3:break
  time.sleep(.2)
 header=path.read_bytes()[:16] if path.exists() else b''
 container='MP4' if b'ftyp' in header else ('Matroska' if header.startswith(bytes.fromhex('1a45dfa3')) else 'unknown')
 check(path.is_file() and path.stat().st_size>10000 and container!='unknown','real encoded video has a recognized container',{'path':str(path),'bytes':path.stat().st_size if path.exists() else 0,'container':container})
 check(samples[-1]['record']['outputDuration']>=30000,'encoder stayed active through30second sample',samples[-1]['record'])
 check(after['allocations']==warm['allocations'] and after['failures']==0,'SMAA plus RCAS reuses resources while encoding',{'warm':warm,'after':after})
 last=samples[-1]['stats'];check(last['renderSkippedFrames']==start['renderSkippedFrames'],'no additional render-skipped frames while encoding',{'before':start,'after':last})
 check(last['outputSkippedFrames']==start['outputSkippedFrames'],'no additional encode-skipped frames while encoding')
 check(min(s['stats']['activeFps'] for s in samples)>59,'actual fps stays above59 during encode')
except Exception:check(False,'encode probe exception',traceback.format_exc())
finally:
 if recording:
  try:o.call('StopRecord')
  except Exception:pass
 o.close();report={'checks':checks,'failed':sum(not c['pass'] for c in checks),'samples':samples,'scope':'Local x264 recording at1080p60; no internet streaming; no integrated GPU hardware.'}
 (out/'encode-tests.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
raise SystemExit(bool(report['failed']))
