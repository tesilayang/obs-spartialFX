from pathlib import Path
p=Path('tests/obs_qa_driver.cpp');s=p.read_text(encoding='utf-8')
insert=r'''
void lifecycle(){
    auto*original=obs_frontend_get_current_scene();auto*originalStage=stageSource();
    auto*seed=originalStage?obs_source_get_settings(originalStage):nullptr;auto*arr=seed?obs_data_get_array(seed,"layers"):nullptr;auto*first=arr?obs_data_array_item(arr,0):nullptr;auto*image=first?obs_get_source_by_uuid(obs_data_get_string(first,"uuid")):nullptr;
    auto*imageSettings=image?obs_source_get_settings(image):nullptr;auto*test=obs_scene_create("Spatial FX Lifecycle QA");
    if(imageSettings&&test){
        auto*source=obs_source_create("image_source","Spatial lifecycle original",imageSettings,nullptr);auto*item=source?obs_scene_add(test,source):nullptr;
        obs_frontend_set_current_scene(obs_scene_get_source(test));click("spatial.refresh");click("spatial.create-stage");
        if(item)obs_sceneitem_select(item,true);click("spatial.import-selected");
        auto*stage=stageSource();auto*d=stage?obs_source_get_settings(stage):nullptr;auto*layers=d?obs_data_get_array(d,"layers"):nullptr;
        check(layers&&obs_data_array_count(layers)==1,"selected source imports into actual stage");check(item&&!obs_sceneitem_visible(item),"import hides original item");
        if(source)obs_source_set_name(source,"Spatial lifecycle renamed");click("spatial.refresh");auto*list=panel()->findChild<QListWidget*>("spatial.layers");check(list&&list->count()==1&&list->item(0)->text()=="Spatial lifecycle renamed","UUID association survives source rename");
        click("spatial.restore-all");check(item&&obs_sceneitem_visible(item),"restore button restores original visibility");
        if(item)obs_sceneitem_select(item,true);click("spatial.import-selected");if(source)obs_source_remove(source);click("spatial.refresh");
        check(list&&list->count()==1&&list->item(0)->text().contains(QString::fromUtf8("源已删除")),"deleted source marked safely in dock");click("spatial.restore-all");
        obs_data_array_release(layers);obs_data_release(d);obs_source_release(stage);obs_source_release(source);
    }else check(false,"lifecycle fixture available");
    if(original){obs_frontend_set_current_scene(original);click("spatial.refresh");}check(stageSource()!=nullptr,"scene switching returns to persisted stage");
    auto*checkStage=stageSource();obs_source_release(checkStage);obs_source_release(original);obs_source_release(originalStage);obs_source_release(image);obs_data_release(imageSettings);obs_data_release(first);obs_data_array_release(arr);obs_data_release(seed);if(test)obs_scene_release(test);
    QTimer::singleShot(2000,qApp,finish);
}
'''
# Avoid acquiring an unbalanced reference in the final assertion.
insert=insert.replace('check(stageSource()!=nullptr,"scene switching returns to persisted stage");','')
insert=insert.replace('auto*checkStage=stageSource();','auto*checkStage=stageSource();check(checkStage!=nullptr,"scene switching returns to persisted stage");')
s=s.replace('void exercise(){',insert+'\nvoid exercise(){')
s=s.replace('if(scenario=="persist"||scenario=="hold")','if(scenario=="lifecycle"){lifecycle();return;}\n    if(scenario=="persist"||scenario=="hold")')
p.write_text(s,encoding='utf-8')
