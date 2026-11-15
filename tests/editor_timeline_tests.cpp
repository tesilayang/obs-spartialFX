#include "editor.hpp"
#include <QApplication>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QTimer>
#include <QMessageBox>
#include <QMenu>
#include <QMouseEvent>
#include <QKeyEvent>
#include <obs-module.h>
#include <iostream>
#include <cstdlib>
using namespace spatial;
extern "C" obs_module_t* obs_current_module(void){return nullptr;}
void check(bool ok,const char*message){if(!ok){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}std::cout<<"PASS: "<<message<<'\n';}
template<class T>T* field(QWidget&w,const char*name){auto*p=w.findChild<T*>(name);check(p!=nullptr,name);return p;}
void popupAction(const QString&name,bool confirm=true){QTimer::singleShot(0,qApp,[name,confirm]{for(auto*w:QApplication::topLevelWidgets())if(auto*m=qobject_cast<QMenu*>(w)){for(auto*a:m->actions())if(a->objectName()==name){if(confirm)QTimer::singleShot(0,qApp,[]{for(auto*w:QApplication::topLevelWidgets())if(auto*b=qobject_cast<QMessageBox*>(w)){if(auto*yes=b->button(QMessageBox::Yes))yes->click();else if(auto*ok=b->button(QMessageBox::Ok))ok->click();}});m->setActiveAction(a);QKeyEvent enter(QEvent::KeyPress,Qt::Key_Return,Qt::NoModifier);QApplication::sendEvent(m,&enter);return;}}});}
void context(CameraEditor&w,int row,const char*name,bool confirm=true){popupAction(name,confirm);QPointF pos(80,32+row*27+13);QMouseEvent event(QEvent::MouseButtonPress,pos,pos,Qt::RightButton,Qt::RightButton,Qt::NoModifier);QApplication::sendEvent(w.timeline,&event);QApplication::processEvents();}
int main(int argc,char**argv){std::cout.setf(std::ios::unitbuf);check(obs_startup("en-US",nullptr,nullptr),"libobs initialized");struct Shutdown{~Shutdown(){obs_shutdown();}} shutdown;QApplication app(argc,argv);StageModel model;Layer l;l.instanceId="actor";l.sourceUuid="source";l.label="角色";model.layers.push_back(l);model.light.instanceId="lamp";model.light.present=true;
 auto*def=fxDefinition("FX03.0");auto f=makeFx(*def);f.enabled=true;model.layers[0].fx.attached=true;model.layers[0].fx.groups[2]=true;model.layers[0].fx.effects.push_back(f);
 CameraEditor editor(&model);editor.resize(950,1200);editor.show();QApplication::processEvents();auto*objects=field<QComboBox>(editor,"spatial.editor-object");check(objects->count()==5,"object picker contains camera composite layer FX and light");
 objects->setCurrentIndex(objects->findData(lightHandleBase));auto*intensity=field<QDoubleSpinBox>(editor,"spatial.param32.intensity");check(intensity->value()==1.5,"light inspector starts at base value");editor.record();editor.seek(toTime(2));model.autoKey=true;intensity->setValue(4.5);check(model.light.animation["intensity"].contains(toTime(2)),"light inspector writes native key");editor.seek(toTime(1));check(std::abs(intensity->value()-3)<.001,"light inspector follows interpolated playhead");
 editor.undo();check(!model.light.animation["intensity"].contains(toTime(2)),"undo restores light channel");editor.redo();check(model.light.animation["intensity"].contains(toTime(2)),"redo restores light channel edit");
 int fx=0;for(auto&o:timelineObjects(model))if(o.ref.kind==TimelineKind::Fx)fx=o.handle;objects->setCurrentIndex(objects->findData(fx));auto*strength=field<QDoubleSpinBox>(editor,"spatial.param32.strength");check(strength->maximum()==1,"FX inspector uses catalog limits");editor.seek(0);strength->setValue(.25);editor.record();editor.seek(toTime(3));strength->setValue(.9);check(findFx(model.layers[0].fx,def->id)->animation["strength"].contains(toTime(3)),"FX parameter can be keyed from inspector");
 editor.selectLayer(0);model.timelinePosition=0;editor.refresh();field<QDoubleSpinBox>(editor,"spatial.param32.x")->setValue(100);editor.record();editor.seek(toTime(2));field<QDoubleSpinBox>(editor,"spatial.param32.x")->setValue(400);const auto original=model.layers[0].animation;
 context(editor,2,"spatial.copy-animation364",false);editor.seek(toTime(10));context(editor,2,"spatial.paste-animation364");check(model.layers[0].animation["x"].contains(toTime(12)),"right click paste aligns group to playhead");
 context(editor,2,"spatial.copy-layer364",false);context(editor,2,"spatial.paste-layer364",false);check(model.layers.size()==2&&model.layers[1].instanceId!="actor","right click paste creates unique independent layer");editor.undo();check(model.layers.size()==1,"duplicate layer undo is atomic");editor.redo();check(model.layers.size()==2,"duplicate layer redo restores instance");
 context(editor,2,"spatial.delete-object364");check(model.layers.size()==1&&model.layers[0].instanceId!="actor","confirmed context delete removes requested row");editor.undo();check(model.layers.size()==2&&model.layers[0].instanceId=="actor","context delete undo restores exact object");
 editor.selectLayer(0);QTimer::singleShot(0,qApp,[]{for(auto*w:QApplication::topLevelWidgets())if(auto*b=qobject_cast<QMessageBox*>(w))for(auto*button:b->buttons())if(b->buttonRole(button)==QMessageBox::RejectRole)button->click();});editor.reset(false);check(!model.layers[0].animation.empty(),"cancel reset preserves keys");QTimer::singleShot(0,qApp,[]{for(auto*w:QApplication::topLevelWidgets())if(auto*b=qobject_cast<QMessageBox*>(w))for(auto*button:b->buttons())if(b->buttonRole(button)==QMessageBox::AcceptRole)button->click();});editor.reset(false);check(model.layers[0].animation.empty(),"confirmed reset clears native preset compatible keys");
 model.layers[0].animation["x"].locked=true;const auto lockedKeys=model.layers[0].animation["x"].keys().size();editor.reset(false);check(model.layers[0].animation["x"].keys().size()==lockedKeys,"reset refuses locked parameter track");model.layers[0].animation["x"].locked=false;
 // FX identity must follow its owner when the material list moves.
 int ownerFx=*resolveTimelineObject(model,{TimelineKind::Fx,def->id,"actor"});objects->setCurrentIndex(objects->findData(ownerFx));editor.reorderLayer(0,1);check(editor.active==*resolveTimelineObject(model,{TimelineKind::Fx,def->id,"actor"}),"reordering retains selected FX owner identity");
 bool blankMenu=false;QTimer::singleShot(0,qApp,[&]{for(auto*w:QApplication::topLevelWidgets())if(auto*m=qobject_cast<QMenu*>(w)){blankMenu=true;for(auto*a:m->actions())if(!a->isSeparator())check(a->objectName()=="spatial.paste-layer364","blank timeline only offers layer paste");m->close();}});QPointF blank(80,editor.timeline->height()+30);QMouseEvent blankEvent(QEvent::MouseButtonPress,blank,blank,Qt::RightButton,Qt::RightButton,Qt::NoModifier);QApplication::sendEvent(editor.timeline,&blankEvent);check(blankMenu,"empty timeline context menu exercised");
 std::cout<<"PASS: editor objects, parameter controls, animation and right click operations\n";
}
