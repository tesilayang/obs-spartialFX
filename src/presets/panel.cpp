#include "presets/panel.hpp"
#include "presets/composer.hpp"
#include "scene_evaluator.hpp"
#include "widgets.hpp"
#include "hover_help.hpp"
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <optional>
#include <set>
#include <array>
#include <limits>

namespace spatial::presets {
namespace {
constexpr double maxSeconds=double(INT64_MAX/1000000);
QString u(const std::string&s){return QString::fromStdString(s);}
std::string id(QComboBox*w){return w->currentData().toString().toStdString();}
QColor color(Vec3 v){return QColor::fromRgbF(float(std::clamp(v.x,0.,1.)),float(std::clamp(v.y,0.,1.)),float(std::clamp(v.z,0.,1.)));}
Vec3 rgb(const QColor&c){return {c.redF(),c.greenF(),c.blueF()};}
std::vector<std::string> checked(QListWidget*w){std::vector<std::string>result;for(int i=0;i<w->count();++i){auto*v=w->item(i);if((v->flags()&Qt::ItemIsEnabled)&&v->checkState()==Qt::Checked)result.push_back(v->data(Qt::UserRole).toString().toStdString());}return result;}
QPushButton*button(const QString&text,const char*name,QWidget*p){auto*w=new QPushButton(text,p);w->setObjectName(name);w->setMinimumWidth(0);w->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);return w;}
QWidget*fold(QVBoxLayout*layout,const QString&label,const char*name){auto*t=new QToolButton;t->setText(label);t->setObjectName(name);t->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);t->setCheckable(true);t->setArrowType(Qt::RightArrow);t->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);auto*body=new QWidget;body->hide();layout->addWidget(t);layout->addWidget(body);QObject::connect(t,&QToolButton::toggled,body,[t,body](bool on){t->setArrowType(on?Qt::DownArrow:Qt::RightArrow);body->setVisible(on);});return body;}
QFormLayout*form(QWidget*w){auto*f=new QFormLayout(w);f->setContentsMargins(0,2,0,3);f->setHorizontalSpacing(6);f->setVerticalSpacing(3);f->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);f->setRowWrapPolicy(QFormLayout::WrapLongRows);return f;}
QDoubleSpinBox*number(QFormLayout*f,const QString&label,const char*name,double lo,double hi,double v,double sliderLo,double sliderHi){auto*w=new QDoubleSpinBox;w->setObjectName(name);w->setDecimals(2);w->setSingleStep(.01);w->setRange(lo,hi);w->setValue(v);w->setKeyboardTracking(false);auto*slider=new NumericSlider(w,false);slider->setSliderRange(sliderLo,sliderHi);f->addRow(label,slider);return w;}
QByteArray modelJson(const StageModel&m){auto*data=modelData(m);const QByteArray json(obs_data_get_json(data));obs_data_release(data);return json;}
std::vector<std::string> decorationIds(const QJsonObject&recipe){std::vector<std::string>ids;for(const auto&entry:recipe["decoration_ids"].toArray()){const auto value=entry.toObject()["id"].toString();if(!value.isEmpty())ids.push_back(value.toStdString());}return ids;}
bool hasTimelineContent(const StageModel&m){
 auto keys=[](const Animation&a){return std::any_of(a.begin(),a.end(),[](const auto&entry){return !entry.second.keys().empty();});};
 if(keys(m.cameraAnimation)||keys(m.compositeAnimation)||!m.geometries.empty()||!m.motionBlocks.empty()||!m.presetInstances.empty())return true;
 return std::any_of(m.layers.begin(),m.layers.end(),[&](const auto&layer){return keys(layer.animation);});
}
// Exact multiply/divide with a 32-bit multiplier. Frame rates use a divisor no
// larger than 1,000,000 * UINT32_MAX, so the partial remainder cannot overflow.
bool mulDiv(uint64_t a,uint32_t b,uint64_t divisor,uint64_t& quotient,uint64_t&remainder){
 if(!divisor)return false;const auto whole=a/divisor,part=a%divisor;const auto limit=std::numeric_limits<uint64_t>::max();
 if(b&&whole>limit/b)return false;uint64_t fraction=0;remainder=0;
 for(int bit=31;bit>=0;--bit){fraction*=2;remainder=remainder*2+(((b>>bit)&1u)?part:0);fraction+=remainder/divisor;remainder%=divisor;}
 const auto integral=whole*b;if(integral>limit-fraction)return false;quotient=integral+fraction;return true;
}
std::optional<TimeUs> nextVideoFrame(TimeUs end,std::pair<uint32_t,uint32_t>rate){
 const auto num=rate.first,den=rate.second;if(!num||!den||end<0)return std::nullopt;
 const uint64_t denominator=1000000ULL*den,limit=uint64_t(INT64_MAX);uint64_t frame=0,remainder=0;
 if(!mulDiv(uint64_t(end),num,denominator,frame,remainder)||frame==std::numeric_limits<uint64_t>::max())return std::nullopt;++frame;
 const auto whole=frame/num;if(whole>limit/denominator)return std::nullopt;uint64_t fraction=0;
 if(!mulDiv(denominator,uint32_t(frame%num),num,fraction,remainder))return std::nullopt;
 if(remainder)++fraction;const auto integral=whole*denominator;if(fraction>limit-integral)return std::nullopt;
 return TimeUs(integral+fraction);
}
QPixmap thumbnail(int kind,double phase,const StyleDefinition&style){
 QPixmap pix(100,52);pix.fill(QColor("#1c2330"));QPainter p(&pix);p.setRenderHint(QPainter::Antialiasing);const double s=std::sin(phase*2*pi),c=std::cos(phase*2*pi);const QColor a=color(style.colorA),b=color(style.colorB);
 auto tint=[](QColor value,int alpha){value.setAlpha(alpha);return value;};
 auto rect=[&](QRectF area,QColor value,double radius=0.){p.setPen(Qt::NoPen);p.setBrush(value);p.drawRoundedRect(area,radius,radius);};
 auto ellipse=[&](QRectF area,QColor value){p.setPen(Qt::NoPen);p.setBrush(value);p.drawEllipse(area);};
 auto line=[&](QPointF from,QPointF to,QColor value,double width=1.){p.setPen(QPen(value,width));p.drawLine(from,to);};
 auto ring=[&](QRectF area,QColor value,double width=1.){p.setBrush(Qt::NoBrush);p.setPen(QPen(value,width));p.drawEllipse(area);};
 auto star=[&](QPointF at,double radius,QColor value){QPainterPath path;for(int i=0;i<10;++i){const double angle=-pi/2+i*pi/5,arm=i%2?radius*.43:radius;const QPointF point=at+QPointF(std::cos(angle)*arm,std::sin(angle)*arm);if(i)path.lineTo(point);else path.moveTo(point);}path.closeSubpath();p.setPen(Qt::NoPen);p.fillPath(path,value);};
 QLinearGradient wash(0,0,100,52);wash.setColorAt(0,tint(a,28));wash.setColorAt(1,tint(b,7));p.fillRect(pix.rect(),wash);
 switch(kind){
 case 0:
  ellipse(QRectF(-10,24+c*2,36,30),tint(a,72));ellipse(QRectF(77+s*2,-5,29,27),tint(b,94));
  ring(QRectF(24-s,4-s,52+s*2,44+s*2),tint(a,185),1.5);ellipse(QRectF(13+s*2,9,9,9),b);ring(QRectF(78,35,11,11),b,1.4);break;
 case 1:
  rect(QRectF(0,5+s*2,13,42),tint(a,170));rect(QRectF(87,9-s*2,13,37),tint(b,175));
  p.save();p.translate(50,26);p.rotate(-28);rect(QRectF(-65,-13,130,4),tint(a,120));rect(QRectF(-65,14,130,2),tint(b,180));p.restore();
  line({20,7},{32,7},a,1.5);line({20,7},{20,17},a,1.5);line({68,45},{80,45},b,1.5);line({80,35},{80,45},b,1.5);break;
 case 2:
  for(int y=0;y<4;++y)for(int x=0;x<3;++x)ellipse(QRectF(3+x*7+s,y*7+24,2,2),tint(b,100));
  ellipse(QRectF(75+s*2,30,18,18),a);star({17,14+c*2},8+s,a);star({86,11-s*2},5,b);ring(QRectF(23,36,7,7),b,1.8);break;
 case 3:
  p.save();p.translate(50,26);p.rotate(-8+s*2);rect(QRectF(-35,-16,28,33),tint(a,165),1);p.rotate(15);rect(QRectF(9,-17,27,34),tint(b,180),1);p.restore();
  rect(QRectF(19,38,60,7),tint(a,210));line({23,41},{76,41},tint(b,210),1);break;
 case 4:
  for(int i=0;i<6;++i){line({double(i*20),0},{double(i*20),52},tint(a,18));line({0,double(i*10)},{100,double(i*10)},tint(a,18));}
  ring(QRectF(25,3,50,46),tint(a,135),1.1);ellipse(QRectF(48+s*23,24+c*20,3,3),b);
  for(int i=0;i<4;++i){double x=i%2?81:13,y=i/2?45:7;line({x,y},{x+(i%2?-9:9),y},a,1.5);line({x,y},{x,y+(i/2?-8:8)},a,1.5);}
  for(int x=20;x<84;x+=9)line({double(x),48},{double(x+4),48},b,1);break;
 case 5:
  p.save();p.translate(50,26);p.rotate(28);rect(QRectF(-65,-20+s*3,130,8),tint(a,220));rect(QRectF(-65,15-s*3,130,4),tint(b,220));p.restore();
  for(int i=0;i<3;++i){QPainterPath path;path.moveTo(76+i*6+s*2,37);path.lineTo(80+i*6+s*2,40);path.lineTo(76+i*6+s*2,43);path.closeSubpath();p.fillPath(path,b);}break;
 case 6:
  for(int i=0;i<3;++i){QRadialGradient glow(QPointF(i%2?86:12,13+i*13),20);glow.setColorAt(0,tint(i%2?a:b,105));glow.setColorAt(1,tint(i%2?a:b,0));p.fillRect(pix.rect(),glow);}
  ellipse(QRectF(-10+s*2,25,39,30),tint(a,90));ring(QRectF(72,4+c*2,21,18),tint(b,125),1.1);
  for(int i=0;i<4;++i)ellipse(QRectF(7+i*27+s,6+(i%2)*38,2,2),tint(b,200));break;
 case 7:
  for(int y=0;y<4;++y)for(int x=0;x<3;++x)if((x+y)%2)rect(QRectF(2+x*7,27+y*7,7,7),tint(a,47));
  rect(QRectF(4+s*4,7,26,3),a);rect(QRectF(69-s*5,17,27,4),b);rect(QRectF(66+s*4,39,31,3),a);rect(QRectF(7-s*3,45,21,2),b);
  line({82,6},{82,13},b,1.5);line({78.5,9.5},{85.5,9.5},b,1.5);break;
 }
 p.save();p.translate(50+(kind==5?s*3:s),26+(kind==6?c*2:0));p.rotate(kind==3?-3+s*2:kind==7?0:s*.8);
 const double breath=1+(kind==0||kind==6?.025*c:0);p.scale(breath,breath);rect(QRectF(-13,-18,28,38),QColor(0,0,0,70),3);rect(QRectF(-14,-19,28,38),QColor("#e0e4e9"),3);
 rect(QRectF(-11,-16,22,24),QColor("#bdc8d4"),1);ellipse(QRectF(-4,-12,8,8),QColor("#8295a7"));rect(QRectF(-7,-2,14,10),QColor("#8295a7"),5);rect(QRectF(-9,12,18,2),tint(a,150),1);p.restore();
 if(kind==3){p.save();p.translate(38,9);p.rotate(-22);rect(QRectF(-7,-2,14,4),tint(b,210));p.restore();p.save();p.translate(62,44);p.rotate(-22);rect(QRectF(-7,-2,14,4),tint(b,210));p.restore();}
 if(kind==2)star({73,20-s},4.5,a);
 return pix;
}
}

