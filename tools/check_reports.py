import json,pathlib
for p in pathlib.Path('logs').glob('phase*/dock-smoke.json'):
 d=json.loads(p.read_text(encoding='utf-8'));print(p,'failed=',d['failed'],'checks=',len(d['checks']),'stats=',d.get('performanceLabel'));print([x for x in d['checks'] if not x['pass']])
p=pathlib.Path('tests/obs_qa_driver.cpp');s=p.read_text(encoding='utf-8');s=s.replace('auto*main=mainWidget();if(main)main->resize(1460,920);auto*p=panel();','auto*main=mainWidget();if(main)main->resize(1460,920);if(scenario=="baseline"){QTimer::singleShot(3000,qApp,[]{mainWidget()->close();});return;}auto*p=panel();');p.write_text(s,encoding='utf-8')
