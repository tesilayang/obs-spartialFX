#include "fx_panel.hpp"
#include "hover_help.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QEventLoop>
#include <cstdio>
#include <stdexcept>
#include <windows.h>
#include <dbghelp.h>

namespace {
void pump(int ms=120){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
void check(bool ok,const char*name){std::fprintf(stderr,"%s: %s\n",ok?"PASS":"FAIL",name);if(!ok)throw std::runtime_error(name);}
spatial::StageModel fixture(){spatial::StageModel m;spatial::Layer l;l.instanceId="ui-owner";l.label="FX UI fixture";l.fx.attached=true;l.fx.groups.fill(true);for(const auto&d:spatial::fxCatalog()){auto f=spatial::makeFx(d);f.enabled=true;l.fx.effects.push_back(f);}m.layers.push_back(l);return m;}
void scrollTest(){
 auto model=fixture();QScrollArea scroll;scroll.setWidgetResizable(true);scroll.resize(780,600);auto*p=new FxPanel(&model);scroll.setWidget(p);p->changed=[p]{p->refresh();};scroll.show();pump();
 auto*bar=scroll.verticalScrollBar();check(bar->maximum()>1000,"fixture has a long FX page");
 const QString prefix="spatial.fx34.ui-owner.";
 QPointer<QCheckBox> selected=p->findChild<QCheckBox*>(prefix+"FX12.9.enabled");check(bool(selected),"lower effect exists");scroll.ensureWidgetVisible(selected,0,100);pump();const int before=bar->value();check(before>1000,"lower effect scrolled into view");
 selected->click();pump();std::fprintf(stderr,"scroll before=%d after=%d widgetAlive=%d\n",before,bar->value(),bool(selected));
 check(selected&&std::abs(bar->value()-before)<80,"effect toggle preserves controls and scroll position");
 selected->click();pump();check(selected&&selected->isChecked(),"same effect can be immediately reenabled");
 auto*group=p->findChild<QToolButton*>(prefix+"group12");group->click();pump();group->click();pump();check(selected&&selected->isVisible(),"group fold keeps parameter widgets");
 model.layers.front().fx.effects.erase(std::remove_if(model.layers.front().fx.effects.begin(),model.layers.front().fx.effects.end(),[](const auto&f){return f.id=="FX12.9";}),model.layers.front().fx.effects.end());p->refresh();pump();auto*removed=p->findChild<QCheckBox*>(prefix+"FX12.9.enabled");check(removed&&!removed->isChecked(),"external effect deletion refreshes controls safely");
 auto*aa=p->findChild<QCheckBox*>(prefix+"FX02.2.enabled");auto*fxaa=p->findChild<QCheckBox*>(prefix+"FX02.3.enabled");aa->click();aa->setChecked(true);pump();check(aa->isChecked()&&!fxaa->isChecked(),"AA exclusivity survives in-place refresh");
}
void hoverTest(){
 QWidget root;root.setGeometry(100,100,620,420);QToolButton button(&root);button.setGeometry(500,350,100,35);button.setToolTip("One second help: bounded to this OBS window.");installHoverHelp(&root);installHoverHelp(&root);root.show();root.activateWindow();pump();QCursor::setPos(button.mapToGlobal(button.rect().center()));QEvent enter(QEvent::Enter);QApplication::sendEvent(&button,&enter);pump(1150);
 bool native=false;for(auto*w:QApplication::topLevelWidgets())if(w->isVisible()&&w->windowType()==Qt::ToolTip){native=true;std::fprintf(stderr,"native tooltip rect=%d,%d %dx%d\n",w->x(),w->y(),w->width(),w->height());}
 check(!native,"hover help creates no native tooltip/fade surface");
 auto*bubble=root.findChild<QLabel*>("spatial.hover-bubble");check(bubble&&bubble->isVisible(),"one-second help remains available");check(root.rect().contains(bubble->geometry()),"help stays inside owning window bounds");
 QEvent deactivate(QEvent::ApplicationDeactivate);QApplication::sendEvent(qApp,&deactivate);pump();check(!bubble->isVisible(),"switching applications dismisses help");
 QApplication::sendEvent(&button,&enter);QApplication::sendEvent(qApp,&deactivate);pump(1150);check(!bubble->isVisible(),"pending help is cancelled when application deactivates");
}
}
LONG WINAPI crashTrace(EXCEPTION_POINTERS*e){auto process=GetCurrentProcess();SymInitialize(process,nullptr,TRUE);CONTEXT context=*e->ContextRecord;STACKFRAME64 frame{};frame.AddrPC={context.Rip,0,AddrModeFlat};frame.AddrFrame={context.Rbp,0,AddrModeFlat};frame.AddrStack={context.Rsp,0,AddrModeFlat};for(int i=0;i<24&&StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,GetCurrentThread(),&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr);++i){char storage[sizeof(SYMBOL_INFO)+1024]{};auto*symbol=reinterpret_cast<SYMBOL_INFO*>(storage);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=1023;DWORD64 offset=0;SymFromAddr(process,frame.AddrPC.Offset,&offset,symbol);std::fprintf(stderr,"TRACE %llx %s +%llu\n",frame.AddrPC.Offset,symbol->Name,offset);}return EXCEPTION_EXECUTE_HANDLER;}
int main(int argc,char**argv){SetUnhandledExceptionFilter(crashTrace);QApplication app(argc,argv);try{if(app.arguments().contains("--hover"))hoverTest();else scrollTest();check(!qApp->findChild<QObject*>("spatial.hover-help"),"destroying last plugin root removes application filter");return 0;}catch(const std::exception&e){std::fprintf(stderr,"%s\n",e.what());return 1;}}
