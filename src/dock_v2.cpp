#include "dock.hpp"
#include "editor.hpp"
#if SPATIAL_PHASE >= 2
#include <obs-frontend-api.h>
#include <QCheckBox>
#include <QComboBox>
#include <QColor>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <algorithm>
#include <cmath>
using namespace spatial;
namespace {
QString u(const char*s){return QString::fromUtf8(s);}
void set(QDoubleSpinBox*f,double v){if(f){QSignalBlocker b(f);f->setValue(v);if(auto*row=dynamic_cast<NumericSlider*>(f->parentWidget()))row->refresh();}}
void set(QCheckBox*f,bool v){if(f){QSignalBlocker b(f);f->setChecked(v);}}
}
Light& SpatialDock::selectedLight(){int i=lightChoice?lightChoice->currentIndex():0;return i>0&&size_t(i)<=model.extraLights.size()?model.extraLights[size_t(i-1)]:model.light;}
void SpatialDock::initV2(QFormLayout*imports,QFormLayout*layers,QFormLayout*lights){
    sceneChoice=new QComboBox;sceneChoice->setObjectName("spatial.scene-choice");imports->insertRow(0,u("当前 OBS 场景"),sceneChoice);
    sourceList=new QListWidget;sourceList->setObjectName("spatial.scene-sources");sourceList->setMinimumHeight(180);sourceList->setMaximumHeight(240);sourceList->setSpacing(2);sourceList->setToolTip(u("Ctrl / Shift可多选；双击导入。可将同一素材多次导入成独立运动的舞台实例。"));imports->insertRow(1,sourceList);
    connect(sceneChoice,&QComboBox::currentIndexChanged,this,[this]{if(loading)return;auto*s=obs_get_source_by_uuid(sceneChoice->currentData().toString().toUtf8().constData());if(s){obs_frontend_set_current_scene(s);obs_source_release(s);refreshScene(true);}});
    sourceList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    connect(sourceList,&QListWidget::itemSelectionChanged,this,[this]{if(loading)return;auto*source=obs_frontend_get_current_scene();auto*scene=source?obs_scene_from_source(source):nullptr;std::vector<qlonglong> ids;for(auto*i:sourceList->selectedItems())ids.push_back(i->data(Qt::UserRole).toLongLong());if(scene)obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t*item,void*opaque){const auto&v=*static_cast<std::vector<qlonglong>*>(opaque);obs_sceneitem_select(item,std::find(v.begin(),v.end(),obs_sceneitem_get_id(item))!=v.end());return true;},&ids);obs_source_release(source);});
    connect(sourceList,&QListWidget::itemDoubleClicked,this,[this]{importLayers(true);});
    layerName=new QLineEdit;layerName->setObjectName("spatial.layer-name");layers->addRow(u("素材名称"),layerName);
    connect(layerName,&QLineEdit::editingFinished,this,[this]{int r=selectedLayer();if(loading||r<0||size_t(r)>=model.layers.size())return;auto&l=model.layers[size_t(r)];auto*s=obs_get_source_by_uuid(l.sourceUuid.c_str());if(s&&!layerName->text().trimmed().isEmpty()){obs_source_set_name(s,layerName->text().trimmed().toUtf8().constData());l.label=obs_source_get_name(s);}obs_source_release(s);syncLayers();commit();});
    const char*labels[]={"启用此素材","受光","投影","接收阴影","轮廓光","保留默认状态"};
    const char*names[]={"spatial.layer-enabled","spatial.receive-light","spatial.cast-shadow","spatial.receive-shadow","spatial.rim-light","spatial.layer-default-marker"};
    // Five editable processing flags; default state is restored by an action, not a fake toggle.
    for(size_t i=0;i<5;++i){layerFlags[i]=new QCheckBox(u(labels[i]));layerFlags[i]->setObjectName(names[i]);layers->addRow(layerFlags[i]);connect(layerFlags[i],&QCheckBox::toggled,this,[this,i](bool v){int r=selectedLayer();if(loading||r<0||size_t(r)>=model.layers.size())return;auto&l=model.layers[size_t(r)];bool*p[]={&l.enabled,&l.receiveLight,&l.castShadow,&l.receiveShadow,&l.rimLight};*p[i]=v;commit();});}
    auto explain=[&](int index,const char*description){layerExtra[size_t(index)]->setToolTip(u(description));layerExtra[size_t(index)]->parentWidget()->setToolTip(u(description));};
    layerExtra[0]=number(layers,"同深度叠放顺序","spatial.depth-order",-1000,1000,1);
    explain(0,"仅在距相机深度相同时决定遮挡：数值越大越靠前。不同深度请调整 Z；本项不会推近或拉远图层。");
    layerExtra[1]=number(layers,"透视 / 视差倍率","spatial.perspective",0,2,.05);
    explain(1,"0：深度压到参考平面；1：保留原深度；2：深度加倍。默认镜头会补偿初始大小，横移或推拉相机后更易观察差异；Z=0 时本项无变化。");
    layerExtra[2]=number(layers,"景深模糊倍率","spatial.focus-blur",0,4,.1);
    explain(2,"0：本层不模糊；1：正常；更大：离焦时更模糊。需开启光影处理，并在FX页设置景深强度 > 0；位于焦平面时仍清晰。默认景深强度为 0。");
    for(size_t i=0;i<3;++i)connect(layerExtra[i],&QDoubleSpinBox::valueChanged,this,[this,i](double v){int r=selectedLayer();if(loading||r<0||size_t(r)>=model.layers.size())return;auto&l=model.layers[size_t(r)];if(i==0)l.depthOrder=int(v);else if(i==1)l.perspective=v;else l.focusBlur=v;commit();});
    auto*reset=new QPushButton(u("一键恢复选中素材默认状态"));reset->setObjectName("spatial.reset-layer");layers->addRow(reset);
    connect(reset,&QPushButton::clicked,this,[this]{if(editor){editor->selectLayer(selectedLayer());editor->reset(false);}});
    lightChoice=new QComboBox;lightChoice->setObjectName("spatial.light-choice");lights->insertRow(0,u("灯光列表"),lightChoice);connect(lightChoice,&QComboBox::currentIndexChanged,this,[this]{if(!loading)syncLight();});
    auto*addRow=new QHBoxLayout;for(int i=0;i<2;++i){auto*b=new QPushButton(u(i?"添加面光源":"添加点光源"));b->setObjectName(i?"spatial.add-area-light":"spatial.add-point-light");addRow->addWidget(b);connect(b,&QPushButton::clicked,this,[this,i]{addLight(i);});}lights->insertRow(1,addRow);
    auto*removeLight=new QPushButton(u("删除选中灯光"));removeLight->setObjectName("spatial.remove-light");addRow->addWidget(removeLight);addRow->setSpacing(4);for(int i=0;i<addRow->count();++i)addRow->itemAt(i)->widget()->setStyleSheet("QPushButton { padding: 2px 4px; min-height: 24px; }");
    connect(removeLight,&QPushButton::clicked,this,[this]{int i=lightChoice->currentIndex();if(i>0&&size_t(i)<=model.extraLights.size())model.extraLights.erase(model.extraLights.begin()+i-1);else if(!model.extraLights.empty()){model.light=model.extraLights.front();model.extraLights.erase(model.extraLights.begin());}else model.light={};syncV2();commit();});
    lightVisible=new QCheckBox(u("显示灯光实体"));lightVisible->setObjectName("spatial.light-visible");lightVisible->setToolTip(u("在画面中显示光源的发光实体，独立于照明启用开关。灯光启用决定是否照亮素材；此项只改变灯具本身是否可见。"));lights->addRow(lightVisible);connect(lightVisible,&QCheckBox::toggled,this,[this](bool v){if(!loading){selectedLight().visible=v;commit();}});
    const char*extraLabels[]={"灯光尺寸 / 世界单位","距离衰减指数","软阴影半径","轮廓光强度"};const char*extraNames[]={"spatial.light-size","spatial.light-falloff","spatial.light-softness","spatial.light-rim"};
    for(size_t i=0;i<4;++i){lightExtra[i]=number(lights,extraLabels[i],extraNames[i],i==1?.1:0,i==0?2000:(i==1?8:(i==2?300:5)),i==0||i==2?5:.1);connect(lightExtra[i],&QDoubleSpinBox::valueChanged,this,[this,i](double v){if(loading)return;auto&l=selectedLight();double*p[]={&l.size,&l.falloff,&l.softness,&l.rim};*p[i]=v;commit();});}
    lightBind=new QComboBox;lightBind->setObjectName("spatial.light-bind");lightBind->setToolTip(u("绑定后位置XYZ表示相对于所选图层的局部偏移。图层平移、旋转、缩放和动画都会带动光源；不绑定时使用世界坐标。"));lights->addRow(u("灯光绑定素材"),lightBind);
    receiverChoice=new QComboBox;receiverChoice->setObjectName("spatial.receiver-choice");lights->addRow(u("底层受光 / 阴影接收层"),receiverChoice);
    connect(lightBind,&QComboBox::currentIndexChanged,this,[this]{if(!loading){auto&l=selectedLight();l.bindUuid=lightBind->currentData().toString().toStdString();l.bindSceneUuid=lightBind->currentData(Qt::UserRole+2).toString().toStdString();l.bindItemId=lightBind->currentData(Qt::UserRole+1).toLongLong();l.bindInstanceId=lightBind->currentData(Qt::UserRole+3).toString().toStdString();commit();}});
    connect(receiverChoice,&QComboBox::currentIndexChanged,this,[this]{if(!loading){auto&l=selectedLight();l.receiverUuid=receiverChoice->currentData().toString().toStdString();l.receiverSceneUuid=receiverChoice->currentData(Qt::UserRole+2).toString().toStdString();l.receiverItemId=receiverChoice->currentData(Qt::UserRole+1).toLongLong();l.receiverInstanceId=receiverChoice->currentData(Qt::UserRole+3).toString().toStdString();commit();}});
}
void SpatialDock::syncLayerV2(){
    if(!layerName)return;const bool was=loading;loading=true;int r=selectedLayer();bool ok=r>=0&&size_t(r)<model.layers.size();
    layerName->setEnabled(ok);for(size_t i=0;i<5;++i)layerFlags[i]->setEnabled(ok);for(auto*f:layerExtra)f->setEnabled(ok);
    if(ok){const auto&l=model.layers[size_t(r)];QSignalBlocker b(layerName);layerName->setText(QString::fromUtf8(l.label.c_str()));const bool flags[]={l.enabled,l.receiveLight,l.castShadow,l.receiveShadow,l.rimLight};for(size_t i=0;i<5;++i)set(layerFlags[i],flags[i]);set(layerExtra[0],l.depthOrder);set(layerExtra[1],l.perspective);set(layerExtra[2],l.focusBlur);}loading=was;
}
void SpatialDock::syncLight(){
    if(!lightChoice)return;const bool was=loading;loading=true;const auto&l=selectedLight();
    if(lightColorPreview){const auto color=QColor::fromRgbF(float(l.color.x),float(l.color.y),float(l.color.z));lightColorPreview->setProperty("color",color);lightColorPreview->setStyleSheet(QString("background-color: %1; border: 1px solid #8893a2; border-radius: 3px;").arg(color.name()));lightColorPreview->setToolTip(u("所选灯光颜色：")+color.name().toUpper());lightColorPreview->setAccessibleName(lightColorPreview->toolTip());}
    if(shadowColorPreview){const auto color=QColor::fromRgbF(float(l.shadowColor.x),float(l.shadowColor.y),float(l.shadowColor.z));shadowColorPreview->setStyleSheet(QString("background-color:%1; border:1px solid #8893a2; border-radius:3px;").arg(color.name()));shadowColorPreview->setToolTip(u("所选阴影颜色：")+color.name().toUpper());}
    const double values[]={l.position.x,l.position.y,l.position.z,l.intensity,l.range,l.ambient,l.shadowOpacity,l.receiverZ};for(size_t i=0;i<8;++i)set(lightFields[i],values[i]);
    set(lightEnabled,l.enabled);set(shadows,l.shadows);set(lightVisible,l.visible);set(lightExtra[0],l.size);set(lightExtra[1],l.falloff);set(lightExtra[2],l.softness);set(lightExtra[3],l.rim);if(rimFields[0]){set(rimFields[0],l.rimWidth);set(rimFields[1],l.rimSoftness);}
    for(auto*combo:{lightBind,receiverChoice}){QSignalBlocker b(combo);combo->clear();combo->addItem(u(combo==lightBind?"不绑定（世界坐标）":"全部开启接收阴影的层"),"");for(const auto&layer:model.layers){combo->addItem(QString::fromStdString(layerDisplayName(layer))+QString(" [#%1]").arg(layer.itemId),QString::fromStdString(layer.sourceUuid));combo->setItemData(combo->count()-1,qlonglong(layer.itemId),Qt::UserRole+1);combo->setItemData(combo->count()-1,QString::fromStdString(layer.sceneUuid),Qt::UserRole+2);combo->setItemData(combo->count()-1,QString::fromStdString(layer.instanceId),Qt::UserRole+3);}const auto&uuid=combo==lightBind?l.bindUuid:l.receiverUuid;int selected=0;bool found=uuid.empty();for(size_t i=0;i<model.layers.size();++i)if(lightMatchesLayer(l,model.layers[i],combo==receiverChoice)){selected=int(i)+1;found=true;break;}if(!found){combo->addItem(u("[关联图层已移除，重新选择]。"),QString::fromStdString(uuid));selected=combo->count()-1;combo->setItemData(selected,qlonglong(combo==lightBind?l.bindItemId:l.receiverItemId),Qt::UserRole+1);combo->setItemData(selected,QString::fromStdString(combo==lightBind?l.bindSceneUuid:l.receiverSceneUuid),Qt::UserRole+2);combo->setItemData(combo->count()-1,QString::fromStdString(combo==lightBind?l.bindInstanceId:l.receiverInstanceId),Qt::UserRole+3);}combo->setCurrentIndex(selected);}
    loading=was;
}
void SpatialDock::syncV2(){
    if(!sceneChoice)return;const bool was=loading;loading=true;set(focusField,model.focusDistance);set(apertureField,model.aperture);
    const int previous=lightChoice->currentIndex();{QSignalBlocker b(lightChoice);lightChoice->clear();lightChoice->addItem(QString::fromUtf8(model.light.label.c_str())+u(model.light.type?" / 面光":" / 点光"));for(const auto&l:model.extraLights)lightChoice->addItem(QString::fromUtf8(l.label.c_str())+u(l.type?" / 面光":" / 点光"));lightChoice->setCurrentIndex(std::clamp(previous,0,lightChoice->count()-1));}
    loading=was;syncLight();syncLayerV2();refreshSources();
}
void SpatialDock::refreshSources(){
    if(!sceneChoice)return;const bool was=loading;loading=true;obs_frontend_source_list scenes{};obs_frontend_get_scenes(&scenes);{QSignalBlocker b(sceneChoice);sceneChoice->clear();for(size_t i=0;i<scenes.sources.num;++i){auto*s=scenes.sources.array[i];sceneChoice->addItem(QString::fromUtf8(obs_source_get_name(s)),QString::fromUtf8(obs_source_get_uuid(s)));}sceneChoice->setCurrentIndex(sceneChoice->findData(sceneUuid));}obs_frontend_source_list_free(&scenes);
    auto*sceneSource=obs_frontend_get_current_scene();auto*scene=sceneSource?obs_scene_from_source(sceneSource):nullptr;
    struct Entry{qlonglong id;QString name;};std::vector<Entry> entries;
    if(scene)obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t*i,void*p){auto*s=obs_sceneitem_get_source(i);if(std::string(obs_source_get_unversioned_id(s))!=stageId)static_cast<std::vector<Entry>*>(p)->push_back({obs_sceneitem_get_id(i),QString::fromUtf8(obs_source_get_name(s))+(obs_sceneitem_visible(i)?"":" [原项隐藏]")});return true;},&entries);
    bool changed=sourceList->count()!=int(entries.size());for(int i=0;!changed&&i<sourceList->count();++i)changed=sourceList->item(i)->text()!=entries[size_t(i)].name||sourceList->item(i)->data(Qt::UserRole).toLongLong()!=entries[size_t(i)].id;
    if(changed){QSignalBlocker b(sourceList);sourceList->clear();for(const auto&e:entries){auto*i=new QListWidgetItem(e.name,sourceList);i->setData(Qt::UserRole,e.id);}}
    obs_source_release(sceneSource);loading=was;
}
void SpatialDock::addLight(int type){if(model.extraLights.size()>=3){message(u("最多支持 4 个灯光。"),true);return;}Light light;light.enabled=true;light.type=type;light.ambient=0;light.label=std::string(type?"面光源 ":"点光源 ")+std::to_string(model.extraLights.size()+2);model.extraLights.push_back(light);syncV2();lightChoice->setCurrentIndex(int(model.extraLights.size()));syncLight();commit();}
#endif
