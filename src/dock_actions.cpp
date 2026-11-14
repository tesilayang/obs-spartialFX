#if SPATIAL_PHASE >= 2
#include "dock.hpp"
#include "editor.hpp"
#include "presets/panel.hpp"
#include "scene_evaluator.hpp"
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <graphics/matrix4.h>
#include <graphics/vec2.h>
#include <graphics/vec3.h>
#include <util/bmem.h>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QSaveFile>
#include <QUuid>
#include <QSignalBlocker>
#include <QTableWidget>
#include <algorithm>
#include <cmath>
using namespace spatial;
namespace {
QString tr8(const char*s){return QString::fromUtf8(s);}
std::string uniqueName(const QString &base){QString name=base;for(int n=2;;++n){auto*s=obs_get_source_by_name(name.toUtf8().constData());if(!s)return name.toUtf8().toStdString();obs_source_release(s);name=base+QString(" %1").arg(n);}}
QJsonValue remapPresetIds(QJsonValue v,const QMap<QString,QString>& ids){if(v.isString())return ids.value(v.toString(),v.toString());if(v.isArray()){QJsonArray a;for(auto e:v.toArray())a.append(remapPresetIds(e,ids));return a;}if(v.isObject()){auto o=v.toObject();for(auto it=o.begin();it!=o.end();++it)it.value()=remapPresetIds(it.value(),ids);return o;}return v;}
}
void SpatialDock::refreshScene(bool force){
    auto*sceneSource=obs_frontend_get_current_scene();if(!sceneSource)return;
    const QString uuid=QString::fromUtf8(obs_source_get_uuid(sceneSource));sceneLabel->setText(tr8("当前场景 · ")+QString::fromUtf8(obs_source_get_name(sceneSource)));
    std::vector<std::pair<QString,QString>> stages;
    auto*scene=obs_scene_from_source(sceneSource);
    if(scene)obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t*item,void*param){auto*source=obs_sceneitem_get_source(item);if(std::string(obs_source_get_unversioned_id(source))==stageId)static_cast<std::vector<std::pair<QString,QString>>*>(param)->push_back({QString::fromUtf8(obs_source_get_name(source)),QString::fromUtf8(obs_source_get_uuid(source))});return true;},&stages);
    obs_source_release(sceneSource);QString ids;for(const auto&s:stages)ids+=s.second+';';
    if(force||uuid!=sceneUuid||ids!=lastStageIds){
        const QString previous=stageUuid;sceneUuid=uuid;lastStageIds=ids;loading=true;stageChoice->clear();for(const auto&s:stages)stageChoice->addItem(s.first,s.second);
        const int index=stageChoice->findData(previous);if(index>=0)stageChoice->setCurrentIndex(index);stageUuid=stageChoice->currentData().toString();loading=false;loadStage();
    }
    refreshSources();
}
void SpatialDock::createStage(const StageModel *initial){
    auto*sceneSource=obs_frontend_get_current_scene();auto*scene=sceneSource?obs_scene_from_source(sceneSource):nullptr;if(!scene){obs_source_release(sceneSource);message(tr8("当前场景不可用。"),true);return;}
    StageModel next=initial?*initial:StageModel{};if(!initial){obs_video_info video{};if(obs_get_video_info(&video)){next.width=video.base_width;next.height=video.base_height;}}
    auto*settings=modelData(next);const auto name=uniqueName("Spatial FX Stage");auto*source=obs_source_create(stageId,name.c_str(),settings,nullptr);obs_data_release(settings);
    if(source){auto*item=obs_scene_add(scene,source);if(item){obs_sceneitem_set_order(item,OBS_ORDER_MOVE_TOP);stageUuid=QString::fromUtf8(obs_source_get_uuid(source));obs_frontend_save();}obs_source_release(source);}
    obs_source_release(sceneSource);refreshScene(true);message(source?tr8("空间舞台已建立；选择原有源后导入。") :tr8("无法创建舞台。"),!source);
}
void SpatialDock::importLayers(bool selected){
    if(stageUuid.isEmpty()){createStage();if(stageUuid.isEmpty()){message(tr8("无法创建空间舞台。"),true);return;}}
    auto*sceneSource=obs_frontend_get_current_scene();auto*scene=sceneSource?obs_scene_from_source(sceneSource):nullptr;if(!scene){obs_source_release(sceneSource);return;}
    size_t total=0;obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t*item,void*p){auto*s=obs_sceneitem_get_source(item);if(std::string(obs_source_get_unversioned_id(s))!=stageId)++*static_cast<size_t*>(p);return true;},&total);
    struct Context {StageModel *model;bool selected;size_t added=0,skipped=0,index=0,total=0;std::string sceneUuid;};
    Context ctx{&model,selected,0,0,0,total,obs_source_get_uuid(sceneSource)};
    obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t*item,void*opaque){
        auto&ctx=*static_cast<Context*>(opaque);auto*source=obs_sceneitem_get_source(item);const std::string id=obs_source_get_unversioned_id(source);if(id==stageId)return true;
        const size_t rank=ctx.index++;if(ctx.selected&&!obs_sceneitem_selected(item))return true;
        if(id=="scene"||id=="group"||obs_source_get_type(source)!=OBS_SOURCE_TYPE_INPUT||!(obs_source_get_output_flags(source)&OBS_SOURCE_VIDEO)){++ctx.skipped;return true;}
        const std::string uuid=obs_source_get_uuid(source);const auto itemId=obs_sceneitem_get_id(item);
        int instanceNumber=1;bool originalVisible=obs_sceneitem_visible(item);for(const auto&existing:ctx.model->layers)if(existing.sourceUuid==uuid&&existing.itemId==itemId&&existing.sceneUuid==ctx.sceneUuid){instanceNumber=std::max(instanceNumber,existing.instanceNumber+1);originalVisible=existing.originalVisible;}
        if(ctx.model->layers.size()>=32){++ctx.skipped;return true;}
        obs_transform_info transform{};obs_sceneitem_get_info2(item,&transform);obs_sceneitem_crop crop{};obs_sceneitem_get_crop(item,&crop);
        const uint32_t actualW=obs_source_get_width(source),actualH=obs_source_get_height(source);
        // A hidden or not-yet-ready WGC source can report 0x0. Keep the association
        // and activate it through the stage; capture starts when a window becomes available.
        const uint32_t w=actualW?actualW:ctx.model->width,h=actualH?actualH:ctx.model->height;
        Layer l;l.sourceUuid=uuid;l.sceneUuid=ctx.sceneUuid;l.itemId=itemId;l.label=obs_source_get_name(source);l.originalVisible=originalVisible;l.instanceNumber=instanceNumber;l.instanceId=QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
        l.cropLeft=crop.left;l.cropRight=crop.right;l.cropTop=crop.top;l.cropBottom=crop.bottom;
        l.boundsWidth=transform.bounds.x;l.boundsHeight=transform.bounds.y;l.boundsType=int(transform.bounds_type);
        l.followSourceSize=true;l.importWidth=w;l.importHeight=h;auto&t=l.importTransform;
        t.scaleX=transform.scale.x;t.scaleY=transform.scale.y;t.boundsWidth=l.boundsWidth;t.boundsHeight=l.boundsHeight;t.boundsType=l.boundsType;t.alignment=int(transform.alignment);t.boundsAlignment=int(transform.bounds_alignment);t.cropToBounds=transform.crop_to_bounds;
        t.left=crop.left;t.right=crop.right;t.top=crop.top;t.bottom=crop.bottom;
        const auto layout=sourceLayout(w,h,t);const auto offset=rotate({layout.x,layout.y,0},{0,0,transform.rot});
        const double cw=layout.width,ch=layout.height,scaleX=layout.scaleX,scaleY=layout.scaleY;
        const double screenX=transform.pos.x+offset.x,screenY=transform.pos.y+offset.y;
        const double depth=double(ctx.total-rank-1)*180;
        l.plane=importPlane(screenX,screenY,cw,ch,scaleX,scaleY,transform.rot,depth,ctx.model->camera,ctx.model->width,ctx.model->height);
        l.defaultPlane=l.plane;l.hasDefault=true;l.depthOrder=int(ctx.model->layers.size());
        ctx.model->layers.push_back(l);obs_sceneitem_set_visible(item,false);++ctx.added;return true;
    },&ctx);
    obs_source_release(sceneSource);syncLayers();syncV2();if(ctx.added&&editor){editor->selectLayer(int(model.layers.size())-1);editor->seek(0);}commit();refreshSources();message(QString::fromUtf8("已导入 %1 项；跳过 %2 项。支持窗口捕捉、裁剪、边界与镜像；未就绪源将自动等待。").arg(ctx.added).arg(ctx.skipped));
}
void SpatialDock::restoreLayer(const Layer&layer){
    auto*sceneSource=obs_get_source_by_uuid(layer.sceneUuid.c_str());if(!sceneSource)return;auto*scene=obs_scene_from_source(sceneSource);
    if(scene){auto*item=obs_scene_find_sceneitem_by_id(scene,layer.itemId);if(item&&std::string(obs_source_get_uuid(obs_sceneitem_get_source(item)))==layer.sourceUuid)obs_sceneitem_set_visible(item,layer.originalVisible);}
    obs_source_release(sceneSource);
}
void SpatialDock::removeLayer(){int row=selectedLayer();if(row<0||size_t(row)>=model.layers.size())return;const auto removed=model.layers[size_t(row)];model.layers.erase(model.layers.begin()+row);if(std::none_of(model.layers.begin(),model.layers.end(),[&](const Layer&l){return sameSourceItem(l,removed);}))restoreLayer(removed);syncLayers();syncV2();commit();}
void SpatialDock::confirmRemoveLayer(){
    const int row=selectedLayer();if(row<0||size_t(row)>=model.layers.size())return;
    const auto target=model.layers[size_t(row)];const auto stage=stageUuid;
    QMessageBox box(QMessageBox::Question,tr8("确认移除对象"),tr8("从时间轴移除“")+QString::fromStdString(layerDisplayName(target))+tr8("”及其动画？\n其他实例保留；原 OBS 素材不会删除。"),QMessageBox::Yes|QMessageBox::Cancel,this);
    box.setObjectName("spatial.confirm-remove-object33");box.setDefaultButton(QMessageBox::Cancel);box.setEscapeButton(QMessageBox::Cancel);
    if(box.exec()!=QMessageBox::Yes||stageUuid!=stage)return;
    const auto it=std::find_if(model.layers.begin(),model.layers.end(),[&](const Layer&l){return sameLayerInstance(l,target);});
    if(it==model.layers.end())return;
    editor->selectLayer(int(it-model.layers.begin()));removeLayer();
}
void SpatialDock::restoreAll(){for(const auto&l:model.layers)restoreLayer(l);model.layers.clear();syncLayers();syncV2();commit();message(tr8("原图层可见状态已还原，舞台列表已清空。"));}
QString SpatialDock::configFile(const char *name){char*path=obs_module_config_path(name);QString file=path?QString::fromUtf8(path):QString();bfree(path);if(!file.isEmpty())QDir().mkpath(QFileInfo(file).absolutePath());return file;}
namespace { void cameraTrack(StageModel&m,const Camera&a,const Camera&b,double end){
 m.cameraAnimation.clear();const double av[]={a.position.x,a.position.y,a.position.z,a.rotation.x,a.rotation.y,a.rotation.z,a.fov},bv[]={b.position.x,b.position.y,b.position.z,b.rotation.x,b.rotation.y,b.rotation.z,b.fov};const char*ids[]={"x","y","z","rx","ry","rz","fov"};for(size_t i=0;i<7;++i){auto&c=m.cameraAnimation[ids[i]];c.enabled=true;AnimKey first{0,av[i]};first.interpolation=Interpolation::EaseBoth;c.put(first);c.put({toTime(end),bv[i]});}}
}
void SpatialDock::createDemo(){
    const QString folder=configFile("demo/");if(folder.isEmpty()||!QDir().mkpath(folder)){message(tr8("无法建立演示资源目录。"),true);return;}
    const auto name=uniqueName("Spatial FX Demo");auto*scene=obs_scene_create(name.c_str());if(!scene){message(tr8("无法创建演示场景。"),true);return;}
    StageModel demo;obs_video_info video{};if(obs_get_video_info(&video)){demo.width=video.base_width;demo.height=video.base_height;}
    const QColor colors[]={QColor("#25c4be"),QColor("#e3b35f"),QColor("#a88af3")};const QString titles[]={"SPACE","LIGHT","MOTION"};const Vec3 positions[]={{-350,-90,80},{310,100,220},{-30,-170,450}};
    for(int i=0;i<3;++i){
        QImage image(640,360,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);QPainter p(&image);p.setRenderHint(QPainter::Antialiasing);QLinearGradient gradient(0,0,640,360);gradient.setColorAt(0,colors[i].lighter(110));gradient.setColorAt(1,colors[i].darker(165));p.setBrush(gradient);p.setPen(QPen(QColor(255,255,255,100),2));p.drawRoundedRect(QRectF(8,8,624,344),26,26);
        p.setPen(QColor(255,255,255,30));for(int x=40;x<640;x+=40)p.drawLine(x,20,x,340);for(int y=40;y<360;y+=40)p.drawLine(20,y,620,y);
        QFont font("Segoe UI",48,QFont::Bold);p.setFont(font);p.setPen(Qt::white);p.drawText(QRect(40,95,560,100),Qt::AlignLeft|Qt::AlignVCenter,titles[i]);font.setPointSize(13);font.setBold(false);p.setFont(font);p.drawText(QRect(44,220,550,40),Qt::AlignLeft|Qt::AlignVCenter,"OBS SPATIAL FX / DEPTH "+QString::number(int(positions[i].z)));font.setPointSize(11);p.setFont(font);p.drawText(44,48,"0"+QString::number(i+1)+"  /  NATIVE COMPOSITOR");p.end();
        const QString file=QDir(folder).filePath(QString("card-%1.png").arg(i));if(!image.save(file)){message(tr8("演示图像写入失败。"),true);continue;}
        auto*settings=obs_data_create();obs_data_set_string(settings,"file",file.toUtf8().constData());const auto sourceName=uniqueName("Spatial FX / "+titles[i]);auto*source=obs_source_create("image_source",sourceName.c_str(),settings,nullptr);obs_data_release(settings);
        if(source){auto*item=obs_scene_add(scene,source);if(item){Layer l;l.sourceUuid=obs_source_get_uuid(source);l.sceneUuid=obs_source_get_uuid(obs_scene_get_source(scene));l.itemId=obs_sceneitem_get_id(item);l.label=sourceName;l.plane.position=positions[i];l.plane.rotation={double(i==0?-6:6),double(i==1?18:-12),double(i==2?-4:2)};l.defaultPlane=l.plane;l.hasDefault=true;demo.layers.push_back(l);obs_sceneitem_set_visible(item,false);}obs_source_release(source);}
    }
#if SPATIAL_PHASE >= 3
    Keyframe first;first.time=0;first.camera=demo.camera;Keyframe last=first;last.time=6;last.camera.position.x=250;last.camera.position.z=-1200;last.camera.rotation.y=-5;cameraTrack(demo,first.camera,last.camera,6);demo.loop=true;
#endif
#if SPATIAL_PHASE >= 4
    demo.light.enabled=true;demo.light.ambient=.75;demo.light.intensity=1.8;
#endif
    obs_frontend_set_current_scene(obs_scene_get_source(scene));createStage(&demo);obs_scene_release(scene);message(tr8("三层演示已建立；调整摄像机 X / Z 观察视差。"));
}
void SpatialDock::loadPresets(){
    if(!presetChoice)return;
    QString previous=presetChoice->currentText();presetChoice->clear();presetChoice->addItem(tr8("默认空间"),"builtin-default");presetChoice->addItem(tr8("平滑推镜"),"builtin-push");presetChoice->addItem(tr8("环绕视差"),"builtin-orbit");
    QFile file(configFile("presets.json"));if(file.open(QIODevice::ReadOnly)){auto root=QJsonDocument::fromJson(file.readAll()).object();for(const auto&entry:root.value("presets").toArray()){auto obj=entry.toObject();const QString name=obj.value("name").toString();if(!name.isEmpty())presetChoice->addItem(name,obj.value("settings").toObject());}}
    int index=presetChoice->findText(previous);if(index>=0)presetChoice->setCurrentIndex(index);
}
void SpatialDock::savePreset(){
    if(!presetChoice||!presetName)return;
    if(stageUuid.isEmpty()){message(tr8("请先创建舞台。"),true);return;}const QString name=presetName->text().trimmed();if(name.isEmpty()){message(tr8("请输入预设名称。"),true);return;}
    auto*settingsData=modelData(model);auto settings=QJsonDocument::fromJson(obs_data_get_json(settingsData)).object();obs_data_release(settingsData);QJsonArray layout;
    for(const auto&entry:settings.value("layers").toArray()){
        auto layer=entry.toObject();auto stack=layer.value("fx_stack").toObject();auto effects=stack.value("effects").toArray();
        // Auxiliary FX relationships travel with reusable layer slots, never another stage's IDs.
        for(qsizetype j=0;j<effects.size();++j){auto effect=effects[j].toObject();const auto reference=effect.value("reference").toString();int slot=-1;if(!reference.isEmpty())for(size_t i=0;i<model.layers.size();++i)if(reference==QString::fromStdString(model.layers[i].instanceId)){slot=int(i);break;}effect.insert("reference_layer",slot);effect.remove("reference");effects[j]=effect;}
        if(layer.contains("fx_stack")){stack.insert("effects",effects);layer.insert("fx_stack",stack);}
        for(const char*k:{"uuid","scene_uuid","item_id","label","visible","original_visible","instance_id","instance_number"})layer.remove(k);layout.append(layer);
    }settings.insert("layout",layout);for(const char*k:{"layers","width","height","play_token","playing"})settings.remove(k);
    QMap<QString,QString> presetSlots;for(size_t i=0;i<model.layers.size();++i)presetSlots[QString::fromStdString(model.layers[i].instanceId)]=QString("@spatial-layer-slot:%1").arg(i);for(const char*k:{"presets_geometries","presets_motion_blocks","presets_instances"})if(settings.contains(k))settings[k]=remapPresetIds(settings[k],presetSlots);
    // Presets are reusable layouts: encode light relationships by layer slot, never foreign UUID.
    auto encodeLight=[&](QJsonObject &light){
        for(bool receiver:{false,true}){const char*key=receiver?"receiver_uuid":"light_bind";const char*item=receiver?"receiver_item":"light_bind_item";const char*scene=receiver?"receiver_scene":"light_bind_scene";const char*instance=receiver?"receiver_instance":"light_bind_instance";int index=-1;
            for(size_t i=0;i<model.layers.size();++i){const auto&l=model.layers[i];if(QString::fromStdString(l.sourceUuid)==light.value(key).toString()&&(light.value(item).toInteger()==0||light.value(item).toInteger()==l.itemId)&&(light.value(scene).toString().isEmpty()||light.value(scene).toString()==QString::fromStdString(l.sceneUuid))&&(light.value(instance).toString().isEmpty()?l.instanceNumber==1:light.value(instance).toString()==QString::fromStdString(l.instanceId))){index=int(i);break;}}
            light.insert(QString(key)+"_layer",index);for(const char*field:{key,item,scene,instance})light.remove(field);
        }
    };
    encodeLight(settings);auto extra=settings.value("extra_lights").toArray();for(qsizetype i=0;i<extra.size();++i){auto light=extra[i].toObject();encodeLight(light);extra[i]=light;}settings.insert("extra_lights",extra);
    QFile existing(configFile("presets.json"));QJsonArray entries;if(existing.open(QIODevice::ReadOnly))entries=QJsonDocument::fromJson(existing.readAll()).object().value("presets").toArray();existing.close();QJsonArray result;for(const auto&e:entries)if(e.toObject().value("name").toString()!=name)result.append(e);result.append(QJsonObject{{"name",name},{"settings",settings}});
    QSaveFile file(configFile("presets.json"));if(!file.open(QIODevice::WriteOnly)||file.write(QJsonDocument(QJsonObject{{"version",1},{"presets",result}}).toJson())<0||!file.commit()){message(tr8("预设保存失败。"),true);return;}loadPresets();presetChoice->setCurrentIndex(presetChoice->findText(name));message(tr8("预设已原子写入 OBS 配置目录。"));blog(LOG_INFO,"[SpatialFX] preset saved: %s",name.toUtf8().constData());
}
void SpatialDock::applyPreset(){
    if(!presetChoice)return;
    if(stageUuid.isEmpty()){message(tr8("请先创建舞台。"),true);return;}const QVariant selected=presetChoice->currentData();if(presetPanel)presetPanel->cancelCurrentPreview();const StageModel before=model;
    if(selected.metaType().id()==QMetaType::QString){const QString builtin=selected.toString();model.camera={};model.cameraAnimation.clear();model.cameraOrbit={};model.compositeAnimation.clear();model.outputScaleX=model.outputScaleY=model.outputOpacity=1;model.fx={};model.light={};model.extraLights.clear();model.lookAtTarget=false;model.target={};if(builtin!="builtin-default"){Keyframe a;a.time=0;a.camera=model.camera;Keyframe b=a;b.time=6;if(builtin=="builtin-push"){b.camera.position.z=-1050;}else{a.camera.position.x=-250;a.camera.rotation.y=5;b.camera.position.x=250;b.camera.rotation.y=-5;}model.camera=a.camera;cameraTrack(model,a.camera,b.camera,6);}}
    else {const auto settings=selected.toJsonObject();const auto json=QJsonDocument(settings).toJson(QJsonDocument::Compact);auto*settingsData=obs_data_create_from_json(json.constData());auto incoming=readModel(settingsData);obs_data_release(settingsData);model.camera=incoming.camera;model.cameraAnimation=incoming.cameraAnimation;model.cameraOrbit=incoming.cameraOrbit;model.compositeAnimation=incoming.compositeAnimation;model.outputScaleX=incoming.outputScaleX;model.outputScaleY=incoming.outputScaleY;model.outputOpacity=incoming.outputOpacity;model.light=incoming.light;model.extraLights=incoming.extraLights;model.fx=incoming.fx;model.loop=incoming.loop;model.target=incoming.target;model.lookAtTarget=incoming.lookAtTarget;model.animationSpeed=incoming.animationSpeed;model.animationDuration=incoming.animationDuration;model.focusDistance=incoming.focusDistance;model.aperture=incoming.aperture;
        const auto layout=settings.value("layout").toArray();for(int i=0;i<std::min(layout.size(),qsizetype(model.layers.size()));++i){auto item=layout[i].toObject();
            auto stack=item.value("fx_stack").toObject();auto effects=stack.value("effects").toArray();
            for(qsizetype j=0;j<effects.size();++j){auto effect=effects[j].toObject();const qint64 slot=effect.value("reference_layer").toInteger(-1);effect.insert("reference",slot>=0&&quint64(slot)<model.layers.size()?QString::fromStdString(model.layers[size_t(slot)].instanceId):QString());effect.remove("reference_layer");effects[j]=effect;}
            if(item.contains("fx_stack")){stack.insert("effects",effects);item.insert("fx_stack",stack);}
            item.insert("uuid",QString::fromStdString(model.layers[size_t(i)].sourceUuid));QJsonObject wrapper{{"schema_version",settings.value("schema_version")},{"layers",QJsonArray{item}}};const auto bytes=QJsonDocument(wrapper).toJson();auto*d=obs_data_create_from_json(bytes.constData());auto sanitized=readModel(d);obs_data_release(d);if(!sanitized.layers.empty()){auto &current=model.layers[size_t(i)];const auto identity=current;current=sanitized.layers[0];current.sourceUuid=identity.sourceUuid;current.sceneUuid=identity.sceneUuid;current.itemId=identity.itemId;current.instanceId=identity.instanceId;current.instanceNumber=identity.instanceNumber;current.label=identity.label;current.originalVisible=identity.originalVisible;current.visible=identity.visible;current.defaultOpacity=identity.defaultOpacity;current.defaultPlane=identity.defaultPlane;current.hasDefault=identity.hasDefault;
            // Crop, bounds and readiness anchors describe this input, not a reusable spatial layout.
            current.followSourceSize=identity.followSourceSize;current.importWidth=identity.importWidth;current.importHeight=identity.importHeight;current.importTransform=identity.importTransform;
            current.cropLeft=identity.cropLeft;current.cropRight=identity.cropRight;current.cropTop=identity.cropTop;current.cropBottom=identity.cropBottom;current.boundsWidth=identity.boundsWidth;current.boundsHeight=identity.boundsHeight;current.boundsType=identity.boundsType;}}
        auto remap=[&](Light&light,const QJsonObject&presetData){auto field=[&](const char*key,std::string&uuid,std::string&scene,int64_t&item,std::string&instance){const QString slot=QString(key)+"_layer";if(presetData.contains(slot)){const int i=presetData.value(slot).toInt(-1);if(i>=0&&size_t(i)<model.layers.size()){const auto&l=model.layers[size_t(i)];uuid=l.sourceUuid;scene=l.sceneUuid;item=l.itemId;instance=l.instanceId;}else{uuid.clear();scene.clear();item=0;instance.clear();}}else if(!uuid.empty()&&std::none_of(model.layers.begin(),model.layers.end(),[&](const Layer&l){return l.sourceUuid==uuid;})){uuid.clear();instance.clear();}};field("light_bind",light.bindUuid,light.bindSceneUuid,light.bindItemId,light.bindInstanceId);field("receiver_uuid",light.receiverUuid,light.receiverSceneUuid,light.receiverItemId,light.receiverInstanceId);};
        remap(model.light,settings);const auto extra=settings.value("extra_lights").toArray();for(size_t i=0;i<model.extraLights.size()&&i<size_t(extra.size());++i)remap(model.extraLights[i],extra[int(i)].toObject());}

    if(selected.metaType().id()!=QMetaType::QString){const auto original=selected.toJsonObject();QMap<QString,QString>ids;for(size_t i=0;i<model.layers.size();++i)ids[QString("@spatial-layer-slot:%1").arg(i)]=QString::fromStdString(model.layers[i].instanceId);for(const char*k:{"presets_geometries","presets_motion_blocks","presets_instances"})for(auto v:original[k].toArray()){const auto old=v.toObject()["instance_id"].toString();if(!old.isEmpty())ids[old]=QUuid::createUuid().toString(QUuid::WithoutBraces);}auto*d=modelData(model);auto settings=QJsonDocument::fromJson(obs_data_get_json(d)).object();obs_data_release(d);for(const char*k:{"presets_geometries","presets_motion_blocks","presets_instances"})settings[k]=remapPresetIds(original[k],ids);const auto bytes=QJsonDocument(settings).toJson(QJsonDocument::Compact);d=obs_data_create_from_json(bytes.constData());auto incoming=readModel(d);obs_data_release(d);model.geometries=std::move(incoming.geometries);model.motionBlocks=std::move(incoming.motionBlocks);model.presetInstances=std::move(incoming.presetInstances);}
    model.animationDuration=toSeconds(contentEnd(model));model.playing=false;model.timelinePreview=true;model.timelinePosition=0;++model.playToken;const StageModel next=model;model=before;editor->applyDocument(next);syncControls();message(tr8("预设已应用；图层 UUID 关联保留。"));
}
#endif


