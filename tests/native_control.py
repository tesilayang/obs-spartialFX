"""QA-only bridge to production owned-payload proc API; never packaged as runtime control."""
import json,pathlib,time,uuid,os
class Native:
 def __init__(self,out,source):self.out=pathlib.Path(out);self.source=source;self.current=None
 def request(self,settings=None):
  token=uuid.uuid4().hex;p=self.out/'native-command.tmp';p.write_text(json.dumps({'id':token,'source':self.source,'settings':settings}),encoding='utf-8')
  for attempt in range(100):
   try:os.replace(p,self.out/'native-command.json');break
   except PermissionError:
    if attempt==99:raise
    time.sleep(.02)
  deadline=time.monotonic()+15
  while time.monotonic()<deadline:
   try:
    d=json.loads((self.out/'native-answer.json').read_text(encoding='utf-8-sig'))
    if d.get('id')==token:
     if not d.get('ok'):raise RuntimeError('native state request failed')
     return d.get('state',{})
   except (FileNotFoundError,PermissionError,json.JSONDecodeError):pass
   time.sleep(.03)
  raise TimeoutError('native QA bridge did not answer')
 def get(self):self.current=self.request();return dict(self.current)
 def update(self,patch):
  if self.current is None:self.get()
  self.current.update(patch);self.request(self.current);time.sleep(.06)

class LegacyNative(Native):
 """Keep historical render tests on their documented schema3 inputs, exercising migration."""
 def get(self):
  state=super().get()
  if state.get('use_legacy_playback_range') and state.get('legacy_playback_end',-1)>=0:
   self.current['animation_duration']=state['legacy_playback_end']
  return dict(self.current)
 def update(self,patch):
  if self.current is None:self.get()
  self.current.update(patch);self.current['schema_version']=3
  self.current.pop('animation',None);self.current.pop('timeline_position_us',None)
  for layer in self.current.get('layers',[]):layer.pop('animation',None)
  self.request(self.current);time.sleep(.06)
