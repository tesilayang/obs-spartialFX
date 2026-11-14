#include "project_obs.hpp"
#include <obs-frontend-api.h>
#include <obs-hotkey.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QSet>
#include <QUuid>
#include <QDateTime>
#include <functional>
namespace spatial::projects {
namespace {
constexpr const char*stageId="obs_spatial_fx_stage";
obs_data_t*stageState(obs_source_t*s){auto*p=obs_source_get_private_settings(s);auto*saved=obs_data_get_obj(p,"spatial_fx_model");obs_data_release(p);if(saved)return saved;calldata_t cd;calldata_init(&cd);const bool ok=proc_handler_call(obs_source_get_proc_handler(s),"spatial_fx_get_state",&cd);auto*d=ok?static_cast<obs_data_t*>(calldata_ptr(&cd,"settings")):nullptr;calldata_free(&cd);return d?d:obs_source_get_settings(s);}
QString str(const char*s){return QString::fromUtf8(s?s:"");}
QJsonObject json(obs_data_t*d){return d?QJsonDocument::fromJson(obs_data_get_json(d)).object():QJsonObject{};}
obs_data_t*data(const QJsonObject&o){return obs_data_create_from_json(QJsonDocument(o).toJson(QJsonDocument::Compact).constData());}
QString fresh(){return QUuid::createUuid().toString(QUuid::WithoutBraces);}
QString unique(const QString&base,QSet<QString>&reserved){QString name=base;for(int i=2;;++i){auto*s=obs_get_source_by_name(name.toUtf8().constData());bool used=s||reserved.contains(name);obs_source_release(s);if(!used){reserved.insert(name);return name;}name=base+QString(" %1").arg(i);}}
QJsonValue remap(QJsonValue v,const QMap<QString,QString>&ids,const QMap<QString,QString>&names,const QString&key={}){
 if(v.isObject()){auto o=v.toObject();o.remove("canvas_uuid");for(auto it=o.begin();it!=o.end();++it)it.value()=remap(it.value(),ids,names,it.key());return o;}
 if(v.isArray()){auto a=v.toArray();for(int i=0;i<a.size();++i)a[i]=remap(a[i],ids,names,key);return a;}
 if(v.isString()){QString s=v.toString();if(ids.contains(s))return ids[s];if((key=="name"||key=="label"||key=="sidechain_source")&&names.contains(s))return names[s];}
 return v;
}
void setSettings(obs_source_t*s,const QJsonObject&o){auto*d=data(o);auto*settings=obs_source_get_settings(s);obs_data_clear(settings);obs_data_apply(settings,d);obs_data_release(settings);obs_source_update(s,d);obs_source_load2(s);obs_data_release(d);}
int itemCount(obs_source_t*s){int n=0;auto*scene=obs_scene_from_source(s);if(!scene)scene=obs_group_from_source(s);if(scene)obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t*,void*p){++*static_cast<int*>(p);return true;},&n);return n;}
int expectedCount(const QJsonObject&o){int n=0;for(auto v:o["settings"].toObject()["items"].toArray())if(!v.toObject()["group_item_backup"].toBool())++n;return n;}
bool availableImplementations(QJsonValue value,QString&error){
 if(value.isArray()){for(auto v:value.toArray())if(!availableImplementations(v,error))return false;}
 else if(value.isObject()){auto o=value.toObject();if(o["id"].isString()&&o["settings"].isObject()){
  const QString id=o["versioned_id"].toString(o["id"].toString());
  if(!obs_source_get_display_name(id.toUtf8().constData())){error=QString::fromUtf8("缺少来源或滤镜插件，已取消导入：")+id;return false;}
 }for(auto it=o.begin();it!=o.end();++it)if(!availableImplementations(it.value(),error))return false;}
 return true;
}
bool instantiated(obs_source_t*s,const QJsonObject&saved){
 if(!s||obs_obj_invalid(s))return false;
 std::vector<obs_source_t*>filters;obs_source_enum_filters(s,[](obs_source_t*,obs_source_t*f,void*p){static_cast<std::vector<obs_source_t*>*>(p)->push_back(f);},&filters);
 const auto expected=saved["filters"].toArray();if(filters.size()!=size_t(expected.size()))return false;
 for(auto*f:filters){QJsonObject match;for(auto v:expected)if(v.toObject()["uuid"].toString()==str(obs_source_get_uuid(f)))match=v.toObject();if(match.isEmpty()||!instantiated(f,match))return false;}
 return true;
}
void copyRootMetadata(obs_source_t*dst,obs_source_t*src){
 std::vector<obs_source_t*>filters;obs_source_enum_filters(dst,[](obs_source_t*,obs_source_t*f,void*p){static_cast<std::vector<obs_source_t*>*>(p)->push_back(obs_source_get_ref(f));},&filters);for(auto*f:filters){obs_source_filter_remove(dst,f);obs_source_release(f);}filters.clear();obs_source_enum_filters(src,[](obs_source_t*,obs_source_t*f,void*p){static_cast<std::vector<obs_source_t*>*>(p)->push_back(obs_source_get_ref(f));},&filters);for(auto*f:filters){obs_source_filter_remove(src,f);obs_source_filter_add(dst,f);obs_source_release(f);}
 auto*from=obs_source_get_private_settings(src);auto*to=obs_source_get_private_settings(dst);obs_data_clear(to);obs_data_apply(to,from);obs_data_release(from);obs_data_release(to);
 obs_source_set_volume(dst,obs_source_get_volume(src));obs_source_set_balance_value(dst,obs_source_get_balance_value(src));obs_source_set_audio_mixers(dst,obs_source_get_audio_mixers(src));obs_source_set_sync_offset(dst,obs_source_get_sync_offset(src));obs_source_set_flags(dst,obs_source_get_flags(src));obs_source_set_enabled(dst,obs_source_enabled(src));obs_source_set_muted(dst,obs_source_muted(src));obs_source_set_monitoring_type(dst,obs_source_get_monitoring_type(src));
 obs_source_enable_push_to_mute(dst,obs_source_push_to_mute_enabled(src));obs_source_set_push_to_mute_delay(dst,obs_source_get_push_to_mute_delay(src));obs_source_enable_push_to_talk(dst,obs_source_push_to_talk_enabled(src));obs_source_set_push_to_talk_delay(dst,obs_source_get_push_to_talk_delay(src));
 obs_source_set_deinterlace_mode(dst,obs_source_get_deinterlace_mode(src));obs_source_set_deinterlace_field_order(dst,obs_source_get_deinterlace_field_order(src));
 auto*hotkeys=obs_hotkeys_save_source(src);obs_hotkeys_load_source(dst,hotkeys);obs_data_release(hotkeys);
}
}
QString matchingSceneUuid(const QJsonObject&doc){auto*s=obs_get_source_by_name(doc["scene_name"].toString().toUtf8().constData());QString id=s&&obs_scene_from_source(s)?str(obs_source_get_uuid(s)):QString();obs_source_release(s);return id;}
QJsonObject captureScene(obs_source_t*root,QString&error){
 error.clear();if(!root||!obs_scene_from_source(root)){error=QString::fromUtf8("请选择有效的 OBS 场景。");return {};}
 QSet<QString>seen;QJsonArray sources,requirements;std::vector<obs_source_t*>queue{obs_source_get_ref(root)};
 for(size_t i=0;i<queue.size();++i){auto*s=queue[i];const QString uuid=str(obs_source_get_uuid(s));if(seen.contains(uuid)){obs_source_release(s);continue;}seen.insert(uuid);
  auto*d=obs_save_source(s);auto saved=json(d);obs_data_release(d);
  if(str(obs_source_get_unversioned_id(s))==stageId){auto*state=stageState(s);auto settings=json(state);settings["playing"]=false;saved["settings"]=settings;auto priv=saved["private_settings"].toObject();priv["spatial_fx_model"]=settings;saved["private_settings"]=priv;obs_data_release(state);}
  sources.append(saved);
  auto*scene=obs_scene_from_source(s);if(!scene&&str(obs_source_get_unversioned_id(s))=="group")scene=obs_group_from_source(s);
  if(scene)obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t*item,void*p){static_cast<std::vector<obs_source_t*>*>(p)->push_back(obs_source_get_ref(obs_sceneitem_get_source(item)));return true;},&queue);
  std::function<void(QJsonValue)>references=[&](QJsonValue v){if(v.isObject()){auto o=v.toObject();for(auto it=o.begin();it!=o.end();++it){if(it.key()=="sidechain_source"&&it.value().isString()){auto*ref=obs_get_source_by_name(it.value().toString().toUtf8().constData());if(ref)queue.push_back(ref);}else if(!(it.key()=="uuid"&&o["id"].isString()&&o["settings"].isObject()))references(it.value());}}else if(v.isArray()){for(auto e:v.toArray())references(e);}else if(v.isString()&&v.toString().size()==36){auto*ref=obs_get_source_by_uuid(v.toString().toUtf8().constData());if(ref){if(!seen.contains(str(obs_source_get_uuid(ref))))queue.push_back(ref);else obs_source_release(ref);}}};references(saved["settings"]);references(saved["private_settings"]);references(saved["filters"]);
  const QString id=str(obs_source_get_unversioned_id(s));
  if(id.contains("capture")||id.contains("dshow")||id.contains("wasapi")||id.contains("decklink"))requirements.append(QString::fromUtf8("设备/捕捉来源需要在目标机器核对：")+str(obs_source_get_name(s))+" ["+id+"]");
  if(saved["settings"].toObject().contains("url"))requirements.append(QString::fromUtf8("网络来源需联网且保持访问权限：")+str(obs_source_get_name(s)));
  auto font=saved["settings"].toObject()["font"].toObject()["face"].toString();if(!font.isEmpty())requirements.append(QString::fromUtf8("目标机器需安装字体：")+font);
  static const QSet<QString>builtin={"scene","group","obs_spatial_fx_stage","image_source","color_source","color_source_v3","ffmpeg_source","vlc_source","slideshow","browser_source","text_gdiplus","text_ft2_source","monitor_capture","window_capture","game_capture","dshow_input","wasapi_input_capture","wasapi_output_capture"};
  if(!builtin.contains(id))requirements.append(QString::fromUtf8("目标机器需安装来源插件：")+id);
  std::function<void(QJsonValue)>filterRequirements=[&](QJsonValue v){if(v.isArray())for(auto f:v.toArray())filterRequirements(f);else if(v.isObject()){auto o=v.toObject();if(o["id"].isString())requirements.append(QString::fromUtf8("目标机器需支持滤镜：")+o["versioned_id"].toString(o["id"].toString()));filterRequirements(o["filters"]);}};filterRequirements(saved["filters"]);
  obs_source_release(s);
  if(sources.size()>10000){for(size_t j=i+1;j<queue.size();++j)obs_source_release(queue[j]);error=QString::fromUtf8("场景依赖超过 10000 个来源。");return {};}
 }
 return QJsonObject{{"format","obs-spatial-fx-project"},{"version",1},{"plugin_version","3.6.0"},{"obs_version",str(obs_get_version_string())},{"created_utc",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},{"scene_name",str(obs_source_get_name(root))},{"root_uuid",str(obs_source_get_uuid(root))},{"sources",sources},{"requirements",requirements}};
}
ImportResult restoreScene(const QJsonObject&doc,bool nameMatch){
 ImportResult result;if(!validateDocument(doc,result.error))return result;
 if(!availableImplementations(doc["sources"],result.error))return result;
 auto sources=doc["sources"].toArray();const QString rootOld=doc["root_uuid"].toString(),requested=doc["scene_name"].toString();
 obs_source_t*existing=nameMatch?obs_get_source_by_name(requested.toUtf8().constData()):nullptr;
 if(existing&&!obs_scene_from_source(existing)){result.error=QString::fromUtf8("同名已被非场景来源占用：")+requested;obs_source_release(existing);return result;}
 if(existing){auto*saved=obs_save_source(existing);const auto previous=json(saved);obs_data_release(saved);for(auto v:sources)if(v.toObject()["uuid"]==rootOld&&previous["settings"].toObject()["custom_size"].toBool()!=v.toObject()["settings"].toObject()["custom_size"].toBool()){result.error=QString::fromUtf8("同名场景的自定义尺寸模式与存档不同，OBS 公共接口无法安全恢复该模式。请先重命名该场景，再导入以新建原场景。");obs_source_release(existing);return result;}}
 QSet<QString>reserved;QMap<QString,QString>ids,names;QString rootName=existing?requested:unique(requested,reserved);QString finalUuid=existing?str(obs_source_get_uuid(existing)):fresh();QString tempUuid=existing?fresh():finalUuid;
 ids[rootOld]=finalUuid;
 for(auto v:sources){auto s=v.toObject();QString old=s["uuid"].toString();if(old!=rootOld)ids[old]=fresh();names[s["name"].toString()]=old==rootOld?rootName:unique(s["name"].toString(),reserved);
  std::function<void(QJsonValue)>instances=[&](QJsonValue v){if(v.isObject()){auto o=v.toObject();const QString instance=o["instance_id"].toString();if(!instance.isEmpty()&&!ids.contains(instance))ids[instance]=fresh();if(o["id"].isString()&&o["settings"].isObject()){auto uuid=o["uuid"].toString();if(!uuid.isEmpty()&&!ids.contains(uuid))ids[uuid]=fresh();}for(auto it=o.begin();it!=o.end();++it)instances(it.value());}else if(v.isArray())for(auto e:v.toArray())instances(e);};instances(s);
 }
 QMap<QString,QJsonObject>byId;for(auto v:sources){auto s=v.toObject();byId[s["uuid"].toString()]=s;}
 QStringList order;QSet<QString>visiting,done;bool cycle=false;
 std::function<void(QString)>visit=[&](QString id){if(done.contains(id))return;if(visiting.contains(id)){cycle=true;return;}visiting.insert(id);auto s=byId[id];auto settings=s["settings"].toObject();QStringList dependencies;
  if(s["id"]=="scene"||s["id"]=="group")for(auto v:settings["items"].toArray())if(!v.toObject()["group_item_backup"].toBool()){const auto dependency=v.toObject()["source_uuid"].toString();dependencies<<dependency;if(dependency==rootOld)result.error=QString::fromUtf8("工程依赖包含根场景的上级场景，无法安全保留为隐藏依赖。请从最上级场景导出，或解除共享舞台的上级场景关联后重试。");}
  if(s["id"]==stageId){for(auto v:settings["layers"].toArray())dependencies<<v.toObject()["uuid"].toString();if(settings["media_linked"].toBool())dependencies<<settings["media_uuid"].toString();}
  for(auto dep:dependencies){if(!byId.contains(dep)){result.error=QString::fromUtf8("工程来源引用不完整：")+dep;continue;}visit(dep);}visiting.remove(id);done.insert(id);order<<id;};
 for(auto it=byId.begin();it!=byId.end();++it)visit(it.key());
 if(cycle||!result.error.isEmpty()){if(cycle)result.error=QString::fromUtf8("工程包含循环场景依赖。");obs_source_release(existing);return result;}
 auto*array=obs_data_array_create();QMap<QString,QJsonObject>rewritten;QString tempName=existing?unique(requested+QString::fromUtf8(" · 导入校验"),reserved):rootName;
 for(auto id:order){auto s=remap(byId[id],ids,names).toObject();if(id==rootOld){s["uuid"]=tempUuid;s["name"]=tempName;}auto settings=s["settings"].toObject();if(s["id"]==stageId){settings["playing"]=false;s["settings"]=settings;}rewritten[id]=s;auto*d=data(s);obs_data_array_push_back(array,d);obs_data_release(d);}
 std::vector<obs_source_t*>created;obs_load_sources(array,[](void*p,obs_source_t*s){static_cast<std::vector<obs_source_t*>*>(p)->push_back(obs_source_get_ref(s));},&created);obs_data_array_release(array);
 auto*newRoot=obs_get_source_by_uuid(tempUuid.toUtf8().constData());bool valid=created.size()==size_t(sources.size())&&newRoot&&obs_scene_from_source(newRoot);
 for(auto id:order){auto s=rewritten[id];auto*source=obs_get_source_by_uuid(s["uuid"].toString().toUtf8().constData());if(!instantiated(source,s))valid=false;if(source&&(s["id"]=="scene"||s["id"]=="group")&&itemCount(source)!=expectedCount(s))valid=false;obs_source_release(source);}
 if(valid){
  // Native scene items own their source references. UUID/name bindings in
  // filters and stage settings do not guarantee ownership, especially while
  // disabled. Retain detached dependencies as invisible native group items so
  // OBS also saves them and restores them before releasing its load references.
  QSet<QString>owned;for(auto id:order){auto s=rewritten[id];if(s["id"]=="scene"||s["id"]=="group")for(auto item:s["settings"].toObject()["items"].toArray())if(!item.toObject()["group_item_backup"].toBool())owned.insert(item.toObject()["source_uuid"].toString());}
  std::vector<obs_source_t*>detached;for(auto*s:created)if(s!=newRoot&&!owned.contains(str(obs_source_get_uuid(s))))detached.push_back(s);
  if(!detached.empty()){auto*helper=obs_scene_add_group(obs_scene_from_source(newRoot),unique(QString::fromUtf8("Spatial FX 工程依赖"),reserved).toUtf8().constData());if(!helper)valid=false;else{obs_sceneitem_set_visible(helper,false);obs_sceneitem_set_locked(helper,true);auto*group=obs_sceneitem_group_get_scene(helper);for(auto*s:detached){auto*item=obs_scene_add(group,s);if(!item){valid=false;break;}obs_sceneitem_set_visible(item,false);obs_sceneitem_set_locked(item,true);}}}
  if(valid&&existing){struct Check{obs_source_t*target;bool found=false;}check{existing};obs_source_enum_full_tree(newRoot,[](obs_source_t*parent,obs_source_t*child,void*p){auto*c=static_cast<Check*>(p);if(parent==c->target||child==c->target)c->found=true;},&check);if(check.found)valid=false;}
  if(valid&&existing){auto*saved=obs_save_source(newRoot);auto settings=json(saved)["settings"].toObject();obs_data_release(saved);auto*backup=obs_save_source(existing);const auto originalSettings=json(backup)["settings"].toObject();obs_data_release(backup);std::vector<obs_source_t*>originalRefs;obs_scene_enum_items(obs_scene_from_source(existing),[](obs_scene_t*,obs_sceneitem_t*item,void*p){static_cast<std::vector<obs_source_t*>*>(p)->push_back(obs_source_get_ref(obs_sceneitem_get_source(item)));return true;},&originalRefs);setSettings(existing,settings);valid=itemCount(existing)==expectedCount(QJsonObject{{"settings",settings}});if(valid){copyRootMetadata(existing,newRoot);obs_source_remove(newRoot);}else setSettings(existing,originalSettings);for(auto*s:originalRefs)obs_source_release(s);}
  if(valid){
  result.success=true;result.sceneName=rootName;result.sceneUuid=finalUuid;result.warnings=missingAssets(doc);for(auto r:doc["requirements"].toArray())result.warnings<<r.toString();obs_frontend_save();
  }
 }
 if(!valid)result.error=QString::fromUtf8("来源或场景未完整重建，已取消导入；原场景保持原样。请检查缺失插件。");
 if(!valid)for(auto*s:created)obs_source_remove(s);
 obs_source_release(newRoot);for(auto*s:created)obs_source_release(s);obs_source_release(existing);return result;
}
}

