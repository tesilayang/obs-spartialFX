from pathlib import Path
import re
for f in ['src/dock.cpp','src/dock_actions.cpp']:
 p=Path(f);p.write_text(re.sub(r'\bdata\b','settingsData',p.read_text(encoding='utf-8')),encoding='utf-8')
p=Path('src/stage.cpp');s=p.read_text(encoding='utf-8').replace('std::unordered_set<std::string> activated;','std::unordered_set<std::string> activated,rejected;')
s=s.replace('if(allowed&&layer.visible&&!oldIds.count(layer.sourceUuid)&&activated.insert(layer.sourceUuid).second)allowed=obs_source_add_active_child(s.owner,source);','''if(allowed&&layer.visible&&!oldIds.count(layer.sourceUuid)){
                if(rejected.count(layer.sourceUuid))allowed=false;
                else if(!activated.count(layer.sourceUuid)){
                    allowed=obs_source_add_active_child(s.owner,source);
                    (allowed?activated:rejected).insert(layer.sourceUuid);
                }
            }''')
p.write_text(s,encoding='utf-8')
p=Path('tests/obs_qa_driver.cpp');s=p.read_text(encoding='utf-8').replace('QTimer::singleShot(15000,qApp,finish);','QTimer::singleShot(scenario=="phase4"?25000:15000,qApp,finish);')
s=s.replace('if(scenario=="phase1"){finish();return;}','if(scenario=="baseline"){QTimer::singleShot(3000,qApp,[]{mainWidget()->close();});return;}\n    if(scenario=="phase1"){finish();return;}')
p.write_text(s,encoding='utf-8')
