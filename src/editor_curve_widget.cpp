#include "editor.hpp"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVariantList>
#include <algorithm>
#include <cmath>
using namespace spatial;
CurveWidget::CurveWidget(QWidget*p):QWidget(p){
 setObjectName("spatial.curve32");setMinimumHeight(220);setFocusPolicy(Qt::StrongFocus);setMouseTracking(true);
 setToolTip(QString::fromUtf8("横轴是时间（秒），纵轴是当前参数的实际数值。单击菱形选择关键帧；拖动菱形改变时间与数值。圆点控制相邻区间的贝塞尔速度。拖动时坐标范围保持固定；Esc取消本次拖动。锁定关键帧位置时仍可上下改值，但不能改变时间。"));
}
void CurveWidget::setCurve(const Curve&c,TimeUs t,const std::string&id){
 if(drag)return;curve=c;selected=t;parameterId=id;geometry(true);update();
}
void CurveWidget::fit(){if(!drag){geometry(true);update();}}
QPointF CurveWidget::point(TimeUs t,double value)const{
 return {plot.left()+(toSeconds(t)-timeLow)/(timeHigh-timeLow)*plot.width(),plot.bottom()-(value-valueLow)/(valueHigh-valueLow)*plot.height()};
}
void CurveWidget::geometry(bool fitRange){
 plot=QRectF(64,20,std::max(20,width()-84),std::max(20,height()-60));
 const auto&keys=curve.keys();segment=false;
 if(keys.size()>=2){
  auto it=std::upper_bound(keys.begin(),keys.end(),selected,[](TimeUs t,const AnimKey&k){return t<k.time;});
  if(it==keys.end())--it;if(it==keys.begin())++it;left=*(it-1);right=*it;materializeHandles(left,right,curve.shortest);segment=true;
 }
 if(fitRange){
  timeLow=keys.empty()?0:toSeconds(keys.front().time);timeHigh=keys.empty()?6:toSeconds(keys.back().time);
  if(timeHigh-timeLow<.001)timeHigh=timeLow+1;
  valueLow=keys.empty()?0:keys.front().value;valueHigh=valueLow;
  for(const auto&key:keys){valueLow=std::min(valueLow,key.value);valueHigh=std::max(valueHigh,key.value);}
  for(size_t i=1;i<keys.size();++i){auto a=keys[i-1],b=keys[i];materializeHandles(a,b,curve.shortest);double end=segmentEndValue(a,b,curve.shortest);valueLow=std::min({valueLow,a.outY,b.inY,end});valueHigh=std::max({valueHigh,a.outY,b.inY,end});}
  double padding=std::max(.1,(valueHigh-valueLow)*.15);valueLow-=padding;valueHigh+=padding;
 }
 if(segment){
  p0=point(left.time,left.value);p3=point(right.time,segmentEndValue(left,right,curve.shortest));
  p1=point(left.time+TimeUs(double(right.time-left.time)*left.outX),left.outY);
  p2=point(left.time+TimeUs(double(right.time-left.time)*right.inX),right.inY);
 }
 setProperty("segmentStartUs",segment?QVariant::fromValue<qlonglong>(left.time):QVariant());
 setProperty("segmentEndUs",segment?QVariant::fromValue<qlonglong>(right.time):QVariant());
 setProperty("outHandle",segment?p1:QPointF());setProperty("inHandle",segment?p2:QPointF());
 setProperty("valueViewSpan",valueHigh-valueLow);setProperty("plotRect",plot);setProperty("keyCount",int(keys.size()));setProperty("selectedTimeUs",QVariant::fromValue<qlonglong>(selected));
}
void CurveWidget::paintEvent(QPaintEvent*){
 geometry(false);QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor("#17212d"));
 p.setPen(QColor("#334456"));for(int i=0;i<=4;++i){double y=plot.top()+plot.height()*i/4;p.drawLine(QPointF(plot.left(),y),QPointF(plot.right(),y));p.setPen(QColor("#b3c5d7"));p.drawText(QRectF(0,y-10,58,20),Qt::AlignRight|Qt::AlignVCenter,QString::number(valueHigh-(valueHigh-valueLow)*i/4,'g',5));p.setPen(QColor("#334456"));}
 for(int i=0;i<=4;++i){double x=plot.left()+plot.width()*i/4;p.drawLine(QPointF(x,plot.top()),QPointF(x,plot.bottom()));p.setPen(QColor("#b3c5d7"));p.drawText(QRectF(x-32,plot.bottom()+8,64,20),Qt::AlignCenter,QString::number(timeLow+(timeHigh-timeLow)*i/4,'g',5)+"s");p.setPen(QColor("#334456"));}
 if(curve.keys().empty())return;
 p.save();p.setClipRect(plot.adjusted(-7,-7,7,7));QPainterPath path;auto displayCurve=curve;displayCurve.enabled=true;
 int samples=std::max(2,std::min(1600,int(plot.width()*2)));for(int i=0;i<=samples;++i){double t=timeLow+(timeHigh-timeLow)*i/samples;auto q=point(toTime(t),displayCurve.value(toTime(t),curve.keys().front().value));if(i==0)path.moveTo(q);else path.lineTo(q);}
 p.setPen(QPen(QColor("#58d9cc"),2));p.drawPath(path);
 if(segment&&left.interpolation!=Interpolation::Hold){p.setPen(QColor("#cfa656"));p.drawLine(p0,p1);p.drawLine(p3,p2);p.setBrush(readOnly?QColor("#77818a"):QColor("#f1cc80"));p.drawEllipse(p1,5,5);p.drawEllipse(p2,5,5);}
 double lastX=-1e100;for(const auto&key:curve.keys()){
  QPointF q=point(key.time,key.value);bool sel=key.time==selected;if(q.x()-lastX<2&&!sel)continue;lastX=q.x();
  QPolygonF diamond;diamond<<q+QPointF(0,-6)<<q+QPointF(6,0)<<q+QPointF(0,6)<<q+QPointF(-6,0);
  p.setPen(sel?QColor("#ffffff"):QColor("#7bafb5"));p.setBrush(sel?QColor("#ffcb6d"):QColor("#263f50"));p.drawPolygon(diamond);
 }p.restore();
 setProperty("selectedPoint",point(selected,displayCurve.value(selected,curve.keys().front().value)));
}
void CurveWidget::mousePressEvent(QMouseEvent*event){
 if(event->button()!=Qt::LeftButton)return;setFocus();geometry(false);origin=event->position();moved=false;dragCurve=curve;dragSelected=selected;
 // Keys have priority over a coincident handle on flat/hold segments.
 double best=11;const AnimKey*hit=nullptr;
 for(const auto&k:curve.keys()){double distance=QLineF(point(k.time,k.value),origin).length();if(distance<best){best=distance;hit=&k;}}
 if(hit){originalKey=*hit;selected=dragSelected=hit->time;if(selectKey)selectKey(selected);geometry(true);drag=readOnly?0:3;update();return;}
 if(readOnly||!segment||left.interpolation==Interpolation::Hold)return;
 if(QLineF(origin,p1).length()<11)drag=1;else if(QLineF(origin,p2).length()<11)drag=2;
}
void CurveWidget::change(QPointF pointAt){
 if(!drag||plot.width()<1||plot.height()<1)return;
 const auto*meta=parameter(parameterId);double low=meta?meta->minimum:-100000,high=meta?meta->maximum:100000;
 double value=valueLow+(plot.bottom()-std::clamp(pointAt.y(),plot.top(),plot.bottom()))/plot.height()*(valueHigh-valueLow);value=std::clamp(value,low,high);
 if(drag==3){
  auto next=originalKey;double seconds=timeLow+(std::clamp(pointAt.x(),plot.left(),plot.right())-plot.left())/plot.width()*(timeHigh-timeLow);
  next.time=positionsLocked?originalKey.time:toTime(std::max(0.0,seconds));next.value=value;
  if(next.time!=originalKey.time&&dragCurve.contains(next.time))return;
  curve=dragCurve;curve.erase(originalKey.time);curve.put(next);selected=next.time;
 }else{
  double x=std::clamp((pointAt.x()-p0.x())/std::max(1.0,p3.x()-p0.x()),0.0,1.0);
  left.interpolation=Interpolation::Bezier;
  if(drag==1){left.outX=std::min(x,right.inX);left.outY=value;left.outAbsolute=true;}
  else{right.inX=std::max(x,left.outX);right.inY=value;right.inAbsolute=true;}
  curve.put(left);curve.put(right);
 }moved=true;geometry(false);update();
}
void CurveWidget::mouseMoveEvent(QMouseEvent*event){if(drag&&QLineF(origin,event->position()).length()>2)change(event->position());}
void CurveWidget::mouseReleaseEvent(QMouseEvent*){
 int action=drag;drag=0;if(!moved){geometry(true);update();return;}moved=false;
 if(action==3&&editKey){for(const auto&key:curve.keys())if(key.time==selected){editKey(originalKey.time,key);break;}}
 else if(action&&handles)handles(left,right);
}
void CurveWidget::keyPressEvent(QKeyEvent*event){
 if(event->key()==Qt::Key_Escape&&drag){curve=dragCurve;selected=dragSelected;drag=0;moved=false;geometry(true);update();event->accept();return;}
 QWidget::keyPressEvent(event);
}
