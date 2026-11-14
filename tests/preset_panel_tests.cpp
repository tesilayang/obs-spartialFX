#include "presets/panel.hpp"
#include "presets/composer.hpp"
#include "widgets.hpp"
#include <QApplication>
#include <QPushButton>
#include <QToolButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QListWidget>
#include <QLabel>
#include <QTimer>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QScrollArea>
#include <QScrollBar>
#include <cstdio>
#include <stdexcept>
#include <map>
#include <set>

template<class T>T*control(QWidget&w,const char*n){auto*r=w.findChild<T*>(n);if(!r)throw std::runtime_error(n);return r;}
void verify(bool v,const char*w){std::fprintf(stderr,"%s: %s\n",v?"PASS":"FAIL",w);if(!v)throw std::runtime_error(w);}
void pump(int ms=100){QEventLoop loop;QTimer::singleShot(ms,&loop,&QEventLoop::quit);loop.exec();}
QJsonObject recipe(const spatial::StageModel&m){return m.presetInstances.empty()?QJsonObject{}:QJsonDocument::fromJson(QByteArray::fromStdString(m.presetInstances.back().definitionSnapshot)).object();}
std::string document(const spatial::StageModel&m){auto*d=spatial::modelData(m);const std::string v=obs_data_get_json(d);obs_data_release(d);return v;}
std::string output(spatial::StageModel m,int scope,bool ignoreColors=false){
 // Scope 0 is all output, 1 is material/camera/mask output, 2 is decoration/mask output.
 std::map<std::string,std::string>ids;for(size_t i=0;i<m.geometries.size();++i)ids[m.geometries[i].instanceId]="geometry-"+std::to_string(i);
 if(scope==1)m.geometries.erase(std::remove_if(m.geometries.begin(),m.geometries.end(),[](const auto&g){return g.mode!=spatial::presets::Mode::Mask;}),m.geometries.end());
 if(scope==2){m.cameraAnimation.clear();for(auto&l:m.layers)l.animation.clear();}
 m.motionBlocks.erase(std::remove_if(m.motionBlocks.begin(),m.motionBlocks.end(),[&](const auto&b){return scope==1?ids.count(b.targetId)!=0:scope==2?ids.count(b.targetId)==0:false;}),m.motionBlocks.end());
 for(auto&g:m.geometries){g.instanceId=ids[g.instanceId];g.ownerId="owner";if(ignoreColors)g.color={};}
 for(size_t i=0;i<m.motionBlocks.size();++i){auto&b=m.motionBlocks[i];b.instanceId="block-"+std::to_string(i);b.ownerId="owner";b.label="block";if(ids.count(b.targetId))b.targetId=ids[b.targetId];}
 m.presetInstances.clear();return document(m);
}
void verifyStalePreviewCannotOverwriteTimeline(){
 spatial::StageModel model;spatial::Layer layer;layer.instanceId="stale-target";layer.label="Target";model.layers={layer};
 spatial::presets::PresetPanel panel(&model);spatial::StageModel preview;int previews=0,commits=0,cancels=0;
 panel.previewDocument=[&](const auto&next){preview=next;++previews;};panel.cancelPreview=[&]{++cancels;};panel.applyDocument=[&](const auto&next){model=next;++commits;};panel.setDocument("stale-stage");
 control<QPushButton>(panel,"spatial.preset-generate361")->click();verify(previews==1,"stale guard fixture has a real compiled preview");
 model.layers.front().plane.position.x=321;const auto edited=document(model);control<QPushButton>(panel,"spatial.preset-apply36")->click();
 verify(commits==0&&document(model)==edited&&cancels==1,"applying stale preview preserves intervening timeline edit");
}
struct AppendFixture {
 spatial::StageModel model,last;spatial::presets::PresetPanel panel;int previews=0,commits=0,cancels=0;
 explicit AppendFixture(bool conflict=false):panel(&model){
  spatial::Layer a;a.instanceId="append-a";a.label="A";spatial::Layer b=a;b.instanceId="append-b";b.label="B";model.layers={a,b};
  auto&curve=model.layers.front().animation[conflict?"x":"y"];curve.enabled=true;
  spatial::AnimKey first{0,10,spatial::Interpolation::Bezier};first.outAbsolute=true;first.outY=123;curve.put(first);
  spatial::AnimKey end{4012345,340};end.inAbsolute=true;end.inY=234;curve.put(end);
  panel.videoFrameRate=[] {return std::make_pair(30000u,1001u);};
  panel.previewDocument=[this](const auto&next){last=next;++previews;};panel.cancelPreview=[this]{++cancels;};panel.applyDocument=[this](const auto&next){model=next;++commits;};panel.setDocument("append-stage");
  select("spatial.preset-motion-part361","M04");select("spatial.preset-mask-part361","T09");select("spatial.preset-decor-part361","D19");
  control<QDoubleSpinBox>(panel,"spatial.preset-seed361")->setValue(12345);
  control<QDoubleSpinBox>(panel,"spatial.preset-period36")->setValue(2);
  control<QComboBox>(panel,"spatial.preset-depth36")->setCurrentIndex(3);
 }
 ~AppendFixture(){panel.cancelCurrentPreview();panel.previewDocument={};panel.cancelPreview={};panel.applyDocument={};}
 void select(const char*name,const char*value){auto*w=control<QComboBox>(panel,name);w->setCurrentIndex(w->findData(value));}
 QPushButton*action(const char*name){return control<QPushButton>(panel,name);}
 void generate(){action("spatial.preset-generate361")->click();}
 void append(){action("spatial.preset-append363")->click();}
 void cancel(){action("spatial.preset-cancel36")->click();}
};
bool sameKey(const spatial::AnimKey&a,const spatial::AnimKey&b){return a.time==b.time&&a.value==b.value&&a.interpolation==b.interpolation&&a.outX==b.outX&&a.outY==b.outY&&a.inX==b.inX&&a.inY==b.inY&&a.outAbsolute==b.outAbsolute&&a.inAbsolute==b.inAbsolute&&a.legacyFrom==b.legacyFrom&&a.legacyTo==b.legacyTo;}
void verifyRetainedKeys(const spatial::StageModel&before,const spatial::StageModel&after){
 for(size_t i=0;i<before.layers.size();++i)for(const auto&entry:before.layers[i].animation){const auto found=after.layers[i].animation.find(entry.first);verify(found!=after.layers[i].animation.end(),"append retains existing parameter tracks");const auto&keys=found->second.keys();for(const auto&old:entry.second.keys()){auto key=std::find_if(keys.begin(),keys.end(),[&](const auto&v){return v.time==old.time;});verify(key!=keys.end()&&sameKey(*key,old),"append retains every existing key and tangent exactly");}}
}
QJsonObject withoutStart(QJsonObject value){value.remove("start_us");return value;}
void verifyCrossStyleDecorationLock(){
 AppendFixture fixed;fixed.select("spatial.preset-decor-part361","");fixed.select("spatial.preset-mask-part361","none");fixed.generate();
 control<QCheckBox>(fixed.panel,"spatial.preset-lock-decor361")->setChecked(true);pump();const auto before=fixed.last;
 fixed.action("spatial.preset-style-dream")->click();pump();verify(output(fixed.last,2,true)==output(before,2,true),"decoration lock preserves exact mixed shapes and channels across style change");
}
void verifyInactiveTrackAppendProtection(){
 AppendFixture inactive;for(auto&layer:inactive.model.layers)layer.animation.clear();auto&curve=inactive.model.layers.front().animation["x"];curve.enabled=false;curve.put({0,10});curve.put({4000000,340});
 inactive.generate();const auto before=document(inactive.model);inactive.append();
 verify(inactive.commits==0&&document(inactive.model)==before&&inactive.cancels==1&&!inactive.action("spatial.preset-append363")->isEnabled(),"append rejects reactivation of retained disabled keys and restores original preview");
}
void verifyAppendGuardCollision(){
 AppendFixture guard;for(auto&layer:guard.model.layers)layer.animation.clear();guard.panel.videoFrameRate=[] {return std::make_pair(60u,1u);};auto&curve=guard.model.layers.front().animation["x"];curve.enabled=true;curve.put({0,10});
 spatial::AnimKey end{999999,340,spatial::Interpolation::Bezier};end.outAbsolute=end.inAbsolute=true;end.outX=.2;end.outY=456;end.inX=.7;end.inY=234;curve.put(end);const auto before=guard.model;
 guard.generate();guard.append();verify(guard.commits==1&&guard.model.presetInstances.back().start==1000000,"append at sixty fps keeps exact next frame despite one-microsecond gap");verifyRetainedKeys(before,guard.model);
}
void verifyAppendWorkflow(){
 AppendFixture normal;auto*apply=normal.action("spatial.preset-apply36");auto*append=normal.action("spatial.preset-append363");verify(!append->isEnabled(),"append requires a compiled preview candidate");
 const auto baseline=normal.model;normal.generate();verify(normal.previews==1&&apply->isEnabled()&&append->isEnabled(),"normal preview offers apply and append");const auto candidate=recipe(normal.last);normal.append();
 verify(normal.commits==1&&normal.cancels==1&&normal.model.presetInstances.size()==1,"append uses one apply transaction and clears temporary preview");
 // 4,012,345 us lies before frame 121: ceil(121 * 1,001,000,000 / 30,000).
 const spatial::TimeUs expectedStart=spatial::TimeUs((121ULL*1001000000ULL+29999)/30000);const auto actualStart=normal.model.presetInstances.back().start;
 std::fprintf(stderr,"append frame oracle: actual=%lld expected=%lld end=%lld\n",static_cast<long long>(actualStart),static_cast<long long>(expectedStart),static_cast<long long>(normal.model.presetInstances.back().end));
 verify(actualStart==expectedStart&&normal.model.presetInstances.back().end==expectedStart+2000000,"append starts at strict next 30000/1001 OBS frame from baseline content end");
 verify(withoutStart(recipe(normal.model))==withoutStart(candidate),"append freezes templates seeds palette duration and selected materials");verifyRetainedKeys(baseline,normal.model);
 verify(!normal.model.loop,"append preserves whole timeline loop setting");
 control<QDoubleSpinBox>(normal.panel,"spatial.preset-start36")->setValue(4.5);normal.generate();verify(!normal.action("spatial.preset-apply36")->isEnabled()&&normal.action("spatial.preset-append363")->isEnabled(),"overlap generation becomes append-only tail preview");
 verify(normal.last.presetInstances.back().start==6039367,"second tail preview accounts for retained finite end guard keys");
 const auto beforeCancel=document(normal.model);normal.cancel();verify(document(normal.model)==beforeCancel&&!normal.action("spatial.preset-append363")->isEnabled(),"cancel tail preview leaves timeline untouched");
 AppendFixture fallback(true);const auto fallbackBaseline=fallback.model;fallback.generate();verify(fallback.previews==1&&!fallback.action("spatial.preset-apply36")->isEnabled()&&fallback.action("spatial.preset-append363")->isEnabled(),"existing animation interval conflict previews at tail");
 verify(fallback.last.presetInstances.back().start==4037367&&spatial::toTime(fallback.last.timelinePosition)==4037367&&control<QLabel>(fallback.panel,"spatial.preset-status36")->text().startsWith(QStringLiteral("尾部预览")),"tail preview reports and seeks to its actual start");const auto tailCandidate=recipe(fallback.last);fallback.append();verify(recipe(fallback.model)==tailCandidate&&fallback.commits==1,"tail append commits the previewed candidate without reroll");verifyRetainedKeys(fallbackBaseline,fallback.model);
 AppendFixture stale;stale.generate();stale.model.layers.back().plane.position.y=47;const auto edited=document(stale.model);stale.append();verify(stale.commits==0&&document(stale.model)==edited&&!stale.action("spatial.preset-append363")->isEnabled(),"append rejects stale baseline without overwriting edits");
 AppendFixture locked;locked.model.layers.front().animation["x"].locked=true;locked.generate();verify(locked.previews==0&&!locked.action("spatial.preset-append363")->isEnabled(),"locked parameter failure never becomes tail preview");
 AppendFixture budget;budget.model.presetInstances.resize(256);budget.generate();verify(budget.previews==0&&!budget.action("spatial.preset-append363")->isEnabled(),"preset budget failure never becomes tail preview");
 AppendFixture invalid;invalid.select("spatial.preset-motion-part361","M44");auto*targets=control<QListWidget>(invalid.panel,"spatial.preset-targets361");targets->item(1)->setCheckState(Qt::Unchecked);invalid.generate();verify(invalid.previews==0&&!invalid.action("spatial.preset-append363")->isEnabled(),"invalid two-material composition never becomes tail preview");
 AppendFixture unavailable;unavailable.panel.videoFrameRate=[] {return std::make_pair(0u,0u);};unavailable.generate();unavailable.append();verify(unavailable.commits==0&&unavailable.model.presetInstances.empty(),"append never invents frame rate when OBS video unavailable");
 AppendFixture empty;for(auto&layer:empty.model.layers)layer.animation.clear();empty.generate();empty.append();verify(empty.model.presetInstances.back().start==0,"empty timeline append begins at zero");
 AppendFixture zero;for(auto&layer:zero.model.layers)layer.animation.clear();auto&zeroCurve=zero.model.layers.front().animation["y"];zeroCurve.enabled=true;zeroCurve.put({0,25});zero.generate();zero.append();verify(zero.model.presetInstances.back().start==33367,"retained zero-time key appends on next frame instead of treating timeline as empty");
 AppendFixture boundary;for(auto&layer:boundary.model.layers)layer.animation.clear();auto&boundaryCurve=boundary.model.layers.front().animation["y"];boundaryCurve.enabled=true;boundaryCurve.put({1001000,25});boundary.generate();boundary.append();verify(boundary.model.presetInstances.back().start==1034367,"exact fractional-rate frame boundary advances one full frame");
 AppendFixture loop;control<QCheckBox>(loop.panel,"spatial.preset-loop36")->setChecked(true);loop.generate();loop.append();verify(!loop.model.loop&&!loop.model.motionBlocks.empty(),"appending periodic candidate keeps whole timeline loop disabled");
 AppendFixture ordinary;control<QDoubleSpinBox>(ordinary.panel,"spatial.preset-start36")->setValue(10);control<QCheckBox>(ordinary.panel,"spatial.preset-loop36")->setChecked(true);ordinary.generate();ordinary.action("spatial.preset-apply36")->click();verify(ordinary.commits==1&&ordinary.model.presetInstances.back().start==10000000&&ordinary.model.loop,"ordinary apply keeps requested start and loop behavior");
 AppendFixture pending;pending.generate();control<QDoubleSpinBox>(pending.panel,"spatial.preset-period36")->setValue(3);pending.append();verify(pending.commits==1&&pending.model.presetInstances.back().end-pending.model.presetInstances.back().start==3000000,"append flushes pending preview controls before freezing candidate");
 AppendFixture reevaluate;reevaluate.select("spatial.preset-motion-part361","M17");reevaluate.select("spatial.preset-mask-part361","none");control<QComboBox>(reevaluate.panel,"spatial.preset-depth36")->setCurrentIndex(1);auto*refA=control<QComboBox>(reevaluate.panel,"spatial.preset-depth-a36");auto*refB=control<QComboBox>(reevaluate.panel,"spatial.preset-depth-b36");refA->setCurrentIndex(refA->findData("append-a"));refB->setCurrentIndex(refB->findData("append-b"));reevaluate.generate();const double initialY=recipe(reevaluate.last)["plane"].toObject()["y"].toDouble();reevaluate.append();verify(initialY==5&&recipe(reevaluate.model)["plane"].toObject()["y"].toDouble()==170,"append recompiles placement against scene evaluated at tail");
 AppendFixture palette;palette.generate();std::set<QString>colors;double low=1,high=0;for(int i=0;i<8;++i){palette.action("spatial.preset-reroll-colors361")->click();const auto a=recipe(palette.last)["color_a"].toObject();QColor c=QColor::fromRgbF(a["r"].toDouble(),a["g"].toDouble(),a["b"].toDouble());colors.insert(c.name());low=std::min(low,double(c.hsvHueF()));high=std::max(high,double(c.hsvHueF()));}verify(colors.size()>=4&&high-low>.25,"style rerolls explore coordinated palettes beyond tiny hue shifts");
 control<QCheckBox>(palette.panel,"spatial.preset-lock-colors361")->setChecked(true);const QJsonValue fixed=recipe(palette.last).value("color_a");palette.action("spatial.preset-reroll361")->click();verify(recipe(palette.last)["color_a"]==fixed,"locked palette survives full reroll");
 AppendFixture mixed;for(auto&layer:mixed.model.layers)layer.animation.clear();mixed.select("spatial.preset-decor-part361","");mixed.select("spatial.preset-mask-part361","none");mixed.generate();verify(recipe(mixed.last)["decoration_ids"].toArray().size()>=3,"panel automatic composition contains mixed decoration families");const auto beforeMotion=mixed.last;mixed.action("spatial.preset-reroll-motion361")->click();verify(output(mixed.last,2)==output(beforeMotion,2),"motion reroll freezes full mixed-decoration geometry and channels");const auto beforePalette=mixed.last;mixed.action("spatial.preset-reroll-colors361")->click();verify(output(mixed.last,0,true)==output(beforePalette,0,true),"palette reroll freezes full mixed-decoration geometry and channels");
 control<QCheckBox>(mixed.panel,"spatial.preset-lock-decor361")->setChecked(true);const auto beforeLock=mixed.last;mixed.action("spatial.preset-reroll361")->click();verify(recipe(mixed.last)["decoration_ids"]==recipe(beforeLock)["decoration_ids"]&&recipe(mixed.last)["decoration_seed"]==recipe(beforeLock)["decoration_seed"]&&output(mixed.last,2,true)==output(beforeLock,2,true),"decoration lock freezes every mixed family and layout across full reroll");
}
int main(int argc,char**argv){QApplication app(argc,argv);try{
 if(app.arguments().contains("--append-disabled")){verifyInactiveTrackAppendProtection();return 0;}
 if(app.arguments().contains("--append-guard")){verifyAppendGuardCollision();return 0;}
 verifyInactiveTrackAppendProtection();
 verifyAppendGuardCollision();
 verifyStalePreviewCannotOverwriteTimeline();
 verifyCrossStyleDecorationLock();
 verifyAppendWorkflow();
 spatial::StageModel m;spatial::Layer a;a.instanceId="a";a.label="A";a.sourceUuid="same";spatial::Layer b=a;b.instanceId="b";b.label="B";spatial::Layer locked=a;locked.instanceId="locked";locked.label="Locked";locked.locked=true;m.layers={a,b,locked};
 spatial::presets::PresetPanel panel(&m);panel.resize(440,600);panel.show();int previews=0,cancels=0,commits=0;spatial::StageModel last;
 panel.previewDocument=[&](const auto&v){last=v;++previews;};panel.cancelPreview=[&]{++cancels;};panel.applyDocument=[&](const auto&v){m=v;++commits;};panel.setDocument("stage-a");pump(10);
 auto*list=control<QListWidget>(panel,"spatial.preset-targets361");verify(list->count()==3&&list->item(0)->checkState()==Qt::Checked&&list->item(1)->checkState()==Qt::Checked&&list->item(2)->checkState()==Qt::Unchecked&&!(list->item(2)->flags()&Qt::ItemIsEnabled),"existing unlocked materials selected; locked excluded");
 auto*details=control<QToolButton>(panel,"spatial.preset-details-toggle361");verify(!details->isChecked(),"details initially folded");verify(!panel.findChild<QWidget*>("spatial.preset-legacy36"),"whole-stage preset UI removed");
 for(const auto&s:spatial::presets::styleCatalog())verify(control<QPushButton>(panel,("spatial.preset-style-"+std::string(s.id)).c_str())->isCheckable(),"eight style cards");
 verify(!control<QCheckBox>(panel,"spatial.preset-camera36")->isChecked(),"camera defaultoff");verify(control<QComboBox>(panel,"spatial.preset-mask-part361")->currentData()=="none","mask defaultoff");
 auto*generate=control<QPushButton>(panel,"spatial.preset-generate361");auto*apply=control<QPushButton>(panel,"spatial.preset-apply36");auto*append=control<QPushButton>(panel,"spatial.preset-append363");auto*cancel=control<QPushButton>(panel,"spatial.preset-cancel36");auto*reroll=control<QPushButton>(panel,"spatial.preset-reroll361");
 auto*scroll=control<QScrollArea>(panel,"spatial.preset-content-scroll361");auto*footer=control<QWidget>(panel,"spatial.preset-footer361");auto visible=[&](QWidget*w){return panel.rect().contains(QRect(w->mapTo(&panel,QPoint()),w->size()));};verify(visible(generate)&&visible(apply)&&visible(append)&&visible(cancel)&&visible(reroll)&&!scroll->widget()->isAncestorOf(footer),"primary actions remain visible in 440x600 dock outside content scroller");details->click();pump(10);scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());pump(10);verify(visible(generate)&&visible(apply)&&visible(append)&&visible(cancel)&&visible(reroll),"expanded details and content scrolling never hide primary actions");details->click();scroll->verticalScrollBar()->setValue(0);
 verify(generate->isEnabled()&&!apply->isEnabled()&&!cancel->isEnabled(),"beginner actions initial state");generate->click();verify(previews==1&&m.presetInstances.empty()&&last.presetInstances.size()==1&&last.presetInstances.back().definitionId=="composition","generate transient shared composition");const auto firstSeed=last.presetInstances.back().seed;reroll->click();verify(previews==2&&last.presetInstances.size()==1&&last.presetInstances.back().seed!=firstSeed,"reroll replaces same baseline without accumulation");
 auto*strength=control<QDoubleSpinBox>(panel,"spatial.preset-strength36");const auto seed=last.presetInstances.back().seed;strength->setValue(.73);pump();verify(last.presetInstances.back().seed==seed&&last.presetInstances.size()==1,"parameter changes retain seed and one transaction");
 const auto prior=document(last);control<QPushButton>(panel,"spatial.preset-color-a36")->click();pump(5);auto*dialog=panel.findChild<QColorDialog*>();verify(dialog&&dialog->isVisible(),"color dialog modeless");dialog->setCurrentColor(QColor("#11cc55"));verify(control<QLabel>(panel,"spatial.preset-color-preview-a361")->text()=="#FF11CC55","full-width color preview hex");verify(std::any_of(last.geometries.begin(),last.geometries.end(),[](const auto&g){return std::abs(g.color.x-17./255)<.001&&std::abs(g.color.y-204./255)<.001;}),"color live updates generated geometry");dialog->reject();pump(5);verify(document(last)==prior&&cancel->isEnabled(),"color cancel restores exact prior preview");
 apply->click();verify(commits==1&&cancels==1&&m.presetInstances.size()==1&&!apply->isEnabled(),"apply one callback and cancel transient preview");const auto saved=document(m);auto*d=obs_data_create_from_json(saved.c_str());auto restored=spatial::readModel(d);obs_data_release(d);verify(restored.presetInstances[0].seed==seed&&restored.presetInstances[0].definitionSnapshot==m.presetInstances[0].definitionSnapshot,"saved composition retains seed and resolved recipe");
 m.presetInstances.clear();m.motionBlocks.clear();m.geometries.clear();for(auto&l:m.layers)l.animation.clear();panel.refresh();std::swap(m.layers[0],m.layers[1]);m.layers[0].label="Renamed B";panel.refresh();verify(list->item(0)->data(Qt::UserRole)=="b"&&list->item(0)->checkState()==Qt::Checked,"reorder rename stable IDs");
 details->click();control<QComboBox>(panel,"spatial.preset-motion-part361")->setCurrentIndex(control<QComboBox>(panel,"spatial.preset-motion-part361")->findData("M28"));control<QComboBox>(panel,"spatial.preset-mask-part361")->setCurrentIndex(control<QComboBox>(panel,"spatial.preset-mask-part361")->findData("T09"));control<QComboBox>(panel,"spatial.preset-decor-part361")->setCurrentIndex(control<QComboBox>(panel,"spatial.preset-decor-part361")->findData("D19"));generate->click();verify(recipe(last)["motion"]=="M28"&&recipe(last)["mask"]=="T09"&&recipe(last)["decoration"]=="D19","manual controls use same compiler");const int priorPreviews=previews;details->click();pump();verify(previews==priorPreviews,"closing details doesn't regenerate");
 const auto beforeMotion=last;control<QPushButton>(panel,"spatial.preset-reroll-motion361")->click();verify(output(last,2)==output(beforeMotion,2),"only motion reroll preserves exact mask/decor layout and channels");verify(recipe(last)["motion_seed"]!=recipe(beforeMotion)["motion_seed"],"motion reroll uses independent new seed");
 const auto beforeDecor=last;control<QPushButton>(panel,"spatial.preset-reroll-decor361")->click();verify(output(last,1)==output(beforeDecor,1),"only decoration reroll preserves exact material/camera/mask output");verify(recipe(last)["decoration_seed"]!=recipe(beforeDecor)["decoration_seed"],"decoration reroll uses independent new seed");
 const auto beforePalette=last;control<QPushButton>(panel,"spatial.preset-reroll-colors361")->click();verify(output(last,0,true)==output(beforePalette,0,true),"only palette reroll preserves every non-color field and channel");
 control<QCheckBox>(panel,"spatial.preset-lock-motion361")->setChecked(true);control<QCheckBox>(panel,"spatial.preset-lock-decor361")->setChecked(true);pump();const auto beforeLocked=last;reroll->click();verify(recipe(last)["motion_seed"]==recipe(beforeLocked)["motion_seed"]&&recipe(last)["decoration_seed"]==recipe(beforeLocked)["decoration_seed"]&&recipe(last)["motion"]==recipe(beforeLocked)["motion"]&&recipe(last)["decoration"]==recipe(beforeLocked)["decoration"],"fixed motion and decoration retain exact effective streams");
 cancel->click();const int beforeColor=previews;control<QPushButton>(panel,"spatial.preset-color-a36")->click();pump(5);dialog=panel.findChild<QColorDialog*>();verify(dialog!=nullptr,"color dialog before generation");dialog->setCurrentColor(QColor("#cc3311"));dialog->reject();pump(5);verify(previews==beforeColor&&!cancel->isEnabled(),"color selection alone never starts random generation");
 generate->click();panel.setDocument("stage-b");verify(!cancel->isEnabled()&&!apply->isEnabled(),"document swap cancels temporary preview");
 m.camera.position={1000,350,-1500};m.camera.rotation={10,25,0};m.layers[0].plane.position={400,-200,300};m.layers[0].importWidth=640;m.layers[0].importHeight=480;panel.refresh();
 auto*depth=control<QComboBox>(panel,"spatial.preset-depth36");auto*mask=control<QComboBox>(panel,"spatial.preset-mask-part361");auto*decor=control<QComboBox>(panel,"spatial.preset-decor-part361");control<QComboBox>(panel,"spatial.preset-amount361")->setCurrentIndex(0);mask->setCurrentIndex(mask->findData("none"));decor->setCurrentIndex(decor->findData("none"));depth->setCurrentIndex(1);int n=previews;generate->click();verify(previews==n+1,"motion-only ignores unused between-layer references");cancel->click();depth->setCurrentIndex(2);n=previews;generate->click();verify(previews==n+1,"motion-only ignores unused anchor reference");cancel->click();
 mask->setCurrentIndex(mask->findData("T02"));control<QDoubleSpinBox>(panel,"spatial.preset-x36")->setValue(12);control<QDoubleSpinBox>(panel,"spatial.preset-y36")->setValue(-8);n=previews;generate->click();verify(previews==n+1&&!last.geometries.empty(),"local-only mask ignores unused world anchor reference");verify(std::all_of(last.geometries.begin(),last.geometries.end(),[](const auto&g){return g.localMask&&std::abs(g.plane.position.x-12)<.001&&std::abs(g.plane.position.y+8)<.001&&std::abs(g.plane.position.z)<.001;}),"translated rotated camera never leaks world offset into local masks");cancel->click();
 depth->setCurrentIndex(0);n=previews;generate->click();verify(previews==n+1&&std::all_of(last.geometries.begin(),last.geometries.end(),[](const auto&g){return std::abs(g.plane.position.x-12)<.001&&std::abs(g.plane.position.y+8)<.001;}),"front placement retains user local offset separately from camera placement");cancel->click();m.layers.clear();panel.refresh();verify(!generate->isEnabled(),"empty scene disables generate");
 std::puts("PASS: beginner workflow,8styles,stableIDs,locks,temporary reroll,seed,manual library,color live preview/cancel,atomic apply,persistence,document cancellation");return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
