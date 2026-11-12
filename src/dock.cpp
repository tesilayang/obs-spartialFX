#if SPATIAL_PHASE < 2
#include "dock_phase1.inc"
#else
#include "dock.hpp"
#include "stage.hpp"
#include "editor.hpp"
#include "fx_panel.hpp"
#include "hover_help.hpp"
#include <QSaveFile>
#include <QFile>
#include <QSlider>
#include <obs-frontend-api.h>
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
using namespace spatial;
namespace {
QString text(const char *s){return QString::fromUtf8(s);}
QPushButton *button(QHBoxLayout *row,const char *label,const char *name){auto*b=new QPushButton(text(label));b->setObjectName(name);row->addWidget(b);return b;}
QFormLayout *page(QTabWidget *tabs,const char *label){auto*scroll=new QScrollArea;scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);auto*widget=new QWidget;auto*form=new QFormLayout(widget);form->setContentsMargins(8,12,8,8);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);scroll->setWidget(widget);tabs->addTab(scroll,text(label));return form;}
void value(QDoubleSpinBox *field,double v){const QSignalBlocker blocked(field);field->setValue(v);if(auto*row=dynamic_cast<NumericSlider*>(field->parentWidget()))row->refresh();}
}
QDoubleSpinBox *SpatialDock::number(QFormLayout *form,const char *label,const char *name,double low,double high,double){auto*f=new QDoubleSpinBox;f->setObjectName(name);f->setRange(low,high);f->setKeyboardTracking(false);auto*row=new NumericSlider(f,std::string(name)=="spatial.depth-order");
    const std::string id(name);if(id=="spatial.layer-x"||id=="spatial.layer-y"||id=="spatial.layer-z")row->setSliderRange(-2000,2000);if(id=="spatial.layer-rx"||id=="spatial.layer-ry"||id=="spatial.layer-rz")row->setSliderRange(-360,360);
    form->addRow(text(label),row);return f;}