struct PresetPanel::Impl {
 PresetPanel*q;StageModel*model;QString document,layerSignature,presetSignature;
 bool refreshing=true,session=false,published=false,tailPreview=false;std::optional<StageModel>baseline,compiled;
 struct Candidate {ComposeRequest request;int depthMode=0;std::string referenceA,referenceB;};
 std::optional<Candidate>candidate;std::vector<std::string>heldDecorations;std::string heldDecorationStyle;
 std::string styleId="gentle";Vec3 colorA{},colorB{};uint32_t motionSeed=0,maskSeed=0,decorationSeed=0;
 QLabel*status=nullptr,*countLabel=nullptr,*colorPreview[2]{};
 QListWidget*targets=nullptr,*applied=nullptr;
 QComboBox*motion=nullptr,*mask=nullptr,*decor=nullptr,*amount=nullptr,*depth=nullptr,*anchor=nullptr,*depthA=nullptr,*depthB=nullptr,*beatRatio=nullptr;
 QCheckBox*camera=nullptr,*loop=nullptr,*replace=nullptr,*localMask=nullptr,*screen=nullptr,*beat=nullptr,*lockMotion=nullptr,*lockDecor=nullptr,*lockPalette=nullptr;
 QSpinBox*cycles=nullptr;QDoubleSpinBox*seed=nullptr,*period=nullptr,*start=nullptr,*strength=nullptr,*bpm=nullptr,*bakeStart=nullptr,*bakeEnd=nullptr,*bakeStep=nullptr,*position[3]{},*rotation[3]{},*scale[2]{};
 QPushButton*generate=nullptr,*reroll=nullptr,*cancel=nullptr,*apply=nullptr,*append=nullptr,*remove=nullptr,*bake=nullptr,*colorButtons[2]{};
 std::vector<QPushButton*>cards;QTimer*debounce=nullptr;double thumbTime=0;
 QPointer<QColorDialog>colorDialog;uint64_t colorEpoch=0;Vec3 dialogOriginal{};int dialogWhich=0;
 explicit Impl(PresetPanel*w,StageModel*m):q(w),model(m){}
 void tell(const QString&s,bool error=false){status->setText(s);status->setStyleSheet(error?"color:#ff9191;":"color:#a4c8ed;");if(q->message)q->message(s,error);}
  void thumbnails(){for(size_t n=0;n<cards.size();++n){auto style=styleCatalog()[n];if(styleId==style.id){style.colorA=colorA;style.colorB=colorB;}cards[n]->setIcon(QIcon(thumbnail(int(n),thumbTime,style)));}}
  void colors(){setColorValuePreview(colorPreview[0],color(colorA));setColorValuePreview(colorPreview[1],color(colorB));thumbnails();}
 QJsonObject recipe()const{return compiled&&!compiled->presetInstances.empty()?QJsonDocument::fromJson(QByteArray::fromStdString(compiled->presetInstances.back().definitionSnapshot)).object():QJsonObject{};}
 void ready(){const bool available=model&&!document.isEmpty()&&!checked(targets).empty();generate->setEnabled(available);reroll->setEnabled(available);apply->setEnabled(available&&published&&!tailPreview);append->setEnabled(available&&published&&candidate.has_value());cancel->setEnabled(session);}
 void changed(){if(refreshing)return;ready();if(session)debounce->start();}
 void closeColor(){if(!colorDialog)return;(dialogWhich?colorB:colorA)=dialogOriginal;colors();++colorEpoch;auto dialog=colorDialog;colorDialog.clear();dialog->reject();}
 void cancelSession(){debounce->stop();closeColor();const bool notify=published;published=session=tailPreview=false;baseline.reset();compiled.reset();candidate.reset();ready();if(notify&&q->cancelPreview)q->cancelPreview();}
 void chooseStyle(const StyleDefinition&style){styleId=style.id;if(!lockDecor->isChecked()){heldDecorations.clear();heldDecorationStyle.clear();}for(auto*c:cards)c->setChecked(c->property("styleId").toString()==u(styleId));if(!lockPalette->isChecked()){colorA=style.colorA;colorB=style.colorB;colors();}changed();}
 ComposeRequest request()const{
  ComposeRequest r;r.styleId=styleId;r.selectedIds=checked(targets);r.seed=uint32_t(seed->value());r.motionId=id(motion);r.maskId=id(mask);r.decorationId=id(decor);
  r.motionSeed=motionSeed;r.maskSeed=maskSeed;r.decorationSeed=decorationSeed;
    r.resolvedDecorationIds=heldDecorations;r.decorationStyleId=heldDecorationStyle;
    const auto last=recipe();if(lockMotion->isChecked()&&!last.isEmpty()){r.motionId=last["motion"].toString().toStdString();r.motionSeed=uint32_t(last["motion_seed"].toDouble());}if(lockDecor->isChecked()&&!last.isEmpty()){r.decorationId=last["decoration"].toString().toStdString();r.resolvedDecorationIds=decorationIds(last);r.decorationStyleId=last["decoration_style"].toString(last["style"].toString()).toStdString();r.decorationSeed=uint32_t(last["decoration_seed"].toDouble());}
  r.start=toTime(start->value());r.duration=toTime(beat->isChecked()?60./bpm->value()*beatRatio->currentData().toDouble():period->value());r.cycles=cycles->value();r.strength=strength->value();r.decorationAmount=amount->currentIndex();r.colorA=colorA;r.colorB=colorB;r.loop=loop->isChecked();r.camera=camera->isChecked();r.replace=replace->isChecked();r.localMask=localMask->isChecked();r.screenFixed=screen->isChecked();r.anchorId=depth->currentIndex()==2?id(anchor):std::string();
  r.decorationPlane.position={position[0]->value(),position[1]->value(),position[2]->value()};r.decorationPlane.rotation={rotation[0]->value(),rotation[1]->value(),rotation[2]->value()};r.decorationPlane.scaleX=scale[0]->value();r.decorationPlane.scaleY=scale[1]->value();r.localMaskPlane=r.decorationPlane;
   return r;
  }
  bool place(ComposeRequest&r,const StageModel&base,const Candidate&selection){
   const bool worldGraphics=(r.decorationAmount>0&&(r.decorationId!="none"||!r.resolvedDecorationIds.empty()))||(r.maskId!="none"&&!r.localMask);
   if(worldGraphics&&selection.depthMode==1&&(selection.referenceA.empty()||selection.referenceB.empty()||selection.referenceA==selection.referenceB)){tell(QStringLiteral("层间位置需要两个不同的参考素材。"),true);return false;}
   if(worldGraphics&&selection.depthMode==2&&r.anchorId.empty()){tell(QStringLiteral("请选择图形要跟随的素材。"),true);return false;}
   if(selection.depthMode<2){const auto scene=evaluateSceneAt(base,r.start);Vec3 initial{};
    if(selection.depthMode==1){Vec3 a{},b{};for(size_t n=0;n<base.layers.size()&&n<scene.world.size();++n){if(base.layers[n].instanceId==selection.referenceA)a=scene.world[n].plane.position;if(base.layers[n].instanceId==selection.referenceB)b=scene.world[n].plane.position;}initial=multiply(add(a,b),.5);}
   else{const auto forward=rotate({0,0,1},scene.camera.rotation);double nearest=1500;bool found=false;for(const auto&layer:scene.world){const auto delta=sub(layer.plane.position,scene.camera.position);const double distance=delta.x*forward.x+delta.y*forward.y+delta.z*forward.z;if(distance>0&&(!found||distance<nearest)){nearest=distance;found=true;}}initial=add(scene.camera.position,multiply(forward,std::max(1.,nearest-100)));}r.decorationPlane.position=add(initial,r.decorationPlane.position);
   }return true;
 }
  bool compile(StageModel&output,const Candidate&selection,std::string&error,ComposeFailure&failure){
   failure=ComposeFailure::Other;auto request=selection.request;if(!place(request,output,selection))return false;
   return compileComposition(output,request,error,&failure);
  }
  bool baselineCurrent(){if(model&&baseline&&modelJson(*model)==modelJson(*baseline))return true;cancelSession();tell(QStringLiteral("时间轴已更改，请重新生成预览后再应用。"),true);return false;}
  bool appendKeepsInactiveTracks(const StageModel&output,std::string&error)const{
   if(!baseline)return false;
   auto reactivated=[&](const Animation&before,const Animation&after,const std::string&label){
    for(const auto&entry:before){const auto track=after.find(entry.first);if(!entry.second.enabled&&!entry.second.keys().empty()&&track!=after.end()&&track->second.enabled){const auto*meta=parameter(entry.first);error=label+" · "+(meta?meta->label:entry.first)+" 保留了已停用的关键帧；请先确认该参数轨的启用状态，再重新生成并补充。";return true;}}return false;
   };
   if(reactivated(baseline->cameraAnimation,output.cameraAnimation,"镜头")||reactivated(baseline->compositeAnimation,output.compositeAnimation,"整体画面"))return false;
   for(const auto&layer:baseline->layers){const auto found=std::find_if(output.layers.begin(),output.layers.end(),[&](const auto&current){return current.instanceId==layer.instanceId;});if(found!=output.layers.end()&&reactivated(layer.animation,found->animation,layerDisplayName(layer)))return false;}
   for(const auto&geometry:baseline->geometries){const auto found=std::find_if(output.geometries.begin(),output.geometries.end(),[&](const auto&current){return current.instanceId==geometry.instanceId;});if(found!=output.geometries.end()&&reactivated(geometry.animation,found->animation,geometry.label))return false;}
   return true;
  }
  std::optional<TimeUs>tailStart(){
   if(!baseline)return std::nullopt;if(!hasTimelineContent(*baseline))return TimeUs(0);
   std::pair<uint32_t,uint32_t>rate{};if(q->videoFrameRate)rate=q->videoFrameRate();else if(obs_initialized()){obs_video_info info{};if(obs_get_video_info(&info))rate={info.fps_num,info.fps_den};}
   const auto time=nextVideoFrame(contentEnd(*baseline),rate);if(!time)tell(QStringLiteral("无法确定补充位置，请确认 OBS 视频帧率可用、时间轴未超出范围。"),true);return time;
  }
  void freeze(Candidate&selection,const StageModel&output){
   const auto resolved=QJsonDocument::fromJson(QByteArray::fromStdString(output.presetInstances.back().definitionSnapshot)).object();auto&r=selection.request;
    r.motionId=resolved["motion"].toString().toStdString();r.maskId=resolved["mask"].toString().toStdString();r.decorationId=resolved["decoration"].toString().toStdString();r.resolvedDecorationIds=decorationIds(resolved);r.decorationStyleId=resolved["decoration_style"].toString(resolved["style"].toString()).toStdString();
   r.motionSeed=uint32_t(resolved["motion_seed"].toDouble());r.maskSeed=uint32_t(resolved["mask_seed"].toDouble());r.decorationSeed=uint32_t(resolved["decoration_seed"].toDouble());
  }
  void publish(){if(!compiled||!q->previewDocument)return;auto preview=*compiled;if(tailPreview&&candidate)preview.timelinePosition=toSeconds(candidate->request.start);q->previewDocument(preview);}
  void clearFailedPreview(const std::string&error){if(published&&q->cancelPreview)q->cancelPreview();published=tailPreview=false;compiled.reset();candidate.reset();ready();if(!error.empty())tell(u(error),true);}
  void describePreview(){
   if(tailPreview&&candidate){tell(QStringLiteral("尾部预览 · 从 %1 秒补充").arg(toSeconds(candidate->request.start),0,'f',3));return;}const auto selected=recipe();
   auto partName=[](const auto&catalog,const QString&value){for(const auto&p:catalog)if(value==QString::fromUtf8(p.id))return QString::fromUtf8(p.name);return QStringLiteral("无");};
   tell(QStringLiteral("预览中：%1 · %2 · %3").arg(partName(motionCatalog(),selected["motion"].toString()),partName(maskCatalog(),selected["mask"].toString()),partName(decorationCatalog(),selected["decoration"].toString())));
 }
 void updatePreview(){
  if(document.isEmpty()||!model||!q->previewDocument){tell(QStringLiteral("请先在镜头与动画页导入素材。"),true);return;}
   if(!session){baseline=*model;session=true;}else if(!baselineCurrent())return;StageModel snapshot=*baseline;
   Candidate selection{request(),depth->currentIndex(),id(depthA),id(depthB)};std::string error;ComposeFailure failure=ComposeFailure::Other;tailPreview=false;
   if(!compile(snapshot,selection,error,failure)){
    if(failure!=ComposeFailure::IntervalConflict){clearFailedPreview(error);return;}
    const auto at=tailStart();if(!at){clearFailedPreview({});return;}selection.request.start=*at;selection.request.replace=false;snapshot=*baseline;
    if(!compile(snapshot,selection,error,failure)){clearFailedPreview(error);return;}tailPreview=true;
   }
    if(tailPreview&&!appendKeepsInactiveTracks(snapshot,error)){clearFailedPreview(error);return;}
    snapshot.loop=tailPreview?baseline->loop:selection.request.loop;freeze(selection,snapshot);candidate=std::move(selection);compiled=std::move(snapshot);published=true;ready();publish();describePreview();
 }
 void nextSeed(){QSignalBlocker block(seed);uint32_t value=uint32_t(seed->value());value=value*1664525u+1013904223u;seed->setValue(double(value));}
  void varyColors(){
   if(lockPalette->isChecked())return;const auto style=std::find_if(styleCatalog().begin(),styleCatalog().end(),[this](const auto&s){return styleId==s.id;});if(style==styleCatalog().end())return;
   using Pair=std::pair<const char*,const char*>;
   static const std::array<std::array<Pair,5>,8>palettes={{
    {{{"#AE8CD9","#EFBDAD"},{"#75B5AE","#F0D2AE"},{"#D49AAB","#9DAFCD"},{"#A9B584","#F0C8A3"},{"#8FAED0","#E0C1DC"}}},
    {{{"#476BEB","#2EC4D6"},{"#EA704C","#F2CD58"},{"#38756A","#C5D488"},{"#735AC6","#D9A7D5"},{"#235A83","#F0C88B"}}},
    {{{"#FAB81F","#F53D7A"},{"#5B4CEB","#F2AC41"},{"#35C6AA","#F06D8E"},{"#EB653F","#83C7E7"},{"#AB64D6","#CBEA51"}}},
    {{{"#D48552","#F0D694"},{"#708C78","#DDB893"},{"#B76F6A","#DDD2AC"},{"#547A95","#E5BB7F"},{"#9F83A7","#D6C58F"}}},
    {{{"#1FC7D9","#6175FA"},{"#30C9A2","#C7E557"},{"#DE60CB","#6A95ED"},{"#F39C45","#8AD4D1"},{"#5EA9E5","#D4DCEB"}}},
    {{{"#F54733","#FAC429"},{"#614DE8","#FF9368"},{"#1ABBA0","#DBED6D"},{"#EE3F88","#76C9EF"},{"#3F7EEA","#E7B65F"}}},
    {{{"#A16BEF","#57BAEF"},{"#E5A2C4","#ADA4EF"},{"#72BBAF","#E5DAB2"},{"#8B9FDB","#DFBDDD"},{"#D5989E","#8CC6D8"}}},
    {{{"#6BFA59","#E53DC2"},{"#3ED9E0","#F67B47"},{"#C8EF45","#8069EB"},{"#EC537A","#65CDAC"},{"#6397F2","#EAD772"}}}
   }};
   const auto&selected=palettes[size_t(style-styleCatalog().begin())][uint32_t(seed->value())%5];colorA=rgb(QColor(selected.first));colorB=rgb(QColor(selected.second));colors();
  }
   void rerollAll(){closeColor();motionSeed=maskSeed=decorationSeed=0;if(!lockDecor->isChecked()){heldDecorations.clear();heldDecorationStyle.clear();}nextSeed();varyColors();updatePreview();}
   void holdPartSeeds(const QJsonObject&current){motionSeed=uint32_t(current["motion_seed"].toDouble());maskSeed=uint32_t(current["mask_seed"].toDouble());decorationSeed=uint32_t(current["decoration_seed"].toDouble());heldDecorations=decorationIds(current);heldDecorationStyle=current["decoration_style"].toString(current["style"].toString()).toStdString();}
 void rerollPart(bool movement){closeColor();const auto current=recipe();if(current.isEmpty()){rerollAll();return;}holdPartSeeds(current);
  refreshing=true;auto select=[](QComboBox*w,const QString&s){w->setCurrentIndex(std::max(0,w->findData(s)));};
    select(mask,current["mask"].toString());select(movement?decor:motion,current[movement?"decoration":"motion"].toString());select(movement?motion:decor,QString());if(!movement){heldDecorations.clear();heldDecorationStyle.clear();}(movement?lockMotion:lockDecor)->setChecked(false);refreshing=false;nextSeed();(movement?motionSeed:decorationSeed)=std::max(1u,uint32_t(seed->value()));updatePreview();
 }
 void rerollColors(){closeColor();const auto current=recipe();if(!current.isEmpty()){holdPartSeeds(current);refreshing=true;auto select=[](QComboBox*w,const QString&s){w->setCurrentIndex(std::max(0,w->findData(s)));};select(motion,current["motion"].toString());select(mask,current["mask"].toString());select(decor,current["decoration"].toString());refreshing=false;}const bool previous=lockPalette->isChecked();lockPalette->setChecked(false);nextSeed();varyColors();lockPalette->setChecked(previous);if(session)updatePreview();}
 void pickColor(int which){
  if(colorDialog){colorDialog->raise();colorDialog->activateWindow();return;}
   dialogWhich=which;dialogOriginal=which?colorB:colorA;const auto old=dialogOriginal;const auto previous=compiled;const auto previousCandidate=candidate;const bool previousTail=tailPreview;const bool hadSession=session;const uint64_t epoch=++colorEpoch;
  auto*dialog=new QColorDialog(color(old),q);colorDialog=dialog;dialog->setObjectName(which?"spatial.preset-color-dialog-b36":"spatial.preset-color-dialog-a36");dialog->setWindowTitle(QStringLiteral("预设配色 · 实时预览"));dialog->setOption(QColorDialog::DontUseNativeDialog);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowModality(Qt::WindowModal);
  QObject::connect(dialog,&QColorDialog::currentColorChanged,q,[this,which,epoch](const QColor&c){if(epoch!=colorEpoch||!c.isValid())return;(which?colorB:colorA)=rgb(c);colors();if(session)updatePreview();});
   QObject::connect(dialog,&QDialog::finished,q,[this,dialog,which,epoch,old,previous,previousCandidate,previousTail,hadSession](int result){
   if(colorDialog==dialog)colorDialog.clear();if(epoch!=colorEpoch)return;
    debounce->stop();if(result!=QDialog::Accepted){(which?colorB:colorA)=old;colors();if(hadSession&&session&&previous){compiled=previous;candidate=previousCandidate;tailPreview=previousTail;published=true;ready();publish();describePreview();}}
   else lockPalette->setChecked(true);
  });dialog->open();
 }
  void commit(bool atTail=false){
  if(!published||!compiled||!model||document.isEmpty()||!q->applyDocument){tell(QStringLiteral("先点击“生成一套”，满意后再应用。"),true);return;}
   if(!baselineCurrent())return;
  if(debounce->isActive()){debounce->stop();updatePreview();if(!published||!compiled)return;}
   if(!baselineCurrent()||!candidate||(!atTail&&tailPreview))return;
   StageModel snapshot=*compiled;
   if(atTail){const auto at=tailStart();if(!at)return;auto selection=*candidate;selection.request.start=*at;selection.request.replace=false;snapshot=*baseline;std::string error;ComposeFailure failure=ComposeFailure::Other;if(!compile(snapshot,selection,error,failure)){if(!error.empty())tell(u(error),true);return;}if(!appendKeepsInactiveTracks(snapshot,error)){cancelSession();tell(u(error),true);return;}snapshot.loop=baseline->loop;}
   cancelSession();q->applyDocument(snapshot);q->refresh();tell(atTail?QStringLiteral("已补充至时间轴尾部；撤销可恢复整套。"):QStringLiteral("已应用到时间轴；撤销可恢复整套。"));
 }
 QString selectedApplied()const{auto*i=applied->currentItem();return i?i->data(Qt::UserRole).toString():QString();}
 void removeSelected(){
  const auto selected=selectedApplied();const auto doc=document;if(selected.isEmpty()||!model||!q->applyDocument)return;
  if(QMessageBox::question(q,QStringLiteral("移除预设"),QStringLiteral("移除此预设生成的运动、遮罩和装饰？可撤销。"))!=QMessageBox::Yes||doc!=document)return;
  cancelSession();StageModel snapshot=*model;std::string error;if(!removePreset(snapshot,selected.toStdString(),error)){tell(u(error),true);return;}q->applyDocument(snapshot);q->refresh();tell(QStringLiteral("已移除选定预设。"));
 }
 uint64_t bakeCount()const{const auto a=toTime(bakeStart->value()),b=toTime(bakeEnd->value()),step=toTime(bakeStep->value());return b>a&&step>0?uint64_t((b-a)/step)+uint64_t((b-a)%step!=0)+1:0;}
 void updateBakeCount(){const auto n=bakeCount();countLabel->setText(n?QStringLiteral("预计 %1 个采样时刻 / 上限 10001").arg(qulonglong(n)):QStringLiteral("请选择有效的转换区间。"));const bool selected=!document.isEmpty()&&!selectedApplied().isEmpty();bake->setEnabled(selected&&n>1&&n<=10001);remove->setEnabled(selected);}
 void bakeSelected(){
  const auto n=bakeCount();const auto selected=selectedApplied();const auto doc=document;if(!model||!q->applyDocument||selected.isEmpty()||n<2||n>10001)return;
  if(QMessageBox::question(q,QStringLiteral("转换为关键帧"),QStringLiteral("将 %1 至 %2 秒转换为 %3 个采样时刻的关键帧？可以撤销。").arg(bakeStart->value()).arg(bakeEnd->value()).arg(qulonglong(n)))!=QMessageBox::Yes||doc!=document)return;
  cancelSession();StageModel snapshot=*model;std::string error;if(!bakePreset(snapshot,selected.toStdString(),toTime(bakeStart->value()),toTime(bakeEnd->value()),toTime(bakeStep->value()),error)){tell(u(error),true);return;}q->applyDocument(snapshot);q->refresh();tell(QStringLiteral("已转换为可编辑的关键帧。"));
 }
 void rebuildCombo(QComboBox*w){const auto old=w->currentData();QSignalBlocker block(w);w->clear();w->addItem(QStringLiteral("请选择素材"),QString());if(model)for(const auto&layer:model->layers)w->addItem(u(layerDisplayName(layer)),u(layer.instanceId));w->setCurrentIndex(std::max(0,w->findData(old)));}
 void refresh(){
  refreshing=true;QString signature;if(model)for(const auto&l:model->layers)signature+=u(l.instanceId)+QChar(0x1f)+u(layerDisplayName(l))+QString::number(l.locked)+QChar(0x1e);
  if(layerSignature!=signature){const auto selected=checked(targets);std::set<std::string>old,ids(selected.begin(),selected.end());for(int i=0;i<targets->count();++i)old.insert(targets->item(i)->data(Qt::UserRole).toString().toStdString());bool lost=false;for(const auto&selectedId:selected)lost=lost||!model||std::none_of(model->layers.begin(),model->layers.end(),[&](const auto&l){return l.instanceId==selectedId&&!l.locked;});if(session)cancelSession();layerSignature=signature;
   {QSignalBlocker block(targets);targets->clear();if(model)for(const auto&l:model->layers){auto*i=new QListWidgetItem(u(layerDisplayName(l))+(l.locked?QStringLiteral("（已锁定）"):QString()),targets);i->setData(Qt::UserRole,u(l.instanceId));i->setFlags(l.locked?Qt::NoItemFlags:Qt::ItemIsEnabled|Qt::ItemIsUserCheckable);i->setCheckState(!l.locked&&(ids.count(l.instanceId)||!old.count(l.instanceId))?Qt::Checked:Qt::Unchecked);i->setToolTip(l.locked?QStringLiteral("此素材在时间轴中已锁定，解锁后可以选择。") :QStringLiteral("勾选后参与本套运动；不重复导入，不改变基础位置。"));}}
   for(auto*c:{anchor,depthA,depthB})rebuildCombo(c);if(lost)tell(QStringLiteral("已移除或锁定的素材已退出本次选择。"),true);
  }
  QString instances;if(model)for(const auto&i:model->presetInstances)instances+=u(i.instanceId)+QChar(0x1f)+u(i.label)+QChar(0x1e);
  if(instances!=presetSignature){const auto selected=selectedApplied();QSignalBlocker block(applied);applied->clear();if(model)for(const auto&i:model->presetInstances){auto*item=new QListWidgetItem(u(i.label),applied);item->setData(Qt::UserRole,u(i.instanceId));if(u(i.instanceId)==selected)applied->setCurrentItem(item);}presetSignature=instances;}
  ready();updateBakeCount();refreshing=false;if(!model||model->layers.empty())tell(QStringLiteral("先到“镜头与动画”导入素材，再回来生成预设。"));
 }
};

