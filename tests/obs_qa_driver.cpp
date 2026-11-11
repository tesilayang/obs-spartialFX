#include <algorithm>
#include <memory>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QCheckBox>
#include <QColorDialog>
#include <cmath>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QSaveFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QTimer>
#include <QDir>
#include <QPixmap>
#include <QSlider>
#include <QScrollArea>
#include <QScrollBar>
#include <QScreen>
#include <QMouseEvent>
#include <QMessageBox>
#include <QToolTip>
#include <QEnterEvent>
#include <QCursor>
#include <QGroupBox>
#include <QToolButton>
#include <QLayout>
#include <QDialog>
#include <QMenu>
#include <QDateTime>
OBS_DECLARE_MODULE()
namespace {
QString output,scenario;QJsonArray checks;int failures=0;
void check(bool ok,const QString&label){checks.push_back(QJsonObject{{"name",label},{"pass",ok}});if(!ok)++failures;blog(ok?LOG_INFO:LOG_ERROR,"[SpatialFX QA] %s %s",ok?"PASS":"FAIL",label.toUtf8().constData());}
QWidget* mainWidget(){return static_cast<QWidget*>(obs_frontend_get_main_window());}
QWidget* panel(){auto*main=mainWidget();return main?main->findChild<QWidget*>("spatial.dock"):nullptr;}
obs_source_t* stageSource(){auto*scene=obs_frontend_get_current_scene();obs_source_t*result=nullptr;if(scene){auto*s=obs_scene_from_source(scene);if(s)obs_scene_enum_items(s,[](obs_scene_t*,obs_sceneitem_t*i,void*p){auto*source=obs_sceneitem_get_source(i);if(QString::fromUtf8(obs_source_get_unversioned_id(source))=="obs_spatial_fx_stage"){auto**out=static_cast<obs_source_t**>(p);if(!*out)*out=obs_source_get_ref(source);}return true;},&result);obs_source_release(scene);}return result;}
obs_data_t* currentSettings(obs_source_t*source){if(!source)return nullptr;calldata_t cd;calldata_init(&cd);const bool ok=proc_handler_call(obs_source_get_proc_handler(source),"spatial_fx_get_state",&cd);auto*d=ok?static_cast<obs_data_t*>(calldata_ptr(&cd,"settings")):nullptr;calldata_free(&cd);return d?d:obs_source_get_settings(source);}
void finish();
void bridge(){
    static QString lastId;QFile f(output+"/native-command.json");if(!f.open(QIODevice::ReadOnly))return;const auto command=QJsonDocument::fromJson(f.readAll()).object();const QString id=command.value("id").toString();if(id.isEmpty()||id==lastId)return;f.close();
    if(command.value("action").toString()=="finish"){lastId=id;QTimer::singleShot(0,qApp,finish);return;}
    auto*source=obs_get_source_by_name(command.value("source").toString().toUtf8().constData());bool ok=source!=nullptr;
    const auto action=command.value("action").toString();
    if(action=="ui-refresh"){auto*b=panel()?panel()->findChild<QPushButton*>("spatial.refresh"):nullptr;ok&=b!=nullptr;if(b)b->click();}
    if(action=="fx-color-open"){auto*b=panel()?panel()->findChild<QPushButton*>(command.value("widget").toString()):nullptr;ok&=b!=nullptr;if(b)b->click();}
    if(action.startsWith("fx-color-")&&action!="fx-color-open"){
        QColorDialog*d=nullptr;if(panel())for(auto*c:panel()->findChildren<QColorDialog*>("spatial.fx-color-dialog34"))if(c->isVisible())d=c;ok&=d!=nullptr;
        if(d){if(action=="fx-color-set")d->setCurrentColor(QColor(command.value("color").toString()));else if(action=="fx-color-cancel")d->reject();else if(action=="fx-color-accept")d->accept();}
    }
    if(source&&command.value("settings").isObject()){
        const auto bytes=QJsonDocument(command.value("settings").toObject()).toJson(QJsonDocument::Compact);auto*d=obs_data_create_from_json(bytes.constData());calldata_t cd;calldata_init(&cd);calldata_set_ptr(&cd,"settings",d);ok=proc_handler_call(obs_source_get_proc_handler(source),"spatial_fx_set_state",&cd);calldata_free(&cd);obs_data_release(d);
    }
    auto*state=source?currentSettings(source):nullptr;QJsonObject data=state?QJsonDocument::fromJson(obs_data_get_json(state)).object():QJsonObject{};obs_data_release(state);
    if(source){calldata_t metrics;calldata_init(&metrics);if(proc_handler_call(obs_source_get_proc_handler(source),"spatial_fx_get_metrics",&metrics))data.insert("_metrics",QJsonObject{{"allocations",qint64(calldata_int(&metrics,"fx_allocations"))},{"passes",qint64(calldata_int(&metrics,"fx_passes"))},{"failures",qint64(calldata_int(&metrics,"fx_failures"))},{"renders",qint64(calldata_int(&metrics,"renders"))},{"renderMs",calldata_float(&metrics,"render_ms")}});calldata_free(&metrics);}obs_source_release(source);
    QSaveFile answer(output+"/native-answer.json");if(answer.open(QIODevice::WriteOnly)){answer.write(QJsonDocument(QJsonObject{{"id",id},{"ok",ok},{"state",data}}).toJson());if(answer.commit())lastId=id;}
}
void click(const char*name){auto*p=panel();auto*b=p?p->findChild<QPushButton*>(name):nullptr;check(b&&b->isEnabled(),QString("button available: %1").arg(name));if(b&&b->isEnabled())b->click();}
void setNumber(const char*name,double v){auto*p=panel();auto*f=p?p->findChild<QDoubleSpinBox*>(name):nullptr;check(f!=nullptr,QString("field available: %1").arg(name));if(f)f->setValue(v);}
void finish(){
    auto*main=mainWidget();auto*p=panel();auto*tabs=p?p->findChild<QTabWidget*>("spatial.tabs"):nullptr;
    check(p!=nullptr,"native dock content exists");check(tabs&&tabs->count()==((scenario.startsWith("v33")||scenario.startsWith("v34"))?4:5),"expected native tabs exist");bool native=false;if(p)for(auto*w=p->parentWidget();w;w=w->parentWidget())if(qobject_cast<QDockWidget*>(w))native=true;check(native,"content is inside OBS QDockWidget");
    QJsonArray labels;
    if(tabs)for(int i=0;i<tabs->count();++i){tabs->setCurrentIndex(i);labels.append(tabs->tabText(i));QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);check(p->grab().save(output+QString("/dock-tab-%1.png").arg(i)),QString("real dock tab %1 screenshot").arg(i));}
    if(tabs)tabs->setCurrentIndex(0);if(main)check(main->grab().save(output+"/obs-main.png"),"real OBS main window screenshot");
    QJsonObject settings;auto*source=stageSource();if(source){auto*d=currentSettings(source);settings=QJsonDocument::fromJson(obs_data_get_json(d)).object();obs_data_release(d);obs_source_release(source);}
    if(!scenario.startsWith("camera-remake")&&!scenario.startsWith("v3")&&scenario!="phase1"&&scenario!="v2"&&scenario!="v2persist"&&scenario!="v2hold"&&scenario!="v2regress"&&scenario!="v2migrate"){
        auto*layers=p?p->findChild<QListWidget*>("spatial.layers"):nullptr;check(layers&&layers->count()>=3,"demo layers are editable in dock");check(settings.value("camera_x").toDouble()==177,"camera control persisted to actual OBS source settings");
        if(scenario!="phase2"){auto*keys=p?p->findChild<QTableWidget*>("spatial.keyframes"):nullptr;check(keys&&keys->rowCount()>=2,"keyframe table populated");auto*presets=p?p->findChild<QComboBox*>("spatial.preset-choice"):nullptr;check(presets&&presets->findText("QA Persisted")>=0,"saved preset available in native UI");}
    }
    auto*perf=p?p->findChild<QLabel*>("spatial.performance"):nullptr;
    QJsonObject report{{"scenario",scenario},{"checks",checks},{"failed",failures},{"tabs",labels},{"qtRuntime",qVersion()},{"stageSettings",settings},{"performanceLabel",perf?perf->text():QString()}};
    QFile file(output+"/dock-smoke.json");if(file.open(QIODevice::WriteOnly))file.write(QJsonDocument(report).toJson());
    obs_frontend_save();blog(LOG_INFO,"[SpatialFX QA] scenario %s finished: %d failures",scenario.toUtf8().constData(),failures);
    QTimer::singleShot(1000,main,[main](){main->close();});
}

