// QA-only native controls; never linked into the distributed plugin.
QJsonObject v361Ui(const QJsonObject& command,bool& ok){
    QJsonObject info;auto*p=panel();if(!p){ok=false;return info;}
    const auto action=command["action"].toString();const auto name=command["widget"].toString();
    auto*w=p->findChild<QWidget*>(name);
    if(action=="ui-click"){
        auto*b=qobject_cast<QAbstractButton*>(w);ok&=b&&(b->isEnabled()||command["ifEnabled"].toBool());
        if(b&&b->isEnabled()){
            const auto confirm=command["confirm"].toString();
            QTimer::singleShot(0,b,[b,confirm]{
                if(!confirm.isEmpty())QTimer::singleShot(80,qApp,[confirm]{for(auto*d:QApplication::topLevelWidgets())if(auto*m=qobject_cast<QMessageBox*>(d))if(m->isVisible()){const auto answer=confirm=="yes"?QMessageBox::Yes:confirm=="ok"?QMessageBox::Ok:QMessageBox::Cancel;if(auto*button=m->button(answer))button->click();}});
                b->click();
            });
        }
    }else if(action=="ui-check"){auto*b=qobject_cast<QAbstractButton*>(w);ok&=b&&b->isEnabled();if(b)b->setChecked(command["value"].toBool());
    }else if(action=="ui-combo"){auto*c=qobject_cast<QComboBox*>(w);const int n=c?(command.contains("data")?c->findData(command["data"].toVariant()):command["value"].toInt()):-1;ok&=c&&n>=0&&n<c->count();if(c&&n>=0)c->setCurrentIndex(n);
    }else if(action=="ui-number"){auto*c=qobject_cast<QDoubleSpinBox*>(w);auto*i=qobject_cast<QSpinBox*>(w);ok&=c||i;if(c)c->setValue(command["value"].toDouble());else if(i)i->setValue(command["value"].toInt());
    }else if(action=="ui-list"){
        auto*l=qobject_cast<QListWidget*>(w);ok&=l!=nullptr;
        if(l){const auto selected=command["ids"].toArray();for(int i=0;i<l->count();++i){auto*item=l->item(i);if(item->flags()&Qt::ItemIsUserCheckable)item->setCheckState(((item->flags()&Qt::ItemIsEnabled)&&(command["all"].toBool()||selected.contains(QJsonValue(item->data(Qt::UserRole).toString()))))?Qt::Checked:Qt::Unchecked);}if(command.contains("row"))l->setCurrentRow(command["row"].toInt());}
    }else if(action=="ui-tab"){auto*t=p->findChild<QTabWidget*>("spatial.tabs");ok&=t!=nullptr;if(t)t->setCurrentIndex(command["value"].toInt());
    }else if(action.startsWith("ui-color-")){
        QColorDialog*d=nullptr;for(auto*c:p->findChildren<QColorDialog*>())if(c->isVisible())d=c;ok&=d!=nullptr;
        if(d){if(action=="ui-color-set")d->setCurrentColor(QColor(command["color"].toString()));else if(action=="ui-color-accept")d->accept();else if(action=="ui-color-cancel")d->reject();}
    }
    if(action=="ui-save")obs_frontend_save();
    if(action=="ui-inspect"||command["inspect"].toBool()){
        QJsonArray controls;for(auto*c:p->findChildren<QWidget*>()){
            if(c->objectName().isEmpty())continue;
            const auto pos=c->mapTo(p,QPoint(0,0));
            QJsonObject r{{"name",c->objectName()},{"class",c->metaObject()->className()},{"visible",c->isVisible()},{"hidden",c->isHidden()},{"enabled",c->isEnabled()},{"x",pos.x()},{"y",pos.y()},{"width",c->width()},{"height",c->height()},{"tooltip",c->toolTip()}};
            if(auto*b=qobject_cast<QAbstractButton*>(c)){r["text"]=b->text();r["checked"]=b->isChecked();}
            if(auto*l=qobject_cast<QLabel*>(c))r["text"]=l->text();
            if(auto*n=qobject_cast<QDoubleSpinBox*>(c)){r["value"]=n->value();r["step"]=n->singleStep();}
            if(auto*n=qobject_cast<QSpinBox*>(c)){r["value"]=n->value();r["step"]=n->singleStep();}
            if(auto*n=qobject_cast<QLineEdit*>(c))r["text"]=n->text();
            if(auto*b=qobject_cast<QComboBox*>(c)){r["index"]=b->currentIndex();r["data"]=QJsonValue::fromVariant(b->currentData());QJsonArray items;for(int i=0;i<b->count();++i)items.append(QJsonObject{{"text",b->itemText(i)},{"data",QJsonValue::fromVariant(b->itemData(i))}});r["items"]=items;}
            if(auto*l=qobject_cast<QListWidget*>(c)){QJsonArray items;for(int i=0;i<l->count();++i){auto*v=l->item(i);items.append(QJsonObject{{"text",v->text()},{"id",v->data(Qt::UserRole).toString()},{"checked",v->checkState()==Qt::Checked},{"enabled",bool(v->flags()&Qt::ItemIsEnabled)}});}r["items"]=items;}
            controls.append(r);
        }info["controls"]=controls;
        if(command.contains("screenshot")){const auto file=QFileInfo(command["screenshot"].toString()).fileName();ok&=!file.isEmpty()&&p->grab().save(output+"/"+file);}
    }
    return info;
}
