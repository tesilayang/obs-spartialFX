void v2exercise(){
    auto*p=panel();click("spatial.create-demo");
    const char*fields[]={"spatial.scene-choice","spatial.scene-sources","spatial.layer-name","spatial.layer-enabled","spatial.receive-light","spatial.cast-shadow","spatial.receive-shadow","spatial.rim-light","spatial.depth-order","spatial.perspective","spatial.focus-blur","spatial.reset-layer","spatial.target-x","spatial.target-y","spatial.target-z","spatial.look-at","spatial.preview","spatial.motion-speed","spatial.motion-duration","spatial.focus-distance","spatial.aperture","spatial.motion-shot","spatial.light-choice","spatial.light-visible","spatial.light-size","spatial.light-falloff","spatial.light-softness","spatial.light-rim","spatial.light-bind","spatial.receiver-choice"};
    for(auto*field:fields)check(p->findChild<QWidget*>(field)!=nullptr,QString("v2 control available: %1").arg(field));
    auto*list=p->findChild<QListWidget*>("spatial.layers");if(list)list->setCurrentRow(0);
    struct Numeric{const char*control;const char*key;double value;};
    const Numeric layerNumeric[]={{"spatial.layer-x","x",123},{"spatial.layer-y","y",45},{"spatial.layer-z","z",220},{"spatial.layer-rx","rx",5},{"spatial.layer-ry","ry",6},{"spatial.layer-rz","rz",7},{"spatial.layer-sx","sx",-.8},{"spatial.layer-sy","sy",1.2},{"spatial.layer-opacity","opacity",.8},{"spatial.depth-order","depth_order",3},{"spatial.perspective","perspective",.8},{"spatial.focus-blur","focus_blur",1.5}};
    for(const auto&n:layerNumeric)setNumber(n.control,n.value);
    auto*parameterStage=stageSource();auto*state=currentSettings(parameterStage);auto*parameterLayers=state?obs_data_get_array(state,"layers"):nullptr;auto*parameterLayer=parameterLayers?obs_data_array_item(parameterLayers,0):nullptr;
    for(const auto&n:layerNumeric)check(parameterLayer&&std::abs(obs_data_get_double(parameterLayer,n.key)-n.value)<.001,QString("native layer parameter roundtrip: %1").arg(n.key));
    obs_data_release(parameterLayer);obs_data_array_release(parameterLayers);obs_data_release(state);
    const Numeric cameraNumeric[]={{"spatial.camera-x","camera_x",130},{"spatial.camera-y","camera_y",25},{"spatial.camera-z","camera_z",-1600},{"spatial.camera-rx","camera_rx",3},{"spatial.camera-ry","camera_ry",4},{"spatial.camera-rz","camera_rz",5},{"spatial.camera-fov","camera_fov",45},{"spatial.target-x","camera_target_x",33},{"spatial.target-y","camera_target_y",44},{"spatial.target-z","camera_target_z",55},{"spatial.motion-speed","animation_speed",1.25},{"spatial.motion-duration","animation_duration",5},{"spatial.focus-distance","focus_distance",1200},{"spatial.aperture","aperture",4}};
    for(const auto&n:cameraNumeric)setNumber(n.control,n.value);state=currentSettings(parameterStage);for(const auto&n:cameraNumeric)check(state&&std::abs(obs_data_get_double(state,n.key)-n.value)<.001,QString("native camera parameter roundtrip: %1").arg(n.key));obs_data_release(state);click("spatial.reset-camera");
    const char*flagControls[]={"spatial.layer-visible","spatial.layer-enabled","spatial.receive-light","spatial.cast-shadow","spatial.receive-shadow","spatial.rim-light"};const char*flagKeys[]={"visible","enabled","receive_light","cast_shadow","receive_shadow","rim_light"};
    for(int i=0;i<6;++i){auto*flag=p->findChild<QCheckBox*>(flagControls[i]);if(flag)flag->setChecked(false);state=currentSettings(parameterStage);parameterLayers=state?obs_data_get_array(state,"layers"):nullptr;parameterLayer=parameterLayers?obs_data_array_item(parameterLayers,0):nullptr;check(parameterLayer&&!obs_data_get_bool(parameterLayer,flagKeys[i]),QString("native layer switch roundtrip: %1").arg(flagKeys[i]));obs_data_release(parameterLayer);obs_data_array_release(parameterLayers);obs_data_release(state);if(flag)flag->setChecked(true);}
    auto*layerTitle=p->findChild<QLineEdit*>("spatial.layer-name");if(layerTitle){layerTitle->setText("Spatial V2 Renamed Layer");QMetaObject::invokeMethod(layerTitle,"editingFinished",Qt::DirectConnection);}state=currentSettings(parameterStage);parameterLayers=state?obs_data_get_array(state,"layers"):nullptr;parameterLayer=parameterLayers?obs_data_array_item(parameterLayers,0):nullptr;check(parameterLayer&&QString(obs_data_get_string(parameterLayer,"label"))=="Spatial V2 Renamed Layer","native material rename updates OBS source and model");obs_data_release(parameterLayer);obs_data_array_release(parameterLayers);obs_data_release(state);obs_source_release(parameterStage);

    setNumber("spatial.layer-x",777);click("spatial.reset-layer");auto*stage=stageSource();auto*d=stage?currentSettings(stage):nullptr;auto*layers=d?obs_data_get_array(d,"layers"):nullptr;auto*first=layers?obs_data_array_item(layers,0):nullptr;
    check(first&&obs_data_get_double(first,"x")!=777,"reset restores selected layer transform");obs_data_release(first);obs_data_array_release(layers);obs_data_release(d);obs_source_release(stage);
    auto*flag=p->findChild<QCheckBox*>("spatial.cast-shadow");if(flag)flag->setChecked(false);
    setNumber("spatial.perspective",.75);setNumber("spatial.target-x",35);setNumber("spatial.motion-speed",1.5);setNumber("spatial.motion-duration",4);
    click("spatial.move-1");click("spatial.move-2");click("spatial.move-4");click("spatial.move-6");
    click("spatial.add-area-light");auto*lights=p->findChild<QComboBox*>("spatial.light-choice");check(lights&&lights->count()==2,"area emitter added to native list");
    const Numeric lightNumeric[]={{"spatial.light-x","light_x",-300},{"spatial.light-y","light_y",-120},{"spatial.light-z","light_z",-600},{"spatial.light-intensity","light_intensity",2.1},{"spatial.light-range","light_range",2800},{"spatial.light-ambient","light_ambient",.3},{"spatial.shadow-opacity","shadow_opacity",.6},{"spatial.light-size","light_size",150},{"spatial.light-falloff","light_falloff",1.5},{"spatial.light-softness","light_softness",20},{"spatial.light-rim","light_rim",2}};
    for(const auto&n:lightNumeric)setNumber(n.control,n.value);parameterStage=stageSource();state=currentSettings(parameterStage);auto*extra=state?obs_data_get_array(state,"extra_lights"):nullptr;auto*emitter=extra?obs_data_array_item(extra,0):nullptr;for(const auto&n:lightNumeric)check(emitter&&std::abs(obs_data_get_double(emitter,n.key)-n.value)<.001,QString("native area emitter parameter roundtrip: %1").arg(n.key));obs_data_release(emitter);obs_data_array_release(extra);obs_data_release(state);obs_source_release(parameterStage);
    for(const char*key:{"spatial.light-visible","spatial.light-enabled","spatial.shadows"}){auto*lightFlag=p->findChild<QCheckBox*>(key);if(lightFlag){lightFlag->setChecked(false);lightFlag->setChecked(true);}}
    QTimer::singleShot(300,qApp,[]{for(auto*w:QApplication::topLevelWidgets())if(auto*dialog=qobject_cast<QColorDialog*>(w)){dialog->setCurrentColor(QColor(255,170,85));QMetaObject::invokeMethod(dialog,"accept",Qt::DirectConnection);}});click("spatial.light-color");
    parameterStage=stageSource();state=currentSettings(parameterStage);extra=state?obs_data_get_array(state,"extra_lights"):nullptr;emitter=extra?obs_data_array_item(extra,0):nullptr;check(emitter&&std::abs(obs_data_get_double(emitter,"light_green")-170.0/255)<.01,"native color dialog commits selected area emitter color");obs_data_release(emitter);obs_data_array_release(extra);obs_data_release(state);obs_source_release(parameterStage);
    setNumber("spatial.light-size",120);setNumber("spatial.light-softness",22);setNumber("spatial.light-intensity",1.1);
    auto*bind=p->findChild<QComboBox*>("spatial.light-bind");if(bind&&bind->count()>1)bind->setCurrentIndex(1);
    auto*receiver=p->findChild<QComboBox*>("spatial.receiver-choice");if(receiver&&receiver->count()>1)receiver->setCurrentIndex(1);
    click("spatial.motion-shot");QTimer::singleShot(2000,qApp,[]{click("spatial.stop");});
    auto*sceneSource=obs_frontend_get_current_scene();auto*scene=sceneSource?obs_scene_from_source(sceneSource):nullptr;
    auto*settings=obs_data_create();obs_data_set_string(settings,"window","Spatial FX Capture Fixture:SpatialFXCaptureFixture:spatial-capture-fixture.exe");obs_data_set_int(settings,"method",1);obs_data_set_bool(settings,"cursor",false);
    auto*window=obs_source_create("window_capture","Spatial FX Window Capture QA",settings,nullptr);obs_data_release(settings);
    auto*item=scene&&window?obs_scene_add(scene,window):nullptr;
    if(scene)obs_scene_enum_items(scene,[](obs_scene_t*,obs_sceneitem_t*i,void*){obs_sceneitem_select(i,false);return true;},nullptr);
    if(item){obs_sceneitem_select(item,true);obs_transform_info tr{};obs_sceneitem_get_info2(item,&tr);tr.bounds_type=OBS_BOUNDS_SCALE_INNER;tr.bounds.x=800;tr.bounds.y=600;obs_sceneitem_set_info2(item,&tr);}
    const auto widthBefore=window?obs_source_get_width(window):0;check(window!=nullptr,"actual OBS window_capture source created");
    click("spatial.import-selected");stage=stageSource();d=stage?currentSettings(stage):nullptr;layers=d?obs_data_get_array(d,"layers"):nullptr;
    bool imported=false;for(size_t i=0;layers&&i<obs_data_array_count(layers);++i){auto*l=obs_data_array_item(layers,i);if(window&&QString(obs_data_get_string(l,"uuid"))==obs_source_get_uuid(window))imported=true;obs_data_release(l);}
    check(imported,"window capture with bounds and pending dimensions imports");check(item&&!obs_sceneitem_visible(item),"window capture original hidden after import");
    obs_data_array_release(layers);obs_data_release(d);obs_source_release(stage);
    if(window){auto*weak=obs_source_get_weak_source(window);QTimer::singleShot(7000,qApp,[weak,widthBefore]{auto*s=obs_weak_source_get_source(weak);check(s&&obs_source_get_width(s)>0&&obs_source_get_height(s)>0,QString("window capture becomes ready after import (before %1)").arg(widthBefore));obs_source_release(s);obs_weak_source_release(weak);});}
    obs_source_release(window);obs_source_release(sceneSource);
    QTimer::singleShot(11000,qApp,finish);
}

void v2regression(){
    click("spatial.create-demo");auto*p=panel();auto*binding=p->findChild<QComboBox*>("spatial.light-bind");auto*receiver=p->findChild<QComboBox*>("spatial.receiver-choice");
    if(binding)binding->setCurrentIndex(1);if(receiver)receiver->setCurrentIndex(3);
    auto*name=p->findChild<QLineEdit*>("spatial.preset-name");if(name)name->setText("QA V2 Rebind");click("spatial.save-preset");click("spatial.create-demo");
    auto*presets=p->findChild<QComboBox*>("spatial.preset-choice");if(presets)presets->setCurrentIndex(presets->findText("QA V2 Rebind"));click("spatial.apply-preset");
    auto*stage=stageSource();auto*d=stage?currentSettings(stage):nullptr;auto*arr=d?obs_data_get_array(d,"layers"):nullptr;auto*first=arr?obs_data_array_item(arr,0):nullptr;auto*last=arr?obs_data_array_item(arr,2):nullptr;
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
