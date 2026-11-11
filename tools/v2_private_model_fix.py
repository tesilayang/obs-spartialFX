from pathlib import Path
p=Path('src/stage.hpp');s=p.read_text(encoding='utf-8-sig').replace('void stageApplySettings','// UI-thread API: return a detached authoritative model settings copy.\nobs_data_t *stageGetSettings(obs_source_t *source);\nvoid stageApplySettings');p.write_text(s,encoding='utf-8')
p=Path('src/stage.cpp');s=p.read_text(encoding='utf-8-sig')
s=s.replace('                applySettings(stage,settings);obs_data_release(settings);','                stageApplySettings(source,settings);obs_data_release(settings);')
s=s.replace('void *create(obs_data_t *settings,obs_source_t *source){auto *s=new Stage;s->owner=source;applySettings(s,settings);return s;}', '''void loaded(void *data,obs_data_t*){
    auto &s=*static_cast<Stage*>(data);auto *privateSettings=obs_source_get_private_settings(s.owner);auto *saved=obs_data_get_obj(privateSettings,"spatial_fx_model");
    if(saved){applySettings(&s,saved);obs_data_release(saved);}obs_data_release(privateSettings);
}
void *create(obs_data_t *settings,obs_source_t *source){auto *s=new Stage;s->owner=source;applySettings(s,settings);auto *privateSettings=obs_source_get_private_settings(source);obs_data_set_obj(privateSettings,"spatial_fx_model",settings);obs_data_release(privateSettings);return s;}''')
start=s.index('void stageApplySettings(');end=s.index('StageStats stageStats(',start)
s=s[:start]+'''obs_data_t *stageGetSettings(obs_source_t *source){
    auto *result=obs_data_create();if(!source)return result;
    auto *privateSettings=obs_source_get_private_settings(source);auto *saved=obs_data_get_obj(privateSettings,"spatial_fx_model");
    auto *settings=saved?saved:obs_source_get_settings(source);obs_data_apply(result,settings);obs_data_release(settings);obs_data_release(privateSettings);return result;
}
void stageApplySettings(obs_source_t *source,obs_data_t *settings){
    if(!source||obs_source_removed(source)||std::string(obs_source_get_unversioned_id(source))!=stageId)return;
    auto *stage=static_cast<Stage*>(obs_obj_get_data(source));if(!stage)return;
    // Native edits never mutate public context.settings. OBS/WebSocket can traverse
    // that object on worker/video threads without taking the plugin's UI lock.
    // The private model is saved by OBS frontend on UI, then rendered through RCU.
    auto *privateSettings=obs_source_get_private_settings(source);auto *copy=obs_data_create();obs_data_apply(copy,settings);
    obs_data_set_obj(privateSettings,"spatial_fx_model",copy);applySettings(stage,copy);obs_data_release(copy);obs_data_release(privateSettings);
}
'''+s[end:]
s=s.replace('info.update=spatial::update;','info.update=spatial::update;info.load=spatial::loaded;');p.write_text(s,encoding='utf-8')
p=Path('src/dock.cpp');s=p.read_text(encoding='utf-8-sig').replace('auto*settingsData=obs_source_get_settings(source);const bool upgrade=', 'auto*settingsData=stageGetSettings(source);const bool upgrade=');p.write_text(s,encoding='utf-8')
p=Path('tests/obs_qa_driver.cpp');s=p.read_text(encoding='utf-8-sig');needle='void click(const char*name)';helper='''obs_data_t* currentSettings(obs_source_t*source){if(!source)return nullptr;auto*p=obs_source_get_private_settings(source);auto*d=obs_data_get_obj(p,"spatial_fx_model");obs_data_release(p);return d?d:obs_source_get_settings(source);}
''';s=s.replace(needle,helper+needle)
for source in ['source','stage','originalStage']:
 s=s.replace('obs_source_get_settings('+source+')','currentSettings('+source+')')
# Keep public fallback inside helper, not recursive.
s=s.replace('return d?d:currentSettings(source);','return d?d:obs_source_get_settings(source);');p.write_text(s,encoding='utf-8')
p=Path('tests/v2_qa.inc');s=p.read_text(encoding='utf-8-sig').replace('obs_source_get_settings(stage)','currentSettings(stage)');p.write_text(s,encoding='utf-8')
p=Path('tests/v2_stress_probe.py');s=p.read_text(encoding='utf-8-sig').replace('finally:o.close()', 'finally:\n try:o.close()\n except OSError:pass');p.write_text(s,encoding='utf-8')
