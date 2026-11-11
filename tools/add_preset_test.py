from pathlib import Path
p=Path('tests/obs_qa_driver.cpp');s=p.read_text(encoding='utf-8')
anchor='if(scenario=="lifecycle"){lifecycle();return;}'
addition=r'''
    if(scenario=="presets"){
        click("spatial.refresh");auto*source=stageSource();auto*before=source?obs_source_get_settings(source):nullptr;
        const auto previous=before?QJsonDocument::fromJson(obs_data_get_json(before)).object().value("layers").toArray():QJsonArray{};
        auto*choice=p->findChild<QComboBox*>("spatial.preset-choice");const int index=choice?choice->findText("QA Persisted"):-1;
        check(index>=0,"named preset exists before apply");if(index>=0)choice->setCurrentIndex(index);
        setNumber("spatial.camera-x",999);click("spatial.apply-preset");
        auto*after=source?obs_source_get_settings(source):nullptr;check(after&&obs_data_get_double(after,"camera_x")==177,"named preset restores camera through actual button");
        const auto current=after?QJsonDocument::fromJson(obs_data_get_json(after)).object().value("layers").toArray():QJsonArray{};
        bool stable=previous.size()==current.size();for(qsizetype i=0;stable&&i<previous.size();++i)stable=previous[i].toObject().value("uuid")==current[i].toObject().value("uuid")&&previous[i].toObject().value("scene_uuid")==current[i].toObject().value("scene_uuid");
        check(stable,"preset apply preserves actual layer and scene UUIDs");obs_data_release(before);obs_data_release(after);obs_source_release(source);
        QTimer::singleShot(2500,qApp,finish);return;
    }
'''
assert anchor in s;s=s.replace(anchor,addition+'\n    '+anchor);p.write_text(s,encoding='utf-8')
