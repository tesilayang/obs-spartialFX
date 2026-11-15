#include "editor.hpp"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
using namespace spatial;
namespace {
QString u(const char*s){return QString::fromUtf8(s);}
bool pairFor(const Curve&c,TimeUs t,AnimKey&a,AnimKey&b){
 const auto&keys=c.keys();if(keys.size()<2)return false;
 auto it=std::upper_bound(keys.begin(),keys.end(),t,[](TimeUs time,const AnimKey&k){return time<k.time;});
 if(it==keys.end())--it;if(it==keys.begin())++it;a=*(it-1);b=*it;return true;
}
}
void CameraEditor::keySelection(){
 if(syncing)return;syncing=true;const auto&animation=objectAnimation(*model,active);
 if(!activeParameter.empty()&&validParameter(active,activeParameter))curveParameter=activeParameter;
 else if(curveParameter.empty()||!validParameter(active,curveParameter)||animation.find(curveParameter)==animation.end()||animation.at(curveParameter).keys().empty()){
  curveParameter="x";size_t count=0;for(const auto&p:objectParameters(*model,active)){auto it=animation.find(p.id);if(validParameter(active,p.id)&&it!=animation.end()&&it->second.keys().size()>count){curveParameter=p.id;count=it->second.keys().size();}}
  if(!validParameter(active,curveParameter)){auto available=objectParameters(*model,active);curveParameter=available.empty()?"":available.front().id;}
 }
 {QSignalBlocker blocked(curveChoice);curveChoice->clear();for(const auto&p:objectParameters(*model,active))if(validParameter(active,p.id)){
  auto it=animation.find(p.id);bool has=it!=animation.end()&&!it->second.keys().empty();curveChoice->addItem(u(p.label.c_str())+(has?u(" ◆"):QString()),QString::fromStdString(p.id));
 }curveChoice->setCurrentIndex(curveChoice->findData(QString::fromStdString(curveParameter)));}
 auto it=animation.find(curveParameter);const Curve empty;const auto&c=it==animation.end()?empty:it->second;TimeUs desired=toTime(model->timelinePosition);
 for(const auto&ref:timeline->selection)if(ref.object==active&&ref.parameter==curveParameter&&c.contains(ref.time)){desired=ref.time;break;}
 const auto&keys=c.keys();bool found=!keys.empty();AnimKey key{desired,baseValue(*model,active,curveParameter)};
 if(found){auto k=std::upper_bound(keys.begin(),keys.end(),desired,[](TimeUs t,const AnimKey&v){return t<v.time;});key=k==keys.begin()?keys.front():*(k-1);}
 selectedKeyTime=key.time;bool editable=found&&!objectLocked(*model,active)&&!c.locked;
 keyTime->setValue(toSeconds(key.time));keyValue->setRange(objectParameter(*model,active,curveParameter)->minimum,objectParameter(*model,active,curveParameter)->maximum);keyValue->setValue(key.value);
 keyTime->setEnabled(editable&&!model->keyPositionsLocked);keyValue->setEnabled(editable);
 auto currentMeta=objectParameter(*model,active,curveParameter);curve->parameterMinimum=currentMeta?currentMeta->minimum:-100000;curve->parameterMaximum=currentMeta?currentMeta->maximum:100000;
 curve->positionsLocked=model->keyPositionsLocked;curve->readOnly=objectLocked(*model,active)||c.locked;curve->setCurve(c,key.time,curveParameter);
 AnimKey a,b;bool pair=pairFor(c,key.time,a,b);interpolation->setEnabled(pair&&editable);segmentLabel->setText(pair?QString("%1s → %2s").arg(toSeconds(a.time),0,'f',3).arg(toSeconds(b.time),0,'f',3):u("—"));
 if(pair){int mode=int(a.interpolation);if(mode==1)mode=int(Interpolation::EaseBoth);interpolation->setCurrentIndex(interpolation->findData(mode));materializeHandles(a,b,c.shortest);outX->setValue(a.outX*100);outY->setRange(objectParameter(*model,active,curveParameter)->minimum,objectParameter(*model,active,curveParameter)->maximum);outY->setValue(a.outY);inX->setValue(b.inX*100);inY->setRange(objectParameter(*model,active,curveParameter)->minimum,objectParameter(*model,active,curveParameter)->maximum);inY->setValue(b.inY);}
 for(auto*f:{outX,outY,inX,inY})f->setEnabled(pair&&editable);
 rotationMode->setVisible(curveParameter=="rx"||curveParameter=="ry"||curveParameter=="rz");rotationMode->setCurrentIndex(c.shortest?1:0);rotationMode->setEnabled(editable);
 syncing=false;
}
void CameraEditor::editCurveKey(TimeUs old,AnimKey key){
 if(curveParameter.empty()||objectLocked(*model,active))return;auto&c=objectAnimation(*model,active)[curveParameter];if(c.locked||!c.contains(old))return;
 if(model->keyPositionsLocked&&key.time!=old){keySelection();return;}
 if(key.time<0||(key.time!=old&&c.contains(key.time))){if(message)message(u("目标时间已有关键帧，已保留原帧。"),true);keySelection();return;}
 auto meta=objectParameter(*model,active,curveParameter);if(!meta)return;key.value=std::clamp((meta->discrete||meta->integer)?std::round(key.value):key.value,meta->minimum,meta->maximum);if(meta->discrete)key.interpolation=Interpolation::Hold;
 remember();c.erase(old);c.put(key);timeline->selection={{active,curveParameter,key.time}};activeParameter=curveParameter;timeline->parameter=activeParameter;model->timelinePosition=toSeconds(key.time);model->playing=false;model->timelinePreview=true;apply();
}
void CameraEditor::changeKey(){
 if(syncing||curveParameter.empty())return;const auto&a=objectAnimation(*model,active);auto it=a.find(curveParameter);if(it==a.end())return;
 for(auto key:it->second.keys())if(key.time==selectedKeyTime){TimeUs old=key.time;key.time=toTime(keyTime->value());key.value=keyValue->value();editCurveKey(old,key);return;}
}
void CameraEditor::changeSegment(){
 if(syncing||curveParameter.empty()||objectLocked(*model,active))return;auto&c=objectAnimation(*model,active)[curveParameter];if(c.locked)return;AnimKey a,b;if(!pairFor(c,selectedKeyTime,a,b))return;
 auto meta=objectParameter(*model,active,curveParameter);if(meta&&meta->discrete){remember();for(auto key:c.keys()){key.interpolation=Interpolation::Hold;c.put(key);}apply();return;}
 int selectedMode=interpolation->currentData().toInt();remember();
 if(sender()==interpolation&&selectedMode==int(Interpolation::Bezier))materializeHandles(a,b,c.shortest);
 a.interpolation=Interpolation(selectedMode);a.legacyFrom=0;a.legacyTo=1;
 if(sender()!=interpolation){a.interpolation=Interpolation::Bezier;a.outX=outX->value()/100;b.inX=std::max(a.outX,inX->value()/100);a.outY=outY->value();b.inY=inY->value();a.outAbsolute=b.inAbsolute=true;}
 c.put(a);c.put(b);model->playing=false;apply();
}
void CameraEditor::navigateCurrent(bool next){
 const auto&a=objectAnimation(*model,active);TimeUs now=toTime(model->timelinePosition),target=next?INT64_MAX:-1;
 for(const auto&pair:a){if(!activeParameter.empty()&&pair.first!=activeParameter)continue;const auto&keys=pair.second.keys();
  if(next){auto it=std::upper_bound(keys.begin(),keys.end(),now,[](TimeUs t,const AnimKey&k){return t<k.time;});if(it!=keys.end())target=std::min(target,it->time);}
  else{auto it=std::lower_bound(keys.begin(),keys.end(),now,[](const AnimKey&k,TimeUs t){return k.time<t;});if(it!=keys.begin())target=std::max(target,(it-1)->time);}
 }
 if(target<0||target==INT64_MAX)return;timeline->selection.clear();seek(target);timeline->reveal(toSeconds(target));
}
