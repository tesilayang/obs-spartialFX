from pathlib import Path
p=Path('src/stage.cpp');s=p.read_text(encoding='utf-8-sig');start=s.index('void update(void *data,obs_data_t*)');end=s.index('void enumerate(',start)
s=s[:start]+'''void update(void *data,obs_data_t*){
    auto &s=*static_cast<Stage*>(data);
    // OBS mutates public settings before calling this on the video thread.
    // Do not read that shared hash/arrays. Native edits and owned proc requests
    // use the private UI model; generic settings are creation/legacy data only.
    if(!s.uiUpdateQueued.exchange(true))blog(LOG_WARNING,"[SpatialFX] generic live settings update ignored; use native Dock or spatial_fx_set_state proc API");
}
void procSetState(void *data,calldata_t *request){
    auto &s=*static_cast<Stage*>(data);auto *settings=static_cast<obs_data_t*>(calldata_ptr(request,"settings"));if(!settings)return;
    struct Pending{obs_weak_source_t *weak;obs_data_t *settings;};
    auto *copy=obs_data_create();obs_data_apply(copy,settings);auto *pending=new Pending{obs_source_get_weak_source(s.owner),copy};
    obs_queue_task(OBS_TASK_UI,[](void *opaque){auto *p=static_cast<Pending*>(opaque);auto *source=obs_weak_source_get_source(p->weak);if(source&&!obs_source_removed(source))stageApplySettings(source,p->settings);obs_source_release(source);obs_weak_source_release(p->weak);obs_data_release(p->settings);delete p;},pending,false);
}
void procGetState(void *data,calldata_t *request){auto &s=*static_cast<Stage*>(data);calldata_set_ptr(request,"settings",stageGetSettings(s.owner));}
'''+s[end:]
start=s.index('void loaded(void *data,obs_data_t*)');end=s.index('\n}\nobs_data_t *stageGetSettings',start)
s=s[:start]+'''void loaded(void *data,obs_data_t*){
    auto &s=*static_cast<Stage*>(data);auto *weak=obs_source_get_weak_source(s.owner);
    obs_queue_task(OBS_TASK_UI,[](void *opaque){auto *weak=static_cast<obs_weak_source_t*>(opaque);auto *source=obs_weak_source_get_source(weak);
        if(source&&!obs_source_removed(source)){
            auto *privateSettings=obs_source_get_private_settings(source);auto *saved=obs_data_get_obj(privateSettings,"spatial_fx_model");
            auto *upgraded=saved?modelData(readModel(saved)):stageGetSettings(source);stageApplySettings(source,upgraded);
            obs_data_release(upgraded);obs_data_release(saved);obs_data_release(privateSettings);
            blog(LOG_INFO,"[SpatialFX] private scene model restored / legacy schema migrated");
        }obs_source_release(source);obs_weak_source_release(weak);
    },weak,false);
}
void *create(obs_data_t *settings,obs_source_t *source){
    auto *s=new Stage;s->owner=source;applySettings(s,settings);auto *privateSettings=obs_source_get_private_settings(source);auto *copy=modelData(readModel(settings));obs_data_set_obj(privateSettings,"spatial_fx_model",copy);obs_data_release(copy);obs_data_release(privateSettings);
    auto *handler=obs_source_get_proc_handler(source);proc_handler_add(handler,"void spatial_fx_set_state(in ptr settings)",procSetState,s);proc_handler_add(handler,"void spatial_fx_get_state(out ptr settings)",procGetState,s);return s;
}'''+s[end:]
start=s.index('obs_data_t *stageGetSettings(');end=s.index('void stageApplySettings(',start)
s=s[:start]+'''obs_data_t *stageGetSettings(obs_source_t *source){
    if(!source||obs_source_removed(source))return obs_data_create();auto *stage=static_cast<Stage*>(obs_obj_get_data(source));if(!stage)return obs_data_create();
    Read read(*stage);return read.value?modelData(read.value->model):obs_data_create();
}
'''+s[end:];p.write_text(s,encoding='utf-8')
p=Path('tests/obs_qa_driver.cpp');s=p.read_text(encoding='utf-8-sig');start=s.index('obs_data_t* currentSettings(');end=s.index('void click(',start)
s=s[:start]+'''obs_data_t* currentSettings(obs_source_t*source){if(!source)return nullptr;calldata_t cd;calldata_init(&cd);const bool ok=proc_handler_call(obs_source_get_proc_handler(source),"spatial_fx_get_state",&cd);auto*d=ok?static_cast<obs_data_t*>(calldata_ptr(&cd,"settings")):nullptr;calldata_free(&cd);return d?d:obs_source_get_settings(source);}
void bridge(){
    static QString lastId;QFile f(output+"/native-command.json");if(!f.open(QIODevice::ReadOnly))return;const auto command=QJsonDocument::fromJson(f.readAll()).object();const QString id=command.value("id").toString();if(id.isEmpty()||id==lastId)return;lastId=id;f.close();
    auto*source=obs_get_source_by_name(command.value("source").toString().toUtf8().constData());bool ok=source!=nullptr;
    if(source&&command.value("settings").isObject()){
        const auto bytes=QJsonDocument(command.value("settings").toObject()).toJson(QJsonDocument::Compact);auto*d=obs_data_create_from_json(bytes.constData());calldata_t cd;calldata_init(&cd);calldata_set_ptr(&cd,"settings",d);ok=proc_handler_call(obs_source_get_proc_handler(source),"spatial_fx_set_state",&cd);calldata_free(&cd);obs_data_release(d);
    }
    auto*state=source?currentSettings(source):nullptr;QJsonObject data=state?QJsonDocument::fromJson(obs_data_get_json(state)).object():QJsonObject{};obs_data_release(state);obs_source_release(source);
    QSaveFile answer(output+"/native-answer.json");if(answer.open(QIODevice::WriteOnly)){answer.write(QJsonDocument(QJsonObject{{"id",id},{"ok",ok},{"state",data}}).toJson());answer.commit();}
}
'''+s[end:];s=s.replace('#include <QFile>','#include <QFile>\n#include <QSaveFile>')
s=s.replace('obs_frontend_add_event_callback(event,nullptr);return true;', 'obs_frontend_add_event_callback(event,nullptr);auto*bridgeTimer=new QTimer(qApp);bridgeTimer->setInterval(30);QObject::connect(bridgeTimer,&QTimer::timeout,qApp,bridge);bridgeTimer->start();return true;')
p.write_text(s,encoding='utf-8')
