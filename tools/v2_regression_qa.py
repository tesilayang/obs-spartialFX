from pathlib import Path
p=Path('tests/v2_qa.inc');s=p.read_text(encoding='utf-8-sig');s+='''
void v2regression(){
    click("spatial.create-demo");auto*p=panel();auto*binding=p->findChild<QComboBox*>("spatial.light-bind");auto*receiver=p->findChild<QComboBox*>("spatial.receiver-choice");
    if(binding)binding->setCurrentIndex(1);if(receiver)receiver->setCurrentIndex(3);
    auto*name=p->findChild<QLineEdit*>("spatial.preset-name");if(name)name->setText("QA V2 Rebind");click("spatial.save-preset");click("spatial.create-demo");
    auto*presets=p->findChild<QComboBox*>("spatial.preset-choice");if(presets)presets->setCurrentIndex(presets->findText("QA V2 Rebind"));click("spatial.apply-preset");
    auto*stage=stageSource();auto*d=stage?obs_source_get_settings(stage):nullptr;auto*arr=d?obs_data_get_array(d,"layers"):nullptr;auto*first=arr?obs_data_array_item(arr,0):nullptr;auto*last=arr?obs_data_array_item(arr,2):nullptr;
    check(d&&first&&QString(obs_data_get_string(d,"light_bind"))==obs_data_get_string(first,"uuid"),"cross-scene preset remaps emitter binding to target layer");
    check(d&&last&&QString(obs_data_get_string(d,"receiver_uuid"))==obs_data_get_string(last,"uuid"),"cross-scene preset remaps shadow receiver to target layer");
    auto*image=first?obs_get_source_by_uuid(obs_data_get_string(first,"uuid")):nullptr;
    auto*a=obs_scene_create("Spatial V2 ID A"),*b=obs_scene_create("Spatial V2 ID B");
    auto*ia=image&&a?obs_scene_add(a,image):nullptr;auto*dummy=image&&b?obs_scene_add(b,image):nullptr;if(dummy)obs_sceneitem_remove(dummy);auto*ib=image&&b?obs_scene_add(b,image):nullptr;
    if(a&&b&&ia&&ib){obs_frontend_set_current_scene(obs_scene_get_source(a));click("spatial.refresh");obs_frontend_set_current_scene(obs_scene_get_source(b));click("spatial.refresh");auto*list=p->findChild<QListWidget*>("spatial.scene-sources");check(list&&list->count()==1&&list->item(0)->data(Qt::UserRole).toLongLong()==obs_sceneitem_get_id(ib),"same-name scene switch refreshes sceneitem ID");}
    obs_data_release(first);obs_data_release(last);obs_data_array_release(arr);obs_data_release(d);obs_source_release(stage);obs_source_release(image);obs_scene_release(a);obs_scene_release(b);
    auto*scene=obs_scene_create("Spatial V2 Pending Window");if(scene)obs_frontend_set_current_scene(obs_scene_get_source(scene));click("spatial.refresh");click("spatial.create-stage");
    auto*settings=obs_data_create();obs_data_set_string(settings,"window","Spatial FX Capture Fixture:SpatialFXCaptureFixture:spatial-capture-fixture.exe");obs_data_set_int(settings,"method",1);obs_data_set_bool(settings,"cursor",false);
    auto*window=obs_source_create("window_capture","Spatial V2 No Bounds Window",settings,nullptr);obs_data_release(settings);auto*item=scene&&window?obs_scene_add(scene,window):nullptr;
    if(scene)obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t*i,void*){obs_sceneitem_select(i,false);return true;},nullptr);
    if(item){obs_sceneitem_select(item,true);obs_transform_info t{};obs_sceneitem_get_info2(item,&t);t.pos.x=120;t.pos.y=80;t.scale.x=.6f;t.scale.y=.7f;t.alignment=OBS_ALIGN_LEFT|OBS_ALIGN_TOP;obs_sceneitem_set_info2(item,&t);}
    check(window&&obs_source_get_width(window)==0,"regression fixture starts with zero capture dimensions");click("spatial.import-selected");
    if(window){auto*weak=obs_source_get_weak_source(window);QTimer::singleShot(7000,qApp,[weak]{auto*s=obs_weak_source_get_source(weak);check(s&&obs_source_get_width(s)>0,"no-bounds pending window becomes ready");obs_source_release(s);obs_weak_source_release(weak);});}
    obs_source_release(window);obs_scene_release(scene);QTimer::singleShot(30000,qApp,finish);
}
''';p.write_text(s,encoding='utf-8')
p=Path('tests/obs_qa_driver.cpp');s=p.read_text(encoding='utf-8-sig').replace('scenario!="v2hold"','scenario!="v2hold"&&scenario!="v2regress"');s=s.replace('    if(scenario=="v2"){', '    if(scenario=="v2regress"){v2regression();return;}\n    if(scenario=="v2"){');p.write_text(s,encoding='utf-8')
p=Path('src/dock_actions.cpp');s=p.read_text(encoding='utf-8-sig').replace('const QJsonObject&data){auto field=', 'const QJsonObject&presetData){auto field=').replace('if(data.contains(slot))','if(presetData.contains(slot))').replace('data.value(slot).toInt(-1)','presetData.value(slot).toInt(-1)');p.write_text(s,encoding='utf-8')
p=Path('installer/setup.cpp');s=p.read_text(encoding='utf-8-sig').replace('std::vector<RegistryValue> before;try{before=readRegistry(key);regSet', 'std::vector<RegistryValue> before;try{before=readRegistry(key);}catch(...){RegCloseKey(key);throw;}try{regSet');p.write_text(s,encoding='utf-8')
