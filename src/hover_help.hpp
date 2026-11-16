#pragma once
#include <QAbstractButton>
#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QElapsedTimer>
#include <QLabel>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include <algorithm>
#include <vector>

// One controller for registered plugin widgets. Help is painted by an opaque
// child label, never a native Qt tooltip/fade window over the desktop.
class HoverHelp final : public QObject {
 QTimer timer,expiry;QElapsedTimer hoverClock;QPointer<QWidget> target;
 QPointer<QLabel> bubble;std::vector<QPointer<QWidget>> roots;
 bool registered(QWidget*w)const{for(const auto&r:roots)if(r&&(r==w||r->isAncestorOf(w)))return true;return false;}
 void dismiss(){timer.stop();expiry.stop();target.clear();if(bubble)bubble->hide();setProperty("shown",false);}
 void showHelp(){
  setProperty("firedAfterMs",hoverClock.isValid()?hoverClock.elapsed():-1);
  if(!target||!target->isVisible()||QApplication::activeWindow()!=target->window()||!target->rect().contains(target->mapFromGlobal(QCursor::pos()))){dismiss();return;}
  auto*host=target->window();if(!bubble||bubble->parentWidget()!=host){if(bubble)bubble->deleteLater();bubble=new QLabel(host);bubble->setObjectName("spatial.hover-bubble");bubble->setAttribute(Qt::WA_TransparentForMouseEvents);bubble->setFocusPolicy(Qt::NoFocus);bubble->setTextFormat(Qt::PlainText);bubble->setWordWrap(true);bubble->setMargin(8);bubble->setStyleSheet("QLabel { background:#f4f1e8; color:#1f252b; border:1px solid #92979b; border-radius:3px; }");}
  bubble->setText(target->toolTip());bubble->setFixedWidth(std::min(376,std::max(1,host->width()-16)));bubble->setMaximumHeight(std::max(1,host->height()-16));bubble->adjustSize();
  auto pos=host->mapFromGlobal(QCursor::pos()+QPoint(12,16));pos.setX(std::clamp(pos.x(),0,std::max(0,host->width()-bubble->width())));pos.setY(std::clamp(pos.y(),0,std::max(0,host->height()-bubble->height())));bubble->move(pos);bubble->show();bubble->raise();setProperty("shown",true);expiry.start();
 }
public:
 explicit HoverHelp(QObject*parent):QObject(parent){setObjectName("spatial.hover-help");setProperty("delayMs",1000);timer.setSingleShot(true);timer.setTimerType(Qt::PreciseTimer);timer.setInterval(1000);expiry.setSingleShot(true);expiry.setInterval(15000);connect(&timer,&QTimer::timeout,this,[this]{showHelp();});connect(&expiry,&QTimer::timeout,this,[this]{dismiss();});qApp->installEventFilter(this);}
 ~HoverHelp() override{if(qApp)qApp->removeEventFilter(this);if(bubble)delete bubble.data();}
 void addRoot(QWidget*root){roots.erase(std::remove_if(roots.begin(),roots.end(),[](const auto&r){return r.isNull();}),roots.end());for(const auto&r:roots)if(r==root||r->isAncestorOf(root))return;roots.erase(std::remove_if(roots.begin(),roots.end(),[root](const auto&r){return root->isAncestorOf(r);}),roots.end());roots.push_back(root);connect(root,&QObject::destroyed,this,[this,root]{roots.erase(std::remove_if(roots.begin(),roots.end(),[root](const auto&r){return r.isNull()||r.data()==root;}),roots.end());if(roots.empty())delete this;});}
protected:
 bool eventFilter(QObject*object,QEvent*event)override{
  const auto type=event->type();
  if(type==QEvent::ApplicationDeactivate||type==QEvent::WindowDeactivate||type==QEvent::KeyPress||type==QEvent::MouseButtonPress||type==QEvent::Wheel){dismiss();return false;}
  auto*w=qobject_cast<QWidget*>(object);if(!w)return false;
  if(target&&(type==QEvent::Hide||type==QEvent::Close||type==QEvent::Destroy)&&(w==target||w->isAncestorOf(target))){dismiss();return false;}
  if(!registered(w))return false;
  if(type==QEvent::ToolTip)return true;
  if(type==QEvent::Enter){QWidget*candidate=w;while(candidate&&registered(candidate)&&candidate->toolTip().isEmpty())candidate=candidate->parentWidget();if(candidate&&registered(candidate)&&!candidate->toolTip().isEmpty()&&candidate!=target){dismiss();target=candidate;hoverClock.restart();timer.start();setProperty("hoverTarget",candidate->objectName());}}
  else if(type==QEvent::Leave&&target&&!target->rect().contains(target->mapFromGlobal(QCursor::pos())))dismiss();
  return false;
 }
};
inline void installHoverHelp(QWidget*root){
 for(auto*w:root->findChildren<QWidget*>())if(w->toolTip().isEmpty()){if(auto*b=qobject_cast<QAbstractButton*>(w))w->setToolTip(b->text());else if(auto*l=qobject_cast<QLabel*>(w))w->setToolTip(l->text());}
 HoverHelp*controller=nullptr;for(auto*c:qApp->children())if(c->objectName()=="spatial.hover-help")controller=static_cast<HoverHelp*>(c);if(!controller)controller=new HoverHelp(qApp);controller->addRoot(root);
}
