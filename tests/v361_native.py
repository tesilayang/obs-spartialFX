"""Read/write QA bridge messages for real native Qt controls in isolated OBS."""
import json, os, time, uuid
from pathlib import Path
from native_control import Native

class UI(Native):
    def command(self,action,**kwargs):
        token=uuid.uuid4().hex
        payload=dict(id=token,source=self.source,action=action,**kwargs)
        tmp=self.out/'native-command.tmp'
        tmp.write_text(json.dumps(payload,ensure_ascii=False),encoding='utf-8')
        for attempt in range(100):
            try:
                os.replace(tmp,self.out/'native-command.json');break
            except PermissionError:
                if attempt==99:raise
                time.sleep(.02)
        deadline=time.monotonic()+20
        while time.monotonic()<deadline:
            try:
                data=json.loads((self.out/'native-answer.json').read_text(encoding='utf-8-sig'))
                if data.get('id')==token:
                    if not data.get('ok'):raise RuntimeError('Native Qt operation failed: '+repr(payload))
                    time.sleep(.07)
                    return data
            except (FileNotFoundError,PermissionError,json.JSONDecodeError):pass
            time.sleep(.025)
        raise TimeoutError('Native Qt control bridge timed out: '+action)

    def inspect(self,screenshot=None):
        args={} if screenshot is None else dict(screenshot=screenshot)
        return {c['name']:c for c in self.command('ui-inspect',**args)['ui']['controls']}

    def click(self,name,**kw):return self.command('ui-click',widget=name,**kw)
    def combo(self,name,data):return self.command('ui-combo',widget=name,data=data)
    def number(self,name,value):return self.command('ui-number',widget=name,value=value)
    def flag(self,name,value):return self.command('ui-check',widget=name,value=value)