SpatialDock::SpatialDock(QWidget *parent):QWidget(parent){
    setObjectName("spatial.dock");setMinimumWidth(440);
    auto*column=new QVBoxLayout(this);column->setContentsMargins(12,10,12,10);column->setSpacing(8);
    auto*title=new QLabel("SPATIAL FX  <span style='color:#8c9aad;font-size:10px'>3.4.2 · OBS NATIVE</span>");title->setObjectName("spatial.version");title->setStyleSheet("font-weight:700;font-size:16px;letter-spacing:2px;");column->addWidget(title);
    sceneLabel=new QLabel;sceneLabel->setObjectName("spatial.scene");sceneLabel->setWordWrap(true);column->addWidget(sceneLabel);
    auto*stageRow=new QHBoxLayout;stageChoice=new QComboBox;stageChoice->setObjectName("spatial.stage-choice");stageRow->addWidget(stageChoice,1);
    auto*create=button(stageRow,"新建舞台","spatial.create-stage");column->addLayout(stageRow);
    auto*actionRow=new QHBoxLayout;auto*refresh=button(actionRow,"刷新场景","spatial.refresh");auto*demo=button(actionRow,"创建演示","spatial.create-demo");column->addLayout(actionRow);
    fxEnabled=new QCheckBox(text("启用舞台灯光"));fxEnabled->setObjectName("spatial.fx-enabled");fxEnabled->setToolTip(text("默认关闭。开启舞台灯光、空间投影和轮廓光。FX 特效页各素材的独立开关不受此按钮影响；摄像机和图层几何动画持续有效。"));column->addWidget(fxEnabled);connect(fxEnabled,&QCheckBox::toggled,this,[this](bool enabled){if(!loading){model.fx.enabled=enabled;commit();}});
    tabs=new QTabWidget;tabs->setObjectName("spatial.tabs");tabs->setUsesScrollButtons(true);column->addWidget(tabs,1);
    editor=new CameraEditor(&model);auto*editorScroll=new QScrollArea;editorScroll->setObjectName("spatial.editor-scroll33");editorScroll->setWidgetResizable(true);editorScroll->setFrameShape(QFrame::NoFrame);editorScroll->setWidget(editor);tabs->addTab(editorScroll,text("镜头与动画"));
    auto*layerForm=editor->importForm;
    auto*imports=new QHBoxLayout;auto*importSelected=button(imports,"导入选中","spatial.import-selected");auto*importAll=button(imports,"导入场景","spatial.import-all");auto*remove=button(imports,"移除实例","spatial.remove-layer");auto*restore=button(imports,"还原全部","spatial.restore-all");imports->setSpacing(4);for(auto*b:{importSelected,importAll,remove,restore}){b->setMinimumWidth(0);b->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);b->setStyleSheet("QPushButton { padding: 2px 3px; min-height: 24px; }");}layerForm->addRow(imports);
    importSelected->setToolTip(text("选择上方场景素材后导入，支持Ctrl / Shift多选及双击。同一素材可多次导入为独立实例，共享画面但位置、动画互相独立。导入后在0秒调整初始布局。"));
    importAll->setToolTip(text("将当前场景可用视频素材直接加入时间轴，并隐藏原项避免重复显示。导入后自动暂停到0秒、选中新图层；移除可恢复原项。"));
    remove->setToolTip(text("仅移除时间轴选中的实例和它的动画。若还有同一OBS原项的其他实例，原项继续隐藏；最后一个实例移除时还原原项导入前的可见性。"));
    restore->setToolTip(text("将所有已导入图层移出管理，恢复各OBS原项导入前的可见性。对应图层动画会随管理项一并移除。"));
    layerVisible=new QCheckBox(text("显示此空间图层"));layerVisible->setObjectName("spatial.layer-visible");editor->layerPropertiesForm->addRow(layerVisible);
    auto*lightForm=page(tabs,"灯光");lightEnabled=new QCheckBox(text("启用所选灯光"));lightEnabled->setObjectName("spatial.light-enabled");lightForm->addRow(lightEnabled);
    const char*lightLabels[]={"光源 X","光源 Y","光源 Z","强度","范围","环境光","阴影不透明度","接收平面 Z"};const char*lightNames[]={"spatial.light-x","spatial.light-y","spatial.light-z","spatial.light-intensity","spatial.light-range","spatial.light-ambient","spatial.shadow-opacity","spatial.receiver-z"};
    for(size_t i=0;i<8;++i){lightFields[i]=number(lightForm,lightLabels[i],lightNames[i],i<3||i==7?-100000:(i==4?10:0),i<3||i==7||i==4?100000:(i==3?10:(i==5?2:1)),i<3||i==7||i==4?50:.05);connect(lightFields[i],&QDoubleSpinBox::valueChanged,this,[this,i](double v){if(loading)return;double*p[]={&selectedLight().position.x,&selectedLight().position.y,&selectedLight().position.z,&selectedLight().intensity,&selectedLight().range,&selectedLight().ambient,&selectedLight().shadowOpacity,&selectedLight().receiverZ};*p[i]=v;commit();});}
    shadows=new QCheckBox(text("素材间 Alpha 软阴影"));shadows->setObjectName("spatial.shadows");lightForm->addRow(shadows);auto*color=new QPushButton(text("选择灯光颜色"));color->setObjectName("spatial.light-color");auto*colorRow=new QHBoxLayout;colorRow->addWidget(color,1);lightColorPreview=new QLabel;lightColorPreview->setObjectName("spatial.light-color-preview33");lightColorPreview->setFixedSize(48,26);colorRow->addWidget(lightColorPreview);lightForm->addRow(colorRow);connect(color,&QPushButton::clicked,this,[this]{QColor initial=QColor::fromRgbF(float(selectedLight().color.x),float(selectedLight().color.y),float(selectedLight().color.z));auto selected=QColorDialog::getColor(initial,this,text("所选灯光颜色"));if(selected.isValid()){selectedLight().color={selected.redF(),selected.greenF(),selected.blueF()};syncLight();commit();}});
    shadows->setToolTip(text("点光 / 面光漫反射 · 素材 Alpha 投影 · 无 CUDA 依赖。\n选择真实受光层；使用灯光尺寸与软阴影调节半影。"));
    auto*shadowColor=new QPushButton(text("选择阴影颜色"));shadowColor->setObjectName("spatial.shadow-color34");auto*shadowRow=new QHBoxLayout;shadowRow->addWidget(shadowColor,1);shadowColorPreview=new QLabel;shadowColorPreview->setObjectName("spatial.shadow-color-preview34");shadowColorPreview->setFixedSize(48,26);shadowRow->addWidget(shadowColorPreview);lightForm->addRow(shadowRow);
    shadowColor->setToolTip(text("改变所选灯光被遮挡区域的透射颜色。黑色为普通遮光，彩色产生有色阴影；阴影不透明度控制强弱，软阴影控制边缘模糊。每盏灯独立计算投向其他图层的阴影。"));
    connect(shadowColor,&QPushButton::clicked,this,[this]{const auto v=selectedLight().shadowColor;auto chosen=QColorDialog::getColor(QColor::fromRgbF(float(v.x),float(v.y),float(v.z)),this,text("所选灯光阴影颜色"));if(chosen.isValid()){selectedLight().shadowColor={chosen.redF(),chosen.greenF(),chosen.blueF()};syncLight();commit();}});
    // Legacy v1 receiver-Z data remains readable; v2 receives on actual selectable materials.
    lightFields[7]->parentWidget()->hide();if(auto*label=lightForm->labelForField(lightFields[7]->parentWidget()))label->hide();
    initV2(layerForm,editor->layerPropertiesForm,lightForm);initV3(layerForm,nullptr,lightForm);
    fxPanel=new FxPanel(&model);auto*fxScroll=new QScrollArea;fxScroll->setObjectName("spatial.fx-scroll34");fxScroll->setWidgetResizable(true);fxScroll->setFrameShape(QFrame::NoFrame);fxScroll->setWidget(fxPanel);tabs->addTab(fxScroll,text("FX 特效"));fxPanel->changed=[this]{commit();};
    // Depth-of-field remains available with the shared layer inspector.
    focusField=number(editor->layerPropertiesForm,"焦平面距离","spatial.focus-distance",10,100000,50);apertureField=number(editor->layerPropertiesForm,"景深强度 / 像素","spatial.aperture",0,20,.5);
    connect(focusField,&QDoubleSpinBox::valueChanged,this,[this](double v){if(!loading){model.focusDistance=v;commit();}});
    connect(apertureField,&QDoubleSpinBox::valueChanged,this,[this](double v){if(!loading){model.aperture=v;commit();}});
    auto*presetForm=page(tabs,"预设");presetChoice=new QComboBox;presetChoice->setObjectName("spatial.preset-choice");presetForm->addRow(text("预设"),presetChoice);auto*presetApply=new QPushButton(text("应用预设"));presetApply->setObjectName("spatial.apply-preset");presetForm->addRow(presetApply);presetName=new QLineEdit(text("我的空间预设"));presetName->setObjectName("spatial.preset-name");presetName->setMaxLength(80);presetForm->addRow(text("保存名称"),presetName);auto*presetSave=new QPushButton(text("保存当前预设"));presetSave->setObjectName("spatial.save-preset");presetForm->addRow(presetSave);
    presetSave->setToolTip(text("自动保存到 OBS 配置目录。预设包含摄像机、关键帧、灯光、FX 和图层布局；跨场景按现有图层顺序应用布局，保留 UUID 关联。"));
    status=new QLabel(text("新建舞台，或创建演示开始体验。"));status->setObjectName("spatial.status");status->setWordWrap(true);column->addWidget(status);performance=new QLabel;performance->setObjectName("spatial.performance");performance->setWordWrap(true);performance->setStyleSheet("color:#92a0b5;font-size:10px;");column->addWidget(performance);
    saveTimer=new QTimer(this);saveTimer->setSingleShot(true);saveTimer->setInterval(400);connect(saveTimer,&QTimer::timeout,this,[]{obs_frontend_save();});
    connect(create,&QPushButton::clicked,this,[this]{createStage();});connect(refresh,&QPushButton::clicked,this,[this]{refreshScene(true);});connect(demo,&QPushButton::clicked,this,[this]{createDemo();});connect(stageChoice,&QComboBox::currentIndexChanged,this,[this]{if(!loading){stageUuid=stageChoice->currentData().toString();loadStage();}});
    connect(importSelected,&QPushButton::clicked,this,[this]{importLayers(true);});connect(importAll,&QPushButton::clicked,this,[this]{importLayers(false);});connect(remove,&QPushButton::clicked,this,[this]{removeLayer();});connect(restore,&QPushButton::clicked,this,[this]{restoreAll();});connect(layerVisible,&QCheckBox::toggled,this,[this](bool v){int row=selectedLayer();if(!loading&&row>=0&&size_t(row)<model.layers.size()){model.layers[size_t(row)].visible=v;commit();}});
    connect(lightEnabled,&QCheckBox::toggled,this,[this](bool v){if(!loading){selectedLight().enabled=v;commit();}});connect(shadows,&QCheckBox::toggled,this,[this](bool v){if(!loading){selectedLight().shadows=v;commit();}});connect(presetApply,&QPushButton::clicked,this,[this]{applyPreset();});connect(presetSave,&QPushButton::clicked,this,[this]{savePreset();});
    editor->clockChanged=[this]{auto*s=obs_get_source_by_uuid(stageUuid.toUtf8().constData());if(s){stageApplyClock(s,model.timelinePosition,model.playing,model.timelinePreview,model.playToken);obs_source_release(s);saveTimer->start();}syncEvaluatedFields();};editor->changed=[this]{syncEvaluatedFields();commit();};editor->layerSelected=[this](int){syncLayerFields();};editor->removeObjectRequested=[this]{confirmRemoveLayer();};editor->message=[this](QString s,bool error){message(s,error);};
    connect(tabs,&QTabWidget::currentChanged,this,[refresh,demo](int i){refresh->setVisible(i!=0);demo->setVisible(i!=0);});refresh->hide();demo->hide();installHoverHelp(this);loadPresets();auto*timer=new QTimer(this);timer->setInterval(750);connect(timer,&QTimer::timeout,this,[this]{refreshScene();updateStats();});timer->start();refreshScene(true);
}
void SpatialDock::message(const QString&s,bool error){status->setText(error?s:QString());status->setToolTip(s);status->setStyleSheet(error?"color:#ff8989;":"color:#a4c8ed;");}
void SpatialDock::commit(){
    if(fxPanel)fxPanel->refresh();if(loading||stageUuid.isEmpty())return;auto*source=obs_get_source_by_uuid(stageUuid.toUtf8().constData());if(!source)return;
    auto*settingsData=modelData(model);stageApplySettings(source,settingsData);obs_data_release(settingsData);obs_source_release(source);syncTimeline();if(structure){structure->model=model;structure->selected=selectedLayer();structure->update();}saveTimer->start();message(text("更改已应用；OBS 配置将自动保存。"));
}
void SpatialDock::loadStage(){
    if(editor&&editorStageUuid!=stageUuid){editor->resetDocument();editorStageUuid=stageUuid;}
    auto*source=stageUuid.isEmpty()?nullptr:obs_get_source_by_uuid(stageUuid.toUtf8().constData());
    if(source){auto*settingsData=stageGetSettings(source);
        model=readModel(settingsData);obs_data_release(settingsData);obs_source_release(source);}else model={};for(auto&layer:model.layers)if(!layer.hasDefault){layer.defaultPlane=layer.plane;layer.defaultOpacity=layer.opacity;layer.hasDefault=true;}syncControls();
}
void SpatialDock::syncControls(){
    if(fxPanel){fxPanel->setDocument(stageUuid);fxPanel->refresh(true);}
    loading=true;
    
    const double light[]={selectedLight().position.x,selectedLight().position.y,selectedLight().position.z,selectedLight().intensity,selectedLight().range,selectedLight().ambient,selectedLight().shadowOpacity,selectedLight().receiverZ};for(size_t i=0;i<8;++i)value(lightFields[i],light[i]);
    fxEnabled->setChecked(model.fx.enabled);lightEnabled->setChecked(selectedLight().enabled);shadows->setChecked(selectedLight().shadows);loading=false;syncLayers();syncKeys();syncV2();syncEvaluatedFields();
}
int SpatialDock::selectedLayer() const{return editor?editor->active:-1;}
void SpatialDock::syncLayers(){
    const bool was=loading;loading=true;
    for(auto&layer:model.layers){auto*source=obs_get_source_by_uuid(layer.sourceUuid.c_str());if(source&&!obs_source_removed(source))layer.label=obs_source_get_name(source);obs_source_release(source);}
    loading=was;syncTimeline();syncLayerFields();
}
void SpatialDock::syncLayerFields(){
    const int row=selectedLayer();const bool valid=row>=0&&size_t(row)<model.layers.size();const bool was=loading;loading=true;
    layerVisible->setEnabled(valid);if(valid){QSignalBlocker b(layerVisible);layerVisible->setChecked(model.layers[size_t(row)].visible);}
    loading=was;syncLayerV2();if(structure){structure->model=model;structure->selected=valid?row:-1;structure->update();}
}
void SpatialDock::syncKeys(){syncTimeline();}
void SpatialDock::updateStats(){auto*source=stageUuid.isEmpty()?nullptr:obs_get_source_by_uuid(stageUuid.toUtf8().constData());if(!source){performance->setText(text("无空间舞台 · 原生 Qt6 Dock"));return;}const auto stats=stageStats(source);obs_source_release(source);performance->setText(QString::fromUtf8("%1 图层 · %2 个复用表面 · 提交均值 %3ms\nOBS 渲染均值 %4ms · 动画 %5s · 丢失源 %6").arg(model.layers.size()).arg(stats.textureAllocations).arg(stats.renders?double(stats.renderNanoseconds)/double(stats.renders)/1e6:0,0,'f',3).arg(double(obs_get_average_frame_time_ns())/1e6,0,'f',3).arg(stats.timeline,0,'f',1).arg(stats.missing));}
#endif



