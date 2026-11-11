from pathlib import Path
import json,socket
root=Path('.')
config=root/'runtime/obs32/config/obs-studio/plugin_config/obs-websocket/config.json'
data=json.loads(config.read_text(encoding='utf-8-sig'));data['server_enabled']=False;config.write_text(json.dumps(data,indent=2),encoding='utf-8')
for p in list((root/'docs').glob('*.md'))+[root/'README.md']:
 text=p.read_text(encoding='utf-8-sig')
 assert all(ord(c)>=32 or c in '\n\r\t' for c in text),str(p)
try:
 s=socket.create_connection(('127.0.0.1',4457),timeout=.4);s.close();closed=False
except OSError:closed=True
(root/'logs/v2/test-cleanup.json').write_text(json.dumps({'isolatedWebSocketConfigEnabled':False,'testPort4457Closed':closed,'qaDriverInInstalledPackage':Path('C:/ProgramData/obs-studio/plugins/obs-spatial-fx/bin/64bit/spatial-qa-driver.dll').exists()},indent=2),encoding='utf-8')
print('Test port closed:',closed)
assert closed
