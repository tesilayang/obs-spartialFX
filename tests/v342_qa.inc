// Native Qt interaction: sliders, model, stage-master switching and restart.
void v342exercise(){
 obs_frontend_add_scene_collection(("Spatial V342 QA "+QString::number(QDateTime::currentMSecsSinceEpoch())).toUtf8().constData());
 click("spatial.refresh");click("spatial.create-demo");auto*p=panel();p->findChild<QTabWidget*>("spatial.tabs")->setCurrentIndex(2);fx34wait();
 const auto layers=v32state().value("layers").toArray();check(!layers.isEmpty(),"v342 timeline fixture exists");if(layers.isEmpty()){finish();return;}
 fx34instance=layers.first().toObject().value("instance_id").toString();fx34add();
 for(int g:{3,4,5}){auto*b=p->findChild<QToolButton*>(fx34name("group"+QString::number(g)));check(b!=nullptr,"FX category available");if(b)b->click();}
 for(const auto&id:{QString("FX03.0"),QString("FX04.0"),QString("FX05.2")}){auto*c=p->findChild<QCheckBox*>(fx34name(id+".enabled"));check(c!=nullptr,"v342 child available");if(c)c->setChecked(true);}
 struct Value{const char*key;int lo,hi,value;};
 for(const auto&v:{Value{"FX03.0.gamma",10,400,135},Value{"FX03.0.exposure",-500,500,-75},Value{"FX03.0.saturation",0,300,124},Value{"FX04.0.radius",0,16000,12034},Value{"FX04.0.threshold",0,100,42}}){
  const auto name=fx34name(v.key);auto*slider=p->findChild<QSlider*>(name+".slider");auto*number=p->findChild<QDoubleSpinBox*>(name);
  check(slider&&number&&slider->minimum()==v.lo&&slider->maximum()==v.hi,"slider uses full precise physical range "+QString(v.key));
  if(slider&&number){slider->setValue(v.value);check(std::abs(number->value()-v.value/100.)<1e-9,"slider and number agree "+QString(v.key));number->setValue(v.value/100.+.01);check(slider->value()==v.value+1,"manual .01 edit synchronizes slider "+QString(v.key));}
 }
 auto*mode=p->findChild<QComboBox*>(fx34name("FX04.0.mode"));check(mode&&mode->count()==2,"both glow blend modes available");if(mode)mode->setCurrentIndex(1);
 auto*master=p->findChild<QCheckBox*>("spatial.fx-enabled");check(master!=nullptr,"stage lighting master available");
 if(master)for(int i=0;i<6;++i){master->setChecked(i%2==0);fx34wait();check(quality341effect("FX05.2").value("enabled").toBool(),"stage toggle preserves enabled independent candle");}
 for(const auto&id:{QString("FX03.0"),QString("FX04.0")})check(quality341effect(id).value("enabled").toBool(),"stage toggle preserves grade and glow");
 if(auto*scroll=p->findChild<QScrollArea*>("spatial.fx-scroll34"))scroll->ensureWidgetVisible(mode);fx34wait();check(p->grab().save(output+"/v342-glow-controls.png"),"actual v342 FX control screenshot");
 fx34checkGpuFailures();obs_frontend_save();QTimer::singleShot(800,qApp,finish);
}
void v342persist(){
 const auto layers=v32state().value("layers").toArray();check(!layers.isEmpty(),"v342 scene restores after restart");if(layers.isEmpty()){finish();return;}
 fx34instance=layers.first().toObject().value("instance_id").toString();
 const auto grade=quality341effect("FX03.0"),glow=quality341effect("FX04.0"),candle=quality341effect("FX05.2");
 const auto gp=grade.value("parameters").toObject(),bp=glow.value("parameters").toObject();
 check(grade.value("enabled").toBool()&&glow.value("enabled").toBool()&&candle.value("enabled").toBool(),"grade glow and independent light enabled flags persist");
 check(std::abs(gp.value("gamma").toDouble()-1.36)<1e-9&&std::abs(gp.value("exposure").toDouble()+.74)<1e-9&&std::abs(gp.value("saturation").toDouble()-1.25)<1e-9,"precise grade slider values survive restart");
 check(std::abs(bp.value("radius").toDouble()-120.35)<1e-9&&std::abs(bp.value("threshold").toDouble()-.43)<1e-9&&bp.value("mode").toInt()==1,"glow radius threshold and mode survive restart");
 check(!v32state().value("fx_enabled").toBool(),"stage master remains off independently of active FX");
 fx34checkGpuFailures();QTimer::singleShot(600,qApp,finish);
}
