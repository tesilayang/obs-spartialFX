#include "dock.hpp"
#if SPATIAL_PHASE >= 2
#include "stage.hpp"
#include "editor.hpp"
#include "scene_evaluator.hpp"
#include <QAbstractItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
using namespace spatial;
namespace {
QString t(const char*s){return QString::fromUtf8(s);}

}
void SpatialDock::initV3(QFormLayout*,QFormLayout*,QFormLayout*lights){
    structure=new StructurePreview;
    auto*preview=new StructurePanel(structure,this);editor->previewLayout->addWidget(preview);
    structure->select=[this](int i){if(editor)editor->selectLayer(i);};
    rimFields[0]=number(lights,"轮廓光宽度 / 源像素","spatial.light-rim-width",.01,100,.01);
    rimFields[1]=number(lights,"轮廓光柔和度","spatial.light-rim-softness",0,1,.01);
    for(size_t i=0;i<2;++i)connect(rimFields[i],&QDoubleSpinBox::valueChanged,this,[this,i](double v){if(loading)return;if(i==0)selectedLight().rimWidth=v;else selectedLight().rimSoftness=v;commit();});
    auto*timer=new QTimer(this);timer->setInterval(50);connect(timer,&QTimer::timeout,this,[this]{if(!editor||!model.playing||stageUuid.isEmpty()||!presetPreviewStage.isEmpty())return;auto*s=obs_get_source_by_uuid(stageUuid.toUtf8().constData());if(!s)return;double time=stageStats(s).timeline;obs_source_release(s);editor->tick(time);syncEvaluatedFields();});timer->start();
}
void SpatialDock::syncTimeline(){if(editor)editor->refresh();}
void SpatialDock::scrub(int row,double time){if(editor){if(row>0)editor->selectLayer(row-1);editor->seek(toTime(time));}}
void SpatialDock::syncEvaluatedFields(){
    if(editor)editor->refreshValues();if(structure){structure->model=model;structure->selected=selectedLayer();structure->update();}
}
#endif
