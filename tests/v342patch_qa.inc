void v342patchExercise(){
 obs_frontend_add_scene_collection(("Spatial FX342 patch "+QString::number(QDateTime::currentMSecsSinceEpoch())).toUtf8().constData());
 click("spatial.refresh");click("spatial.create-demo");auto*p=panel();p->findChild<QTabWidget*>("spatial.tabs")->setCurrentIndex(2);fx34wait();
 const auto layers=v32state().value("layers").toArray();if(layers.isEmpty()){check(false,"patch fixture created");finish();return;}
 fx34instance=layers.first().toObject().value("instance_id").toString();fx34add();
 for(int g:{5,3,4}){auto*b=p->findChild<QToolButton*>(fx34name("group"+QString::number(g)));if(b)b->setChecked(true);fx34wait();}
 for(int g:{5,3,4}){auto*b=p->findChild<QCheckBox*>(fx34name(g==5?"FX05.0.enabled":g==3?"FX03.0.enabled":"FX04.0.enabled"));check(b&&b->isVisible(),"all enabled groups remain visible "+QString::number(g));}
 auto*curves=p->findChild<QCheckBox*>(fx34name("FX03.4.enabled"));if(curves)curves->setChecked(true);fx34wait();
 for(const auto*key:{"input_black","input_white","output_black","output_white","gamma"}){
  auto*n=p->findChild<QDoubleSpinBox*>(fx34name(QString("FX03.4.")+key));check(n&&n->isVisible()&&n->singleStep()==.01,"five levels slider visible and precise "+QString(key));
  if(n)n->setValue(QString(key)=="gamma"?1.35:QString(key).endsWith("white")?.85:.12);
 }
 auto*back=p->findChild<QCheckBox*>(fx34name("FX05.1.enabled"));if(back)back->setChecked(true);fx34wait();
 for(const auto*key:{"light_x","light_y","light_z"}){auto*n=p->findChild<QDoubleSpinBox*>(fx34name(QString("FX05.1.")+key));check(n&&n->isVisible(),"backlight XYZ visible "+QString(key));if(n)n->setValue(QString(key)=="light_z"?300:-75);}
 auto*five=p->findChild<QCheckBox*>(fx34name("FX05.0.enabled"));auto*three=p->findChild<QCheckBox*>(fx34name("FX03.0.enabled"));auto*four=p->findChild<QCheckBox*>(fx34name("FX04.0.enabled"));
 if(five&&three&&four){const auto y=[](QWidget*w){return w->mapToGlobal(QPoint(0,0)).y();};check(y(five)<y(three)&&y(three)<y(four),"groups follow activation order 5 3 4");}
 auto*g3=p->findChild<QToolButton*>(fx34name("group3"));if(g3){g3->setChecked(false);fx34wait();check(five&&five->isVisible()&&four&&four->isVisible()&&three&&!three->isVisible(),"disabling one group preserves all others");g3->setChecked(true);fx34wait();if(four&&three)check(four->mapToGlobal(QPoint()).y()<three->mapToGlobal(QPoint()).y(),"reactivated group appended last");}
 fx34wait();check(p->grab().save(output+"/fx-multiple-groups.png"),"real multiple FX group screenshot");obs_frontend_save();QTimer::singleShot(800,qApp,finish);
}
void v342patchPersist(){
 auto*p=panel();p->findChild<QTabWidget*>("spatial.tabs")->setCurrentIndex(2);fx34wait();auto layers=v32state().value("layers").toArray();check(!layers.isEmpty(),"patch scene restored");if(layers.isEmpty()){finish();return;}
 fx34instance=layers.first().toObject().value("instance_id").toString();
 const auto l=quality341effect("FX03.4").value("parameters").toObject(),b=quality341effect("FX05.1").value("parameters").toObject();
 check(std::abs(l.value("input_black").toDouble()-.12)<1e-9&&std::abs(l.value("input_white").toDouble()-.85)<1e-9&&std::abs(l.value("output_black").toDouble()-.12)<1e-9&&std::abs(l.value("output_white").toDouble()-.85)<1e-9&&std::abs(l.value("gamma").toDouble()-1.35)<1e-9,"five levels values survive real restart");
 check(b.value("light_x").toDouble()==-75&&b.value("light_y").toDouble()==-75&&b.value("light_z").toDouble()==300,"backlight XYZ survives restart");
 auto*a=p->findChild<QCheckBox*>(fx34name("FX05.0.enabled"));auto*c=p->findChild<QCheckBox*>(fx34name("FX04.0.enabled"));auto*d=p->findChild<QCheckBox*>(fx34name("FX03.0.enabled"));
 check(a&&c&&d&&a->isVisible()&&c->isVisible()&&d->isVisible(),"all enabled groups restored visible");if(a&&c&&d)check(a->mapToGlobal(QPoint()).y()<c->mapToGlobal(QPoint()).y()&&c->mapToGlobal(QPoint()).y()<d->mapToGlobal(QPoint()).y(),"activation order survives restart");
 fx34checkGpuFailures();QTimer::singleShot(600,qApp,finish);
}
