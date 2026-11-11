#pragma once
#include <QAbstractButton>
#include <QCursor>
#include <QEvent>
#include <QElapsedTimer>
#include <QLabel>
#include <QPointer>
#include <QTimer>
#include <QToolTip>
#include <QWidget>

// Local to this Dock: no application style or desktop-sized translucent window.
class HoverHelp final : public QObject {
 QTimer timer;QElapsedTimer hoverClock;QPointer<QWidget> target;QWidget *root;
public:
 explicit HoverHelp(QWidget *parent):QObject(parent),root(parent){
  setObjectName("spatial.hover-help");setProperty("delayMs",1000);
  timer.setSingleShot(true);timer.setTimerType(Qt::PreciseTimer);timer.setInterval(1000);
  connect(&timer,&QTimer::timeout,this,[this]{
   setProperty("firedAfterMs",hoverClock.isValid()?hoverClock.elapsed():-1);
   if(target&&target->isVisible()&&target->rect().contains(target->mapFromGlobal(QCursor::pos()))){
    auto html=QString("<qt><table width='360'><tr><td>%1</td></tr></table></qt>").arg(target->toolTip().toHtmlEscaped().replace("\n","<br>"));
    QToolTip::showText(QCursor::pos()+QPoint(12,16),html,target,target->rect(),15000);
    setProperty("shown",true);
   }
  });
  for(auto *w:parent->findChildren<QWidget*>())w->installEventFilter(this);
 }
protected:
 bool eventFilter(QObject *object,QEvent *event)override{
  auto *w=qobject_cast<QWidget*>(object);if(!w)return false;
  if(event->type()==QEvent::ToolTip)return true;
  if(event->type()==QEvent::Enter){
   QWidget *candidate=w;while(candidate&&candidate!=root&&candidate->toolTip().isEmpty())candidate=candidate->parentWidget();
   if(candidate&&candidate!=root&&!candidate->toolTip().isEmpty()){target=candidate;hoverClock.restart();timer.start();setProperty("hoverTarget",candidate->objectName());setProperty("shown",false);}
  }else if(event->type()==QEvent::Leave||event->type()==QEvent::Hide||event->type()==QEvent::MouseButtonPress||event->type()==QEvent::Wheel){
   // Native tooltip window activation can emit Leave while the cursor remains
   // inside the original control. Do not cancel our own delayed tooltip then.
   const bool stillInside=target&&target->rect().contains(target->mapFromGlobal(QCursor::pos()));
   if(target&&(w==target||w->isAncestorOf(target)||target->isAncestorOf(w))&&!(event->type()==QEvent::Leave&&stillInside)){setProperty("lastCancel",w->objectName()+":"+QString::number(int(event->type())));timer.stop();target.clear();QToolTip::hideText();}
  }
  return false;
 }
};
inline void installHoverHelp(QWidget *root){
 for(auto*w:root->findChildren<QWidget*>()){
  if(w->toolTip().isEmpty()){
   if(auto*b=qobject_cast<QAbstractButton*>(w))w->setToolTip(b->text());
   else if(auto*l=qobject_cast<QLabel*>(w))w->setToolTip(l->text());
  }
 }
 new HoverHelp(root);
}