void lifecycle(){
    auto*original=obs_frontend_get_current_scene();auto*originalStage=stageSource();
    auto*seed=originalStage?currentSettings(originalStage):nullptr;auto*arr=seed?obs_data_get_array(seed,"layers"):nullptr;auto*first=arr?obs_data_array_item(arr,0):nullptr;auto*image=first?obs_get_source_by_uuid(obs_data_get_string(first,"uuid")):nullptr;
    auto*imageSettings=image?obs_source_get_settings(image):nullptr;auto*test=obs_scene_create("Spatial FX Lifecycle QA");
    if(imageSettings&&test){
        auto*source=obs_source_create("image_source","Spatial lifecycle original",imageSettings,nullptr);auto*item=source?obs_scene_add(test,source):nullptr;
        obs_frontend_set_current_scene(obs_scene_get_source(test));click("spatial.refresh");click("spatial.create-stage");
        if(item)obs_sceneitem_select(item,true);click("spatial.import-selected");
        auto*stage=stageSource();auto*d=stage?currentSettings(stage):nullptr;auto*layers=d?obs_data_get_array(d,"layers"):nullptr;
        check(layers&&obs_data_array_count(layers)==1,"selected source imports into actual stage");check(item&&!obs_sceneitem_visible(item),"import hides original item");
        if(source)obs_source_set_name(source,"Spatial lifecycle renamed");click("spatial.refresh");auto*list=panel()->findChild<QListWidget*>("spatial.layers");check(list&&list->count()==1&&list->item(0)->text()=="Spatial lifecycle renamed","UUID association survives source rename");
        click("spatial.restore-all");check(item&&obs_sceneitem_visible(item),"restore button restores original visibility");
        if(item)obs_sceneitem_select(item,true);click("spatial.import-selected");if(source)obs_source_remove(source);click("spatial.refresh");
        check(list&&list->count()==1&&list->item(0)->text().contains(QString::fromUtf8("源已删除")),"deleted source marked safely in dock");click("spatial.restore-all");
        obs_data_array_release(layers);obs_data_release(d);obs_source_release(stage);obs_source_release(source);
    }else check(false,"lifecycle fixture available");
    if(original){obs_frontend_set_current_scene(original);click("spatial.refresh");}
    auto*checkStage=stageSource();check(checkStage!=nullptr,"scene switching returns to persisted stage");obs_source_release(checkStage);obs_source_release(original);obs_source_release(originalStage);obs_source_release(image);obs_data_release(imageSettings);obs_data_release(first);obs_data_array_release(arr);obs_data_release(seed);if(test)obs_scene_release(test);
    QTimer::singleShot(2000,qApp,finish);
}

