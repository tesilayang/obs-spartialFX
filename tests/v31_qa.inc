// Regression against the real dock, OBS model and Qt slider signals.
void v31exercise(){
    auto*p=panel();click("spatial.create-demo");
    auto*list=p->findChild<QListWidget*>("spatial.layers");
    auto*stage=stageSource();if(list)list->setCurrentRow(0);
    const char*names[]={"spatial.layer-x","spatial.layer-y","spatial.layer-z"};
    const char*jsonNames[]={"x","y","z"};
    for(int axis=0;axis<3;++axis){
        auto*f=p->findChild<QDoubleSpinBox*>(names[axis]);auto*s=p->findChild<QSlider*>(QString(names[axis])+".slider");
        check(f&&s,QString("v3.1 XYZ controls exist: %1").arg(names[axis]));if(!f||!s)continue;
        const double saved=f->value();
        auto verify=[&](double expected,const QString&label){
            auto*d=currentSettings(stage);auto*a=obs_data_get_array(d,"layers");auto*l=obs_data_array_item(a,0);
            check(std::abs(f->value()-expected)<.001&&std::abs(obs_data_get_double(l,jsonNames[axis])-expected)<.001,label);
            obs_data_release(l);obs_data_array_release(a);obs_data_release(d);
        };
        check(s->minimum()==-200000&&s->maximum()==200000,"v3.1 fixed XYZ slider range -2000 to +2000: "+QString(names[axis]));
        for(int v:{-200000,-123456,0,123456,200000}){
            s->setValue(v);verify(v/100.0,QString("v3.1 absolute slider maps %1 to model %2: %3").arg(v).arg(v/100.0).arg(names[axis]));
            check(s->minimum()==-200000&&s->maximum()==200000,"v3.1 slider range never recenters after value change");
        }
        s->setValue(0);s->triggerAction(QAbstractSlider::SliderSingleStepAdd);verify(.01,"v3.1 slider keyboard step +0.01");
        s->triggerAction(QAbstractSlider::SliderSingleStepSub);verify(0,"v3.1 slider keyboard step -0.01");
        s->setSliderDown(true);s->setSliderPosition(77777);s->setSliderDown(false);verify(777.77,"v3.1 release preserves absolute drag value");
        check(s->value()==77777,"v3.1 release does not jump thumb back to center");
        // Send real mouse drag events to the native QSlider, including release.
        s->setValue(0);QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        auto dragEnd=[&](int end){const QPointF from(s->width()/2.0,s->height()/2.0),to(end,s->height()/2.0);
            QMouseEvent down(QEvent::MouseButtonPress,from,from,s->mapToGlobal(from.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(s,&down);
            QMouseEvent move(QEvent::MouseMove,to,to,s->mapToGlobal(to.toPoint()),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);QApplication::sendEvent(s,&move);
            QMouseEvent up(QEvent::MouseButtonRelease,to,to,s->mapToGlobal(to.toPoint()),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(s,&up);
        };
        dragEnd(s->width()+20);verify(2000,"v3.1 actual Qt mouse drag reaches +2000 and holds after release");
        s->setValue(0);dragEnd(-20);verify(-2000,"v3.1 actual Qt mouse drag reaches -2000 and holds after release");
        for(double outside:{3456.78,-3456.78,100000.0,-100000.0}){
            auto*editor=f->findChild<QLineEdit*>();check(editor!=nullptr,"v3.1 numeric editor available");
            if(editor){editor->setText(QString::number(outside,'f',2));f->interpretText();}
            verify(outside,"v3.1 manual entry beyond slider range persists in real OBS model");
            check(s->value()==(outside>0?200000:-200000),"v3.1 out-of-range value pins thumb at corresponding endpoint");
            click("spatial.refresh");verify(outside,"v3.1 refresh preserves out-of-range value without clamping model");
            check(s->minimum()==-200000&&s->maximum()==200000,"v3.1 refresh retains fixed slider bounds");
        }
        f->setValue(1250.25);if(list&&list->count()>1){list->setCurrentRow(1);list->setCurrentRow(0);}
        verify(1250.25,"v3.1 layer switching restores full numeric value");
        check(s->value()==125025,"v3.1 layer switching restores absolute thumb position");
        f->setValue(saved);
    }
    const char*helpNames[]={"spatial.depth-order-help","spatial.perspective-help","spatial.focus-blur-help"};
    for(auto*name:helpNames){auto*h=p->findChild<QLabel*>(name);check(h&&h->wordWrap()&&!h->text().isEmpty(),QString("v3.1 permanent parameter explanation: %1").arg(name));}
    auto*title=p->findChild<QLabel*>("spatial.version");check(title&&title->text().contains("3.1"),"v3.1 dock displays correct version");
    auto*tabs=p->findChild<QTabWidget*>("spatial.tabs");if(tabs){tabs->setCurrentIndex(0);auto*scroll=qobject_cast<QScrollArea*>(tabs->currentWidget());if(scroll){scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);check(p->grab().save(output+"/layer-parameter-help.png"),"v3.1 real parameter explanation screenshot");scroll->verticalScrollBar()->setValue(0);}}
    obs_source_release(stage);QTimer::singleShot(1500,qApp,finish);
}
