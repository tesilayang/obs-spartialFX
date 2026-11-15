#include "editor.hpp"
#include <QMenu>
#include <QMessageBox>
#include <QAction>
using namespace spatial;
namespace {QString q(const std::string&s){return QString::fromStdString(s);}}
void CameraEditor::contextMenu(int object,const std::string&parameter,QPoint position){
 const bool hit=validObject(*model,object);
 if(hit)selectObject(object,parameter);
 QMenu menu(this);menu.setObjectName("spatial.timeline-context364");
 auto*copy=menu.addAction(QString::fromUtf8("复制时间轴信息"));copy->setObjectName("spatial.copy-animation364");
 auto*paste=menu.addAction(QString::fromUtf8("粘贴时间轴信息"));paste->setObjectName("spatial.paste-animation364");paste->setEnabled(!timelineClipboard.empty()&&!objectLocked(*model,active));
 menu.addSeparator();auto*remove=menu.addAction(QString::fromUtf8("删除这个图层"));remove->setObjectName("spatial.delete-object364");remove->setEnabled(active!=-1&&active!=-2&&!objectLocked(*model,active));
 auto*copyLayer=menu.addAction(QString::fromUtf8("复制这个图层"));copyLayer->setObjectName("spatial.copy-layer364");copyLayer->setEnabled(active>=0);
 auto*pasteLayer=menu.addAction(QString::fromUtf8("粘贴图层"));pasteLayer->setObjectName("spatial.paste-layer364");pasteLayer->setEnabled(layerClipboard.has_value()&&model->layers.size()<32);
 if(!hit){menu.removeAction(copy);menu.removeAction(paste);menu.removeAction(remove);menu.removeAction(copyLayer);}
 const auto id=identity;auto*action=menu.exec(position);if(!action)return;
 if(action==pasteLayer){StageModel next=*model;std::string error;if(!duplicateTimelineLayer(next,*layerClipboard,error)){if(message)message(q(error),true);return;}applyDocument(next);selectLayer(int(model->layers.size())-1);return;}
 if(id!=identity||!validObject(*model,active))return;
 if(action==copy){timelineClipboard.clear();for(const auto&pair:objectAnimation(*model,active))if(parameter.empty()||pair.first==parameter)timelineClipboard.insert(pair);if(message)message(QString::fromUtf8("已复制 %1 条参数轨。").arg(timelineClipboard.size()),false);}
 else if(action==copyLayer){layerClipboard=model->layers[size_t(active)];if(message)message(QString::fromUtf8("已复制图层；粘贴后创建独立实例。"),false);}
 else if(action==remove)deleteTimelineObject();
 else if(action==paste){auto prepared=prepareAnimationPaste(*model,active,timelineClipboard,toTime(model->timelinePosition));QStringList skipped;for(auto&s:prepared.skipped)skipped<<q(s);if(prepared.tracks.empty()){if(message)message(QString::fromUtf8("没有可粘贴的兼容参数：")+skipped.join(", "),true);return;}
  if(prepared.conflict){QStringList names;for(auto&v:prepared.tracks)names<<q(objectParameter(*model,active,v.first)->label);QMessageBox box(QMessageBox::Question,QString::fromUtf8("确认替换关键帧"),QString::fromUtf8("将替换 %1s 至 %2s 内的以下参数关键帧：\n%3\n区间外关键帧保留。是否继续？").arg(toSeconds(prepared.first),0,'f',3).arg(toSeconds(prepared.last),0,'f',3).arg(names.join(", ")),QMessageBox::Ok|QMessageBox::Cancel,this);box.setObjectName("spatial.paste-confirm364");box.setDefaultButton(QMessageBox::Cancel);if(box.exec()!=QMessageBox::Ok||id!=identity)return;}
  StageModel next=*model;if(!pasteObjectAnimation(next,active,prepared))return;remember();*model=std::move(next);model->playing=false;model->timelinePreview=true;timeline->selection.clear();apply();if(message)message(skipped.empty()?QString::fromUtf8("时间轴信息已粘贴到当前播放头。"):QString::fromUtf8("已粘贴兼容参数；跳过：")+skipped.join(", "),false);
 }
}
void CameraEditor::deleteTimelineObject(){
 if(!validObject(*model,active)||active==-1||active==-2||objectLocked(*model,active))return;const auto id=identity;
 QMessageBox box(QMessageBox::Question,QString::fromUtf8("确认移除对象"),QString::fromUtf8("移除“%1”及其动画子轨？相关绑定将解除。原 OBS 来源保留，此操作可撤销。").arg(objectName(active)),QMessageBox::Yes|QMessageBox::Cancel,this);box.setObjectName("spatial.confirm-remove-object364");box.setDefaultButton(QMessageBox::Cancel);if(box.exec()!=QMessageBox::Yes||id!=identity)return;
 StageModel next=*model;std::string removed;std::optional<Layer>original;
 if(active>=0){original=next.layers[size_t(active)];removed=original->instanceId;next.layers.erase(next.layers.begin()+active);}
 else if(geometryObject(active)){removed=next.geometries[size_t(-3-active)].instanceId;next.geometries.erase(next.geometries.begin()+(-3-active));}
 else if(lightObject(active)){const auto i=size_t(lightHandleBase-active);if(i){next.extraLights.erase(next.extraLights.begin()+ptrdiff_t(i-1));}else if(!next.extraLights.empty()){next.light=next.extraLights.front();next.extraLights.erase(next.extraLights.begin());}else next.light={};}
 else if(auto*f=timelineFx(next,active)){const auto effectId=f->id;auto&effects=next.layers[size_t(int64_t(fxHandleBase)-active)/256].fx.effects;effects.erase(std::remove_if(effects.begin(),effects.end(),[&](const auto&v){return v.id==effectId;}),effects.end());}
 if(!removed.empty()){
  auto detach=[&](Light&l){if(l.bindInstanceId==removed){l.bindUuid.clear();l.bindSceneUuid.clear();l.bindInstanceId.clear();l.bindItemId=0;}if(l.receiverInstanceId==removed){l.receiverUuid.clear();l.receiverSceneUuid.clear();l.receiverInstanceId.clear();l.receiverItemId=0;}};detach(next.light);for(auto&l:next.extraLights)detach(l);
  for(auto&l:next.layers)for(auto&f:l.fx.effects)if(f.reference==removed)f.reference.clear();for(auto&g:next.geometries){if(g.anchorId==removed)g.anchorId.clear();g.receivers.erase(std::remove(g.receivers.begin(),g.receivers.end(),removed),g.receivers.end());}
  next.motionBlocks.erase(std::remove_if(next.motionBlocks.begin(),next.motionBlocks.end(),[&](const auto&b){return b.targetId==removed;}),next.motionBlocks.end());
 }
 applyDocument(next);
}
void CameraEditor::syncSceneVisibility(const StageModel& before){
 std::vector<Layer> touched=before.layers;touched.insert(touched.end(),model->layers.begin(),model->layers.end());
 for(const auto&layer:touched){const bool managed=std::any_of(model->layers.begin(),model->layers.end(),[&](const auto&l){return sameSourceItem(l,layer);});auto*source=obs_get_source_by_uuid(layer.sceneUuid.c_str());auto*scene=source?obs_scene_from_source(source):nullptr;auto*item=scene?obs_scene_find_sceneitem_by_id(scene,layer.itemId):nullptr;if(item&&std::string(obs_source_get_uuid(obs_sceneitem_get_source(item)))==layer.sourceUuid)obs_sceneitem_set_visible(item,managed?false:layer.originalVisible);obs_source_release(source);}
}