#include "v2_qa.inc"
#include "v3_qa.inc"
#include "v31_qa.inc"
#include "v32_qa.inc"
#include "v32fix_qa.inc"
#include "v33_qa.inc"
#include "v33fix_qa.inc"
#include "v33ui_qa.inc"
#include "v34_qa.inc"
#include "v341_qa.inc"
void exercise(){
    auto*main=mainWidget();if(main)main->resize(1460,920);if(scenario=="baseline"){QTimer::singleShot(3000,qApp,[]{mainWidget()->close();});return;}auto*p=panel();if(!p){check(false,"dock found for scenario");finish();return;}
    if(scenario=="baseline"){QTimer::singleShot(3000,qApp,[]{mainWidget()->close();});return;}
    if(scenario=="phase1"){finish();return;}
    
    if(scenario=="presets"){
        click("spatial.refresh");auto*source=stageSource();auto*before=source?currentSettings(source):nullptr;
        const auto previous=before?QJsonDocument::fromJson(obs_data_get_json(before)).object().value("layers").toArray():QJsonArray{};
        auto*choice=p->findChild<QComboBox*>("spatial.preset-choice");const int index=choice?choice->findText("QA Persisted"):-1;
        check(index>=0,"named preset exists before apply");if(index>=0)choice->setCurrentIndex(index);
        setNumber("spatial.camera-x",999);click("spatial.apply-preset");
        auto*after=source?currentSettings(source):nullptr;check(after&&obs_data_get_double(after,"camera_x")==177,"named preset restores camera through actual button");
        const auto current=after?QJsonDocument::fromJson(obs_data_get_json(after)).object().value("layers").toArray():QJsonArray{};
        bool stable=previous.size()==current.size();for(qsizetype i=0;stable&&i<previous.size();++i)stable=previous[i].toObject().value("uuid")==current[i].toObject().value("uuid")&&previous[i].toObject().value("scene_uuid")==current[i].toObject().value("scene_uuid");
        check(stable,"preset apply preserves actual layer and scene UUIDs");obs_data_release(before);obs_data_release(after);obs_source_release(source);
        QTimer::singleShot(2500,qApp,finish);return;
    }

    if(scenario=="v34-hold"){
        click("spatial.refresh");
        QSaveFile ready(output+"/native-ready.json");
        if(ready.open(QIODevice::WriteOnly)){
            ready.write(QJsonDocument(QJsonObject{{"ready",true},{"scenario",scenario},{"pid",qint64(QCoreApplication::applicationPid())},{"utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)}}).toJson());
            ready.commit();
        }
        return;
    }
    if(scenario=="v341"){v341exercise();return;}
    if(scenario=="v341-persist"){v341persist();return;}
    if(scenario=="v34"){v34exercise();return;}
    if(scenario=="v34-persist"){v34persist();return;}
    if(scenario=="v33"){v33exercise();return;}
    if(scenario=="v33-ui"){v33uiExercise();return;}
    if(scenario=="v33-fix"){v33fixExercise();return;}
    if(scenario=="v33-fix-persist"){v33fixPersist();return;}
    if(scenario=="v33-fix-hold"){click("spatial.refresh");return;}
    if(scenario=="v33-edge"){v33edge();return;}
    if(scenario=="v33-shutdown"){v33shutdown();return;}
    if(scenario=="v33-persist"){v33persist();return;}
    if(scenario=="camera-remake-fix"){v32fixExercise();return;}
    if(scenario=="camera-remake-edit"){v32editExercise();return;}
    if(scenario=="camera-remake-dense"){v32denseExercise();return;}
    if(scenario=="camera-remake-audio"){v32audioExercise();return;}
    if(scenario=="camera-remake-persist"){QTimer::singleShot(1500,qApp,finish);return;}
    if(scenario=="camera-remake"){v32exercise();return;}
    if(scenario=="camera-remake-hold"){click("spatial.refresh");return;}
    if(scenario=="v31"){v31exercise();return;}
    if(scenario=="v3"){v3exercise();return;}
    if(scenario=="v3hold"){click("spatial.refresh");return;}
    if(scenario=="v2regress"){v2regression();return;}
    if(scenario=="v2"){v2exercise();return;}
    if(scenario=="v2migrate"){
        click("spatial.refresh");auto*stage=stageSource();auto*d=stage?currentSettings(stage):nullptr;auto*a=d?obs_data_get_array(d,"layers"):nullptr;auto*l=a?obs_data_array_item(a,0):nullptr;
        check(d&&obs_data_get_int(d,"schema_version")==2,"actual v1 scene saved as schema2 after installed upgrade");check(l&&obs_data_get_double(l,"z")>0,"actual v1 all-flat scene receives depth after upgrade");check(d&&obs_data_get_double(d,"animation_duration")==3,"v1 three-second animation survives installed upgrade");
        obs_data_release(l);obs_data_array_release(a);obs_data_release(d);obs_source_release(stage);QTimer::singleShot(4000,qApp,finish);return;
    }
    if(scenario=="v2persist"){
        click("spatial.refresh");auto*stage=stageSource();auto*d=stage?currentSettings(stage):nullptr;
        check(d&&obs_data_get_int(d,"schema_version")==2,"schema2 persisted across restart");
        auto*extra=d?obs_data_get_array(d,"extra_lights"):nullptr;check(extra&&obs_data_array_count(extra)>=1,"additional area light persisted");
        obs_data_array_release(extra);obs_data_release(d);obs_source_release(stage);QTimer::singleShot(3000,qApp,finish);return;
    }
    if(scenario=="stress"){
        click("spatial.create-demo");if(auto*fx=p->findChild<QCheckBox*>("spatial.fx-enabled"))fx->setChecked(true);auto*light=p->findChild<QCheckBox*>("spatial.light-enabled");if(light)light->setChecked(true);
        auto*t=new QTimer(qApp);t->setInterval(std::clamp(qEnvironmentVariableIntValue("SPATIAL_QA_STRESS_INTERVAL_MS")?qEnvironmentVariableIntValue("SPATIAL_QA_STRESS_INTERVAL_MS"):2,1,100));auto*step=new int(0);
        QObject::connect(t,&QTimer::timeout,qApp,[t,step]{
            const int n=(*step)++;auto*p=panel();if(!p)return;
            const char*names[]={"spatial.light-x","spatial.light-y","spatial.light-z","spatial.camera-x","spatial.camera-y","spatial.camera-z","spatial.camera-ry","spatial.layer-z"};
            for(int i=0;i<8;++i){auto*f=p->findChild<QDoubleSpinBox*>(names[i]);if(f)f->setValue(i==6?((n*31)%360-180):(i==5?-1500+(n%7)*250:((n*(71+i*13))%4000-2000)));}
            if(n%100==0)blog(LOG_INFO,"[SpatialFX QA] stress edits=%d",n*8);
            if(n>=std::clamp(qEnvironmentVariableIntValue("SPATIAL_QA_STRESS_ITERATIONS")?qEnvironmentVariableIntValue("SPATIAL_QA_STRESS_ITERATIONS"):2000,2000,200000)){t->stop();delete step;setNumber("spatial.camera-x",177);setNumber("spatial.camera-y",0);setNumber("spatial.camera-z",-1500);setNumber("spatial.camera-ry",0);check(true,QString("%1 spatial edits finished without process termination").arg((n+1)*8));QTimer::singleShot(1500,qApp,finish);}
        });t->start();return;
    }
    if(scenario=="lifecycle"){lifecycle();return;}
    if(scenario=="persist"||scenario=="hold"||scenario=="v2hold"){click("spatial.refresh");int hold=qEnvironmentVariableIntValue("SPATIAL_QA_HOLD_SECONDS");if(scenario=="hold"||scenario=="v2hold")hold=std::clamp(hold,10,600);else hold=3;QTimer::singleShot(hold*1000,qApp,finish);return;}
    click("spatial.create-demo");auto*layers=p->findChild<QListWidget*>("spatial.layers");if(layers)layers->setCurrentRow(0);setNumber("spatial.layer-z",0);setNumber("spatial.camera-x",177);
    if(scenario!="phase2"){
        setNumber("spatial.key-time",0);click("spatial.add-key");setNumber("spatial.camera-x",277);setNumber("spatial.key-time",3);click("spatial.add-key");setNumber("spatial.camera-x",177);
        auto*name=p->findChild<QLineEdit*>("spatial.preset-name");if(name)name->setText("QA Persisted");click("spatial.save-preset");click("spatial.play");
        QTimer::singleShot(2500,qApp,[]{click("spatial.stop");});
    }
    if(scenario=="phase4"){
        auto*light=p->findChild<QCheckBox*>("spatial.light-enabled");if(light)light->setChecked(true);setNumber("spatial.light-intensity",2.0);setNumber("spatial.fx-saturation",.8);
    }
    QTimer::singleShot(scenario=="phase4"?25000:15000,qApp,finish);
}
void event(enum obs_frontend_event e,void*){if(e==OBS_FRONTEND_EVENT_FINISHED_LOADING)QTimer::singleShot(1800,qApp,exercise);}
}
bool obs_module_load(void){output=qEnvironmentVariable("SPATIAL_QA_OUTPUT");scenario=qEnvironmentVariable("SPATIAL_QA_SCENARIO");if(output.isEmpty())return false;if(scenario.isEmpty())scenario="phase1";QDir().mkpath(output);obs_frontend_add_event_callback(event,nullptr);auto*bridgeTimer=new QTimer(qApp);bridgeTimer->setInterval(30);QObject::connect(bridgeTimer,&QTimer::timeout,qApp,bridge);bridgeTimer->start();return true;}
void obs_module_unload(void){obs_frontend_remove_event_callback(event,nullptr);}