PresetPanel::PresetPanel(StageModel*m,QWidget*parent):QWidget(parent),d(std::make_unique<Impl>(this,m)){
 setObjectName("spatial.presets36");auto*root=new QVBoxLayout(this);root->setContentsMargins(0,0,0,0);root->setSpacing(0);auto*scroll=new QScrollArea;scroll->setObjectName("spatial.preset-content-scroll361");scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);auto*content=new QWidget;content->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);auto*layout=new QVBoxLayout(content);layout->setContentsMargins(8,6,8,6);layout->setSpacing(4);scroll->setWidget(content);root->addWidget(scroll,1);auto*footer=new QWidget;footer->setObjectName("spatial.preset-footer361");footer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);auto*footerLayout=new QVBoxLayout(footer);footerLayout->setContentsMargins(8,6,8,6);footerLayout->setSpacing(4);root->addWidget(footer);
 auto*targetHeader=new QHBoxLayout;targetHeader->addWidget(new QLabel(QStringLiteral("让哪些素材动起来")),1);auto*all=button(QStringLiteral("全选"),"spatial.preset-select-all361",this);auto*none=button(QStringLiteral("清空"),"spatial.preset-select-none361",this);all->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);none->setSizePolicy(QSizePolicy::Fixed,QSizePolicy::Fixed);targetHeader->addWidget(all);targetHeader->addWidget(none);layout->addLayout(targetHeader);
 d->targets=new QListWidget;d->targets->setObjectName("spatial.preset-targets361");d->targets->setSelectionMode(QAbstractItemView::NoSelection);d->targets->setMinimumHeight(66);d->targets->setMaximumHeight(110);layout->addWidget(d->targets);connect(d->targets,&QListWidget::itemChanged,this,[this]{d->changed();});
 for(const auto&p:std::vector<std::pair<QPushButton*,bool>>{{all,true},{none,false}})connect(p.first,&QPushButton::clicked,this,[this,on=p.second]{d->refreshing=true;for(int i=0;i<d->targets->count();++i){auto*v=d->targets->item(i);if(v->flags()&Qt::ItemIsEnabled)v->setCheckState(on?Qt::Checked:Qt::Unchecked);}d->refreshing=false;d->changed();});
 layout->addWidget(new QLabel(QStringLiteral("挑一个画面风格")));auto*grid=new QGridLayout;grid->setSpacing(4);int index=0;
 for(const auto&style:styleCatalog()){auto*b=button(QString::fromUtf8(style.name),("spatial.preset-style-"+std::string(style.id)).c_str(),this);b->setCheckable(true);b->setProperty("styleId",u(style.id));b->setIconSize(QSize(75,39));b->setMinimumHeight(46);b->setToolTip(QString::fromUtf8(style.description));grid->addWidget(b,index/2,index%2);d->cards.push_back(b);const auto*def=&style;connect(b,&QPushButton::clicked,this,[this,def]{d->chooseStyle(*def);});++index;}layout->addLayout(grid);
 auto combo=[this](const char*name){auto*w=new QComboBox;w->setObjectName(name);w->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);w->setMinimumContentsLength(8);connect(w,&QComboBox::currentIndexChanged,this,[this]{d->changed();});return w;};
 auto check=[this](const QString&label,const char*name,bool on=false){auto*w=new QCheckBox(label);w->setObjectName(name);w->setChecked(on);connect(w,&QCheckBox::toggled,this,[this]{d->changed();});return w;};
 auto*main=new QWidget;auto*f=form(main);layout->addWidget(main);d->strength=number(f,QStringLiteral("动感"),"spatial.preset-strength36",0,2,.6,0,2);d->strength->parentWidget()->setToolTip(QStringLiteral("0～0.4 轻柔，约0.6适中，1以上强烈。只改变生成动作的幅度，不修改基础布局。"));
 auto*levels=new QWidget;auto*levelLayout=new QHBoxLayout(levels);levelLayout->setContentsMargins(0,0,0,0);for(const auto&p:std::vector<std::pair<QString,double>>{{QStringLiteral("轻柔"),.3},{QStringLiteral("适中"),.6},{QStringLiteral("强烈"),1.2}}){auto*b=button(p.first,("spatial.preset-level-"+std::to_string(int(p.second*100))).c_str(),levels);levelLayout->addWidget(b);connect(b,&QPushButton::clicked,this,[this,v=p.second]{d->strength->setValue(v);});}f->addRow(QString(),levels);
 d->amount=combo("spatial.preset-amount361");d->amount->addItems({QStringLiteral("不加装饰"),QStringLiteral("少量装饰"),QStringLiteral("丰富装饰")});d->amount->setCurrentIndex(1);d->amount->setToolTip(QStringLiteral("控制程序绘制的装饰数量。露出遮罩由详细控制单独选择；不加装饰不会自动裁切素材。"));f->addRow(QStringLiteral("画面装饰"),d->amount);
  d->period=number(f,QStringLiteral("时长 / 秒"),"spatial.preset-period36",.01,maxSeconds,6,.01,30);d->loop=check(QStringLiteral("循环播放"),"spatial.preset-loop36");d->loop->setToolTip(QStringLiteral("勾选后生成有界周期动作。应用到时间轴时开启舞台循环；补充至时间轴时沿用舞台当前的循环设置。每段周期等于这里的时长；全场回到开头的时刻取时间轴最晚结束点。关闭则生成一次性可编辑关键帧。"));f->addRow(d->loop);
  auto*actions=new QHBoxLayout;actions->setSpacing(4);d->generate=button(QStringLiteral("生成一套"),"spatial.preset-generate361",footer);d->reroll=button(QStringLiteral("换一套"),"spatial.preset-reroll361",footer);actions->addWidget(d->generate);actions->addWidget(d->reroll);footerLayout->addLayout(actions);
  auto*commitActions=new QHBoxLayout;commitActions->setSpacing(4);d->apply=button(QStringLiteral("应用到时间轴"),"spatial.preset-apply36",footer);d->append=button(QStringLiteral("补充至时间轴"),"spatial.preset-append363",footer);d->cancel=button(QStringLiteral("取消预览"),"spatial.preset-cancel36",footer);commitActions->addWidget(d->apply);commitActions->addWidget(d->append);commitActions->addWidget(d->cancel);footerLayout->addLayout(commitActions);
 d->status=new QLabel;d->status->setObjectName("spatial.preset-status36");d->status->setWordWrap(true);footerLayout->addWidget(d->status);
 auto*details=fold(layout,QStringLiteral("详细控制"),"spatial.preset-details-toggle361");auto*detailLayout=new QVBoxLayout(details);detailLayout->setContentsMargins(0,0,0,0);detailLayout->setSpacing(4);
 auto*parts=new QWidget;auto*pf=form(parts);detailLayout->addWidget(parts);
 auto partCombo=[&](const char*name,const std::vector<PartDefinition>&catalog){auto*w=combo(name);w->addItem(QStringLiteral("按风格自动挑选"),QString());w->addItem(QStringLiteral("关闭"),QStringLiteral("none"));for(const auto&p:catalog){w->addItem(QString::fromUtf8(p.name),QString::fromUtf8(p.id));w->setItemData(w->count()-1,QString::fromUtf8(p.description),Qt::ToolTipRole);}return w;};
 d->motion=partCombo("spatial.preset-motion-part361",motionCatalog());d->mask=partCombo("spatial.preset-mask-part361",maskCatalog());d->mask->setCurrentIndex(1);d->decor=partCombo("spatial.preset-decor-part361",decorationCatalog());pf->addRow(QStringLiteral("素材运动"),d->motion);pf->addRow(QStringLiteral("露出遮罩"),d->mask);pf->addRow(QStringLiteral("装饰色块"),d->decor);
  d->mask->setToolTip(QStringLiteral("裁切每个勾选素材的可见形状，与可见装饰不同。默认关闭；本版不把单个实时来源伪装成两个时间画面的A/B转场。"));connect(d->decor,&QComboBox::currentIndexChanged,this,[this]{if(!d->refreshing){d->heldDecorations.clear();d->heldDecorationStyle.clear();if(!id(d->decor).empty()&&id(d->decor)!="none"&&d->amount->currentIndex()==0)d->amount->setCurrentIndex(1);}});
 auto*locks=new QWidget;auto*ll=new QHBoxLayout(locks);ll->setContentsMargins(0,0,0,0);d->lockMotion=check(QStringLiteral("固定运动"),"spatial.preset-lock-motion361");d->lockDecor=check(QStringLiteral("固定装饰"),"spatial.preset-lock-decor361");d->lockPalette=check(QStringLiteral("固定配色"),"spatial.preset-lock-colors361");for(auto*w:{d->lockMotion,d->lockDecor,d->lockPalette})ll->addWidget(w);pf->addRow(locks);
 d->lockMotion->setToolTip(QStringLiteral("换一套时保留已生成的运动种类、随机方向和相位。动感、时长等明确调整仍会生效；时间轴对象锁用于禁止编辑已有动作。"));d->lockDecor->setToolTip(QStringLiteral("换一套时保留装饰种类、随机布局和相位。配色、数量、空间位置等明确调整仍会生效。"));d->lockPalette->setToolTip(QStringLiteral("保留当前A/B颜色。选定颜色后自动勾选；取消勾选后，切换风格和换一套可以使用风格色板。"));
  connect(d->amount,&QComboBox::currentIndexChanged,this,[this]{if(!d->refreshing&&!d->lockDecor->isChecked()){d->heldDecorations.clear();d->heldDecorationStyle.clear();}});
 auto*rerolls=new QWidget;auto*rl=new QHBoxLayout(rerolls);rl->setContentsMargins(0,0,0,0);auto*onlyMotion=button(QStringLiteral("只换运动"),"spatial.preset-reroll-motion361",rerolls);auto*onlyDecor=button(QStringLiteral("只换装饰"),"spatial.preset-reroll-decor361",rerolls);auto*onlyColors=button(QStringLiteral("只换配色"),"spatial.preset-reroll-colors361",rerolls);for(auto*b:{onlyMotion,onlyDecor,onlyColors})rl->addWidget(b);pf->addRow(rerolls);connect(onlyMotion,&QPushButton::clicked,this,[this]{d->rerollPart(true);});connect(onlyDecor,&QPushButton::clicked,this,[this]{d->rerollPart(false);});connect(onlyColors,&QPushButton::clicked,this,[this]{d->rerollColors();});
 d->start=number(pf,QStringLiteral("开始 / 秒"),"spatial.preset-start36",0,maxSeconds,0,0,60);d->camera=check(QStringLiteral("同时移动镜头"),"spatial.preset-camera36");d->camera->setToolTip(QStringLiteral("额外给整体镜头增加低幅横移；关闭时只改变勾选素材。已有镜头轨冲突或镜头锁定会阻止应用。"));pf->addRow(d->camera);d->replace=check(QStringLiteral("替换已有关键帧冲突区间"),"spatial.preset-replace36");d->replace->setToolTip(QStringLiteral("默认阻止覆盖已有动画。勾选后只替换生成动作所用参数的时间区间，保留区间外曲线；已有循环块或锁定参数仍会阻止应用。"));pf->addRow(d->replace);
 d->seed=new QDoubleSpinBox;d->seed->setObjectName("spatial.preset-seed361");d->seed->setDecimals(0);d->seed->setSingleStep(1);d->seed->setRange(0,4294967295.);d->seed->setValue(double(QRandomGenerator::global()->generate()));d->seed->setKeyboardTracking(false);pf->addRow(QStringLiteral("随机种子"),d->seed);
 for(int n=0;n<2;++n){auto*row=new QWidget;auto*l=new QHBoxLayout(row);l->setContentsMargins(0,0,0,0);l->setSpacing(4);d->colorPreview[n]=new QLabel;d->colorPreview[n]->setObjectName(n?"spatial.preset-color-preview-b361":"spatial.preset-color-preview-a361");d->colorButtons[n]=button(QStringLiteral("选择颜色"),n?"spatial.preset-color-b36":"spatial.preset-color-a36",row);l->addWidget(d->colorPreview[n],1);l->addWidget(d->colorButtons[n],1);pf->addRow(n?QStringLiteral("配色 B"):QStringLiteral("配色 A"),row);connect(d->colorButtons[n],&QPushButton::clicked,this,[this,n]{d->pickColor(n);});}
 auto*space=fold(detailLayout,QStringLiteral("图形的空间位置"),"spatial.preset-space-toggle36");auto*sf=form(space);d->depth=combo("spatial.preset-depth36");d->depth->addItems({QStringLiteral("放在素材前方"),QStringLiteral("放在两层之间"),QStringLiteral("跟随某个素材"),QStringLiteral("自定义世界 XYZ")});sf->addRow(QStringLiteral("放在哪里"),d->depth);d->depthA=combo("spatial.preset-depth-a36");d->depthB=combo("spatial.preset-depth-b36");d->anchor=combo("spatial.preset-anchor36");sf->addRow(QStringLiteral("参考 A"),d->depthA);sf->addRow(QStringLiteral("参考 B"),d->depthB);sf->addRow(QStringLiteral("跟随素材"),d->anchor);d->depthA->setEnabled(false);d->depthB->setEnabled(false);d->anchor->setEnabled(false);connect(d->depth,&QComboBox::currentIndexChanged,this,[this](int i){d->depthA->setEnabled(i==1);d->depthB->setEnabled(i==1);d->anchor->setEnabled(i==2);});
 d->depth->setToolTip(QStringLiteral("前方/层间依据开始时刻的统一场景求值，XYZ为附加偏移；跟随时XYZ为素材局部偏移；自定义时XYZ是绝对世界位置。只影响可见装饰和世界遮罩，不移动素材基础布局。"));
 const char*xyz[]={"X","Y","Z"};const char*pn[]={"spatial.preset-x36","spatial.preset-y36","spatial.preset-z36"};const char*rn[]={"spatial.preset-rx36","spatial.preset-ry36","spatial.preset-rz36"};for(int a=0;a<3;++a)d->position[a]=number(sf,QStringLiteral("位置 ")+xyz[a],pn[a],-100000,100000,0,-2000,2000);for(int a=0;a<3;++a)d->rotation[a]=number(sf,QStringLiteral("旋转 ")+xyz[a],rn[a],-100000,100000,0,-360,360);d->scale[0]=number(sf,QStringLiteral("缩放 X"),"spatial.preset-scale-x36",.01,100,1,.01,4);d->scale[1]=number(sf,QStringLiteral("缩放 Y"),"spatial.preset-scale-y36",.01,100,1,.01,4);d->localMask=check(QStringLiteral("遮罩跟随各素材局部坐标"),"spatial.preset-local-mask36",true);d->screen=check(QStringLiteral("图形固定在屏幕坐标"),"spatial.preset-screen-fixed36");sf->addRow(d->localMask);sf->addRow(d->screen);
 auto*timing=fold(detailLayout,QStringLiteral("循环与节拍"),"spatial.preset-timing-toggle36");auto*tf=form(timing);d->cycles=new QSpinBox;d->cycles->setObjectName("spatial.preset-cycles36");d->cycles->setRange(1,1000000);d->cycles->setValue(1);tf->addRow(QStringLiteral("段内重复次数"),d->cycles);d->cycles->setToolTip(QStringLiteral("整段结束位置=开始+时长×次数。一次性关键帧最多64次；循环块支持更多次数。是否让整个舞台从末尾回到开头由首屏“循环播放”控制。"));connect(d->cycles,&QSpinBox::valueChanged,this,[this]{d->changed();});d->beat=check(QStringLiteral("按拍长确定每周期时长"),"spatial.preset-beat36");tf->addRow(d->beat);d->bpm=number(tf,QStringLiteral("BPM"),"spatial.preset-bpm36",1,999,120,30,240);d->beatRatio=combo("spatial.preset-beat-ratio36");for(double v:{.25,.5,1.,2.,4.,8.})d->beatRatio->addItem(QString::number(v)+QStringLiteral(" 拍"),v);d->beatRatio->setCurrentIndex(2);tf->addRow(QStringLiteral("每周期"),d->beatRatio);d->bpm->parentWidget()->setEnabled(false);d->beatRatio->setEnabled(false);connect(d->beat,&QCheckBox::toggled,this,[this](bool on){d->bpm->parentWidget()->setEnabled(on);d->beatRatio->setEnabled(on);d->period->parentWidget()->setEnabled(!on);});auto*reuse=button(QStringLiteral("沿用镜头页 BPM 与拍点"),"spatial.preset-reuse-bpm36",timing);tf->addRow(reuse);connect(reuse,&QPushButton::clicked,this,[this]{auto*existing=window()->findChild<QDoubleSpinBox*>("spatial.bpm32");if(!existing){d->tell(QStringLiteral("未找到镜头页 BPM，请手动输入。"),true);return;}d->bpm->setValue(existing->value());if(auto*offset=window()->findChild<QDoubleSpinBox*>("spatial.beat-offset32"))d->start->setValue(offset->value());d->beat->setChecked(true);});
 auto*instances=fold(detailLayout,QStringLiteral("已应用预设 / 转换关键帧"),"spatial.preset-applied-toggle36");auto*af=form(instances);d->applied=new QListWidget;d->applied->setObjectName("spatial.preset-applied36");d->applied->setMaximumHeight(100);af->addRow(d->applied);d->remove=button(QStringLiteral("移除选定预设"),"spatial.preset-remove36",instances);af->addRow(d->remove);d->bakeStart=number(af,QStringLiteral("开始 / 秒"),"spatial.preset-bake-start36",0,maxSeconds,0,0,60);d->bakeEnd=number(af,QStringLiteral("结束 / 秒"),"spatial.preset-bake-end36",0,maxSeconds,6,0,60);d->bakeStep=number(af,QStringLiteral("采样步长 / 秒"),"spatial.preset-bake-step36",.01,3600,.04,.01,1);d->countLabel=new QLabel;d->countLabel->setObjectName("spatial.preset-bake-count36");d->countLabel->setWordWrap(true);af->addRow(d->countLabel);d->bake=button(QStringLiteral("转换选定区间为关键帧…"),"spatial.preset-bake36",instances);af->addRow(d->bake);connect(d->remove,&QPushButton::clicked,this,[this]{d->removeSelected();});connect(d->bake,&QPushButton::clicked,this,[this]{d->bakeSelected();});connect(d->applied,&QListWidget::currentItemChanged,this,[this]{if(d->refreshing)return;const auto selected=d->selectedApplied();if(d->model)for(const auto&i:d->model->presetInstances)if(u(i.instanceId)==selected){d->bakeStart->setValue(toSeconds(i.start));d->bakeEnd->setValue(toSeconds(i.end));break;}d->updateBakeCount();});for(auto*w:{d->bakeStart,d->bakeEnd,d->bakeStep})connect(w,&QDoubleSpinBox::valueChanged,this,[this]{d->updateBakeCount();});layout->addStretch();
  d->debounce=new QTimer(this);d->debounce->setInterval(60);d->debounce->setSingleShot(true);connect(d->debounce,&QTimer::timeout,this,[this]{if(d->session)d->updatePreview();});for(auto*w:findChildren<QDoubleSpinBox*>())if(w!=d->bakeStart&&w!=d->bakeEnd&&w!=d->bakeStep)connect(w,&QDoubleSpinBox::valueChanged,this,[this]{d->changed();});connect(d->generate,&QPushButton::clicked,this,[this]{d->closeColor();d->debounce->stop();d->updatePreview();});connect(d->reroll,&QPushButton::clicked,this,[this]{d->rerollAll();});connect(d->cancel,&QPushButton::clicked,this,[this]{d->cancelSession();d->tell(QStringLiteral("已取消预览；原画面保持不变。"));});connect(d->apply,&QPushButton::clicked,this,[this]{d->commit();});connect(d->append,&QPushButton::clicked,this,[this]{d->commit(true);});
  auto*thumbnailTimer=new QTimer(this);thumbnailTimer->setInterval(100);connect(thumbnailTimer,&QTimer::timeout,this,[this]{if(!isVisible())return;d->thumbTime+=.025;d->thumbnails();});thumbnailTimer->start();d->thumbnails();
 d->chooseStyle(styleCatalog().front());d->refreshing=false;installHoverHelp(this);d->refresh();
}
PresetPanel::~PresetPanel(){d->cancelSession();for(auto*child:findChildren<QObject*>())QObject::disconnect(child,nullptr,this,nullptr);}
void PresetPanel::setDocument(QString document){if(d->document==document)return;d->cancelSession();d->status->clear();d->document=std::move(document);d->layerSignature=QString(QChar(0));d->presetSignature=QString(QChar(0));d->refreshing=true;d->targets->clear();for(auto*c:{d->anchor,d->depthA,d->depthB})c->setCurrentIndex(0);if(d->model){d->start->setValue(d->model->timelinePosition);if(auto*row=dynamic_cast<NumericSlider*>(d->start->parentWidget()))row->refresh();}d->refreshing=false;d->refresh();}
void PresetPanel::refresh(){d->refresh();}
void PresetPanel::cancelCurrentPreview(){d->cancelSession();}
}
