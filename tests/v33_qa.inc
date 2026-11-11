void v33drag(QWidget*w,QPointF a,QPointF b){
 QMouseEvent down(QEvent::MouseButtonPress,a,a,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(w,&down);
 for(int i=1;i<=8;++i){auto at=a+(b-a)*(i/8.0);QMouseEvent move(QEvent::MouseMove,at,at,Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(w,&move);}
 QMouseEvent up(QEvent::MouseButtonRelease,b,b,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(w,&up);
}
void v33exercise(){
 auto*p=panel();auto*t=p->findChild<QTabWidget*>("spatial.tabs");
 check(t&&t->count()==4&&t->tabText(0)==QString::fromUtf8("镜头与动画"),"v3.3 single merged editor and four tabs");
 check(!p->findChild<QWidget*>("spatial.base-mode32"),"base layout toggle removed");
 check(!p->findChild<QListWidget*>("spatial.layers"),"no duplicate managed layer list");
 auto*imports=p->findChild<QToolButton*>("spatial.import-toggle33");check(imports,"import controls live in merged editor");
 auto*previewPanel=p->findChild<QWidget*>("spatial.structure-panel33");check(previewPanel,"collapsible detachable structure preview under timeline");
 if(!previewPanel||!imports){QTimer::singleShot(1000,qApp,finish);return;}
 // Keep stress/dense fixtures in their historical collection; this run uses a small isolated collection.
 obs_frontend_add_scene_collection(("Spatial V33 QA "+QString::number(QDateTime::currentMSecsSinceEpoch())).toUtf8().constData());click("spatial.refresh");
 click("spatial.create-demo");t->setCurrentIndex(0);click("spatial.restore-all");setNumber("spatial.playhead32",1);click("spatial.import-all");
 auto*objects=p->findChild<QComboBox*>("spatial.editor-object");auto*inspector=p->findChild<QTabWidget*>("spatial.inspector-tabs32");inspector->setCurrentIndex(0);
 auto initial=v32state();auto layers=initial["layers"].toArray();check(layers.size()==3,"import current OBS sources directly into timeline");
 check(initial["timeline_position"].toDouble()==0&&!initial["playing"].toBool(),"import pauses and seeks first frame");
 check(objects->currentData().toInt()==layers.size()-1,"import selects newest layer in unified editor");
 auto selected=layers.last().toObject();bool coordinates=true;for(const char*id:{"x","y","z","rx","ry","rz","sx","sy","opacity"}){auto*f=p->findChild<QDoubleSpinBox*>("spatial.param32."+QString(id));coordinates&=f&&std::abs(f->value()-selected[id].toDouble())<.011;}check(coordinates,"import coordinates map directly to first-frame inspector");
 imports->setChecked(false);check(!p->findChild<QListWidget*>("spatial.scene-sources")->isVisible(),"source import area collapses");imports->setChecked(true);check(p->findChild<QListWidget*>("spatial.scene-sources")->isVisible(),"source import area expands");
 objects->setCurrentIndex(objects->findData(0));auto*autoKey=p->findChild<QPushButton*>("spatial.auto-key32");autoKey->setChecked(true);setNumber("spatial.param32.x",123.45);
 check(std::abs(v32layer().value("x").toDouble()-123.45)<.001&&!v32track(v32layer(),"x")["enabled"].toBool()&&v32track(v32layer(),"x")["keys"].toArray().isEmpty(),"first-frame unanimated edit changes initial layout without artificial key");
 click("spatial.diamond32.x");setNumber("spatial.playhead32",2);setNumber("spatial.param32.x",323.45);autoKey->setChecked(false);setNumber("spatial.playhead32",0);setNumber("spatial.param32.x",73.45);
 auto keys=v32track(v32layer(),"x")["keys"].toArray();check(keys.size()==2&&std::abs(keys.first().toObject()["value"].toDouble()-73.45)<.001&&std::abs(keys.last().toObject()["value"].toDouble()-323.45)<.001,"first frame edits animated0 key without touching later motion");
 setNumber("spatial.playhead32",1);auto before=v32track(v32layer(),"x");setNumber("spatial.param32.x",999);check(v32track(v32layer(),"x")==before,"auto-key OFF protects unkeyed intermediate time");
 check(std::abs(p->findChild<QDoubleSpinBox*>("spatial.param32.x")->value()-198.45)<.011,"inspector evaluates shared interpolated coordinates");
 auto fixture=v32state();layers=fixture["layers"].toArray();auto l=layers[0].toObject();auto tr=v32track(l,"x");keys=tr["keys"].toArray();keys.removeFirst();tr["keys"]=keys;l["animation"]=QJsonArray{tr};layers[0]=l;fixture["layers"]=layers;fixture["timeline_position"]=0;fixture["timeline_position_us"]="0";v32fixPublish(fixture);objects->setCurrentIndex(objects->findData(0));setNumber("spatial.param32.x",63.45);
 keys=v32track(v32layer(),"x")["keys"].toArray();check(keys.size()==2&&keys.first().toObject()["time_us"].toString()=="0"&&std::abs(keys.first().toObject()["value"].toDouble()-63.45)<.001,"animated imported legacy curve without0 key accepts first-frame edit");
 auto*staticProps=p->findChild<QWidget*>("spatial.layer-properties33");check(staticProps&&staticProps->isVisible(),"layer static properties are inside unified inspector");objects->setCurrentIndex(objects->findData(-1));check(!staticProps->isVisible(),"camera selection hides layer-only properties");objects->setCurrentIndex(objects->findData(0));
 auto*bind=p->findChild<QComboBox*>("spatial.light-bind");bind->setCurrentIndex(1);auto binding=v32state().value("light_bind");auto oldLayers=v32state().value("layers").toArray();auto*timeline=p->findChild<QWidget*>("spatial.timeline32");
 v33drag(timeline,{70,99},{70,153});layers=v32state().value("layers").toArray();check(layers.last().toObject()["uuid"]==oldLayers.first().toObject()["uuid"],"mouse dragging timeline layer name reorders whole layer");
 check(layers.last().toObject()["animation"]==oldLayers.first().toObject()["animation"]&&layers.last().toObject()["item_id"]==oldLayers.first().toObject()["item_id"],"timeline reorder preserves motion and exact item identity");
 check(objects->currentData().toInt()==2&&v32state().value("light_bind")==binding,"selection and light binding follow dragged layer");
 click("spatial.undo32");check(v32layer().value("uuid")==oldLayers.first().toObject()["uuid"],"undo restores layer order by identity");click("spatial.redo32");check(v32layer(2).value("uuid")==oldLayers.first().toObject()["uuid"],"redo restores dragged layer order");
 click("spatial.structure-expand33");auto*preview=p->findChild<QWidget*>("spatial.structure");check(!preview->isVisible(),"structure preview collapses without losing widget");click("spatial.structure-expand33");check(preview->isVisible(),"structure preview expands");
 for(int i=0;i<3;++i){click("spatial.structure-detach33");QApplication::processEvents();auto*window=p->findChild<QDialog*>("spatial.structure-float33");check(window&&window->isVisible()&&window->isAncestorOf(preview),QString("same structure widget detaches round%1").arg(i));if(window){if(i==0)window->grab().save(output+"/structure-floating.png");window->close();QApplication::processEvents();}check(previewPanel->isAncestorOf(preview)&&preview->isVisible(),QString("closing floating window reattaches visible preview round%1").arg(i));QApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);}
 p->grab().save(output+"/merged-editor.png");
 auto resetBefore=v32state().value("layers");v32confirm(false);click("spatial.reset-current32");check(v32state().value("layers")==resetBefore,"cancel reset current preserves full layer state");v32confirm(true);click("spatial.reset-current32");check(v32layer(2).value("animation").toArray().isEmpty(),"confirmed current reset clears motion");click("spatial.undo32");check(v32state().value("layers")==resetBefore,"undo current reset restores imported layer motion");
 v32confirm(false);click("spatial.reset-all32");check(v32state().value("layers")==resetBefore,"cancel reset all preserves full state");
 auto*help=p->findChild<QObject*>("spatial.hover-help");check(help&&help->property("delayMs").toInt()==1000,"hover help retains one-second delay");
 auto*rotation=p->findChild<QSlider*>("spatial.param32.rx.slider");check(rotation&&rotation->minimum()==-36000&&rotation->maximum()==36000,"merged rotation sliders retain full360degree mapping");
 // A transform undo must not roll back a later static depth-order edit.
 setNumber("spatial.playhead32",0);setNumber("spatial.param32.y",83.21);setNumber("spatial.depth-order",20);click("spatial.undo32");check(v32layer(2).value("depth_order").toInt()==20,"motion undo preserves subsequent static depth-order edit");
 // Legacy non-preview documents still accept time-zero editing through this same inspector.
 auto legacy=v32state();legacy["timeline_preview"]=false;legacy["timeline_position"]=0;legacy["timeline_position_us"]="0";v32fixPublish(legacy);objects->setCurrentIndex(objects->findData(2));setNumber("spatial.param32.x",63.45);
 check(v32state().value("timeline_preview").toBool()&&std::abs(v32track(v32layer(2),"x")["keys"].toArray().first().toObject()["value"].toDouble()-63.45)<.001,"legacy base-mode document transitions to unified first-frame edit");
 imports->setChecked(false);if(preview->isVisible())click("spatial.structure-expand33");mainWidget()->resize(1460,920);QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);p->grab().save(output+"/compact-editor.png");
 auto final=v32state();QFile expected(output+"/expected-state.json");if(expected.open(QIODevice::WriteOnly))expected.write(QJsonDocument(final).toJson());obs_frontend_save();
 click("spatial.play32");check(v32state().value("playing").toBool(),"play action starts actual stage clock");
 auto*playTimer=new QTimer(qApp);playTimer->setInterval(150);auto*attempts=new int(0);QObject::connect(playTimer,&QTimer::timeout,qApp,[playTimer,attempts]{double at=panel()->findChild<QDoubleSpinBox*>("spatial.playhead32")->value();if(at<=0&&++*attempts<30)return;playTimer->stop();delete attempts;click("spatial.pause32");check(v32state().value("timeline_position").toDouble()>0,"real preview playback clock advances");click("spatial.start32");QTimer::singleShot(800,qApp,finish);playTimer->deleteLater();});playTimer->start();
}
void v33persist(){
 click("spatial.refresh");auto*p=panel();auto state=v32state();
 QFile expected(qEnvironmentVariable("SPATIAL_QA_EXPECTED"));check(expected.open(QIODevice::ReadOnly),"saved expected scene fixture available");auto before=QJsonDocument::fromJson(expected.readAll()).object();
 check(state["layers"]==before["layers"],"save and restart preserves layer order identities layout and keys");
 check(state["animation"]==before["animation"]&&state["light_bind"]==before["light_bind"],"save and restart preserves camera motion and light association");
 check(state["schema_version"].toInt()==4,"v3.3 remains schema4 compatible");
 auto*objects=p->findChild<QComboBox*>("spatial.editor-object");objects->setCurrentIndex(objects->findData(2));setNumber("spatial.playhead32",1);check(std::abs(p->findChild<QDoubleSpinBox*>("spatial.param32.x")->value()-193.45)<.011,"reloaded animation still evaluates same first-frame to endpoint motion");click("spatial.start32");QTimer::singleShot(1000,qApp,finish);
}
void v33edge(){
 click("spatial.refresh");auto*p=panel();auto*objects=p->findChild<QComboBox*>("spatial.editor-object");int moving=-1;auto seed=v32state().value("layers").toArray();for(int i=0;i<seed.size();++i)if(!v32track(seed[i].toObject(),"x").value("keys").toArray().isEmpty()){moving=i;break;}check(moving>=0,"clipboard fixture has an actual animated layer");if(moving<0){finish();return;}int destination=moving==0?2:0;objects->setCurrentIndex(objects->findData(moving));auto*timeline=p->findChild<QWidget*>("spatial.timeline32");
 auto original=v32state();auto id=v32layer(moving).value("uuid");double rowY=32+(2+moving)*27+13;double destY=32+(2+destination)*27+13;setNumber("spatial.playhead32",0);
 // Click the summary diamond, copy, reorder the layer, then paste into that same identity.
 v33drag(timeline,{180,rowY},{180,rowY});QKeyEvent copy(QEvent::KeyPress,Qt::Key_C,Qt::ControlModifier);QApplication::sendEvent(timeline,&copy);
 v33drag(timeline,{70,rowY},{70,destY});check(v32layer(destination).value("uuid")==id,"clipboard case drags selected layer to first row");setNumber("spatial.playhead32",3);QKeyEvent paste(QEvent::KeyPress,Qt::Key_V,Qt::ControlModifier);QApplication::sendEvent(timeline,&paste);
 auto keys=v32track(v32layer(destination),"x")["keys"].toArray();bool pasted=false;for(auto k:keys)if(k.toObject()["time_us"].toString()=="3000000")pasted=true;check(pasted&&v32layer(destination).value("uuid")==id,"copy then reorder then paste targets original layer identity");
 click("spatial.undo32");click("spatial.undo32");check(v32state().value("layers")==original["layers"],"undo paste and reorder restores exact layer state");
 objects->setCurrentIndex(objects->findData(moving));auto before=v32state();click("spatial.remove-layer");auto removed=v32state().value("layers");click("spatial.undo32");check(v32state().value("layers")==removed&&removed.toArray().size()==2,"remove invalidates old index undo history");
 auto*source=obs_get_source_by_uuid(id.toString().toUtf8().constData());auto*sceneSource=obs_frontend_get_current_scene();auto*scene=sceneSource?obs_scene_from_source(sceneSource):nullptr;auto itemId=before["layers"].toArray()[moving].toObject()["item_id"].toInteger();auto*item=scene?obs_scene_find_sceneitem_by_id(scene,itemId):nullptr;check(source&&item&&obs_sceneitem_visible(item),"remove restores original OBS source visibility");if(item)obs_sceneitem_select(item,true);click("spatial.import-selected");check(v32state().value("layers").toArray().size()==3&&v32layer(2).value("uuid")==id,"reimport restores stable source association directly to timeline");obs_source_release(source);obs_source_release(sceneSource);
 v32fixPublish(original);objects->setCurrentIndex(objects->findData(moving));
 QTimer::singleShot(1000,qApp,finish);
}
void v33shutdown(){
 click("spatial.refresh");click("spatial.structure-detach33");auto*window=panel()->findChild<QDialog*>("spatial.structure-float33");check(window&&window->isVisible(),"floating preview open when OBS shutdown begins");QTimer::singleShot(800,qApp,finish);
}
