QJsonObject quality341effect(const QString&id){for(const auto&v:fx34stack().value("effects").toArray())if(v.toObject().value("id")==id)return v.toObject();return {};}
void v341exercise(){
 obs_frontend_add_scene_collection(("Spatial V341 QA "+QString::number(QDateTime::currentMSecsSinceEpoch())).toUtf8().constData());click("spatial.refresh");click("spatial.create-demo");
 auto*p=panel();auto*tabs=p->findChild<QTabWidget*>("spatial.tabs");tabs->setCurrentIndex(2);fx34wait();auto layers=v32state().value("layers").toArray();check(layers.size()==3,"v341 imported objects available");if(layers.isEmpty()){finish();return;}fx34instance=layers.first().toObject().value("instance_id").toString();fx34add();
 auto*category=p->findChild<QToolButton*>(fx34name("group2"));check(category&&!category->isChecked(),"quality correction defaults paused");if(category)category->click();
 auto*aa=p->findChild<QToolButton*>(fx34name("quality.aa.fold"));auto*clarity=p->findChild<QToolButton*>(fx34name("quality.clarity.fold"));
 auto*smaa=p->findChild<QCheckBox*>(fx34name("FX02.2.enabled"));auto*fxaa=p->findChild<QCheckBox*>(fx34name("FX02.3.enabled"));auto*rcas=p->findChild<QCheckBox*>(fx34name("FX02.4.enabled"));
 check(aa&&clarity&&smaa&&fxaa&&rcas,"independent AA and clarity sections construct");if(!aa||!clarity||!smaa||!fxaa||!rcas){finish();return;}
 check(!aa->isChecked()&&!clarity->isChecked()&&!smaa->isVisible()&&!rcas->isVisible(),"new sections initially folded");
 check(!smaa->isChecked()&&!fxaa->isChecked()&&!rcas->isChecked(),"all new effects initially off");
 check(!p->findChild<QDoubleSpinBox*>(fx34name("FX02.2.threshold"))&&!p->findChild<QDoubleSpinBox*>(fx34name("FX02.4.amount")),"new effect controls built lazily");
 check(smaa->parentWidget()!=p->findChild<QWidget*>(fx34name("FX02.0.parameters"))&&rcas->parentWidget()!=p->findChild<QWidget*>(fx34name("FX02.0.parameters")),"new sections are siblings of legacy sharpening");
 aa->click();clarity->click();check(smaa->isVisible()&&rcas->isVisible()&&quality341effect("FX02.2").isEmpty(),"opening sections only exposes inactive options");
 smaa->setChecked(true);fx34wait();auto*threshold=p->findChild<QDoubleSpinBox*>(fx34name("FX02.2.threshold"));auto*quality=p->findChild<QComboBox*>(fx34name("FX02.2.quality"));
 check(threshold&&quality&&threshold->isVisible()&&threshold->singleStep()==.01,"SMAA has visible .01 slider and quality control");if(threshold)threshold->setValue(.12);if(quality)quality->setCurrentIndex(1);
 fxaa->setChecked(true);fx34wait();check(fxaa->isChecked()&&!smaa->isChecked()&&threshold&&!threshold->isVisible(),"selecting FXAA deactivates and folds SMAA");
 check(!quality341effect("FX02.2").value("enabled").toBool()&&quality341effect("FX02.3").value("enabled").toBool(),"AA exclusivity reaches authoritative model");
 auto*subpixel=p->findChild<QDoubleSpinBox*>(fx34name("FX02.3.subpixel"));if(subpixel)subpixel->setValue(.37);check(subpixel&&subpixel->singleStep()==.01,"FXAA numeric controls keep .01 precision");
 smaa->setChecked(true);fx34wait();check(!fxaa->isChecked()&&smaa->isChecked(),"switching back deactivates FXAA");
 check(threshold&&threshold->value()==.12&&quality&&quality->currentIndex()==1,"switching preserves prior SMAA parameter values");
 rcas->setChecked(true);fx34wait();auto*amount=p->findChild<QDoubleSpinBox*>(fx34name("FX02.4.amount"));auto*noise=p->findChild<QComboBox*>(fx34name("FX02.4.denoise"));
 check(amount&&noise&&amount->singleStep()==.01&&amount->isVisible(),"RCAS has independent compact controls");if(amount)amount->setValue(.42);if(noise)noise->setCurrentIndex(0);
 aa->click();check(!smaa->isVisible()&&quality341effect("FX02.2").value("enabled").toBool(),"section folding preserves chosen processing state");aa->click();
 check(!aa->toolTip().isEmpty()&&!clarity->toolTip().isEmpty()&&!smaa->toolTip().isEmpty()&&!fxaa->toolTip().isEmpty(),"new sections and algorithms explain their cost and behavior on hover");
 bool nativeHidden=true;for(auto*c:p->findChildren<QCheckBox*>())nativeHidden&=!c->text().contains("Native AA")&&!c->text().contains("NAA");check(nativeHidden,"deferred Native AA has no visible placeholder");
 auto*version=p->findChild<QLabel*>("spatial.version");check(version&&version->text().contains("3.4.1"),"dock identifies v3.4.1");
 if(auto*scroll=p->findChild<QScrollArea*>("spatial.fx-scroll34"))scroll->ensureWidgetVisible(aa);fx34wait();check(p->grab().save(output+"/quality-controls.png"),"actual v341 quality controls screenshot");
 check(!fx34stack(1).value("attached").toBool(),"quality effects stay independent per layer instance");fx34checkGpuFailures();obs_frontend_save();QTimer::singleShot(800,qApp,finish);
}
void v341persist(){
 auto layers=v32state().value("layers").toArray();check(!layers.isEmpty(),"v341 scene loads after restart");if(layers.isEmpty()){finish();return;}fx34instance=layers.first().toObject().value("instance_id").toString();
 auto smaa=quality341effect("FX02.2"),fxaa=quality341effect("FX02.3"),rcas=quality341effect("FX02.4");
 check(smaa.value("enabled").toBool()&&!fxaa.value("enabled").toBool()&&rcas.value("enabled").toBool(),"exclusive AA choice and RCAS survive restart");
 check(smaa.value("parameters").toObject().value("threshold").toDouble()==.12&&smaa.value("parameters").toObject().value("quality").toInt()==1,"SMAA precise parameters survive restart");
 check(fxaa.value("parameters").toObject().value("subpixel").toDouble()==.37,"inactive FXAA settings survive restart");
 check(rcas.value("parameters").toObject().value("amount").toDouble()==.42&&rcas.value("parameters").toObject().value("denoise").toInt()==0,"RCAS precise parameters survive restart");
 auto*p=panel();p->findChild<QTabWidget*>("spatial.tabs")->setCurrentIndex(2);fx34wait();auto*aa=p->findChild<QToolButton*>(fx34name("quality.aa.fold"));auto*clarity=p->findChild<QToolButton*>(fx34name("quality.clarity.fold"));
 check(aa&&clarity&&aa->isChecked()&&clarity->isChecked(),"sections with active effects reopen after restart");fx34checkGpuFailures();QTimer::singleShot(500,qApp,finish);
}
