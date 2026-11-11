from pathlib import Path
p=Path('src/dock_actions.cpp');s=p.read_text(encoding='utf-8-sig').replace('current.hasDefault=identity.hasDefault;', '''current.hasDefault=identity.hasDefault;
            // Crop, bounds and readiness anchors describe this input, not a reusable spatial layout.
            current.followSourceSize=identity.followSourceSize;current.importWidth=identity.importWidth;current.importHeight=identity.importHeight;current.importTransform=identity.importTransform;
            current.cropLeft=identity.cropLeft;current.cropRight=identity.cropRight;current.cropTop=identity.cropTop;current.cropBottom=identity.cropBottom;current.boundsWidth=identity.boundsWidth;current.boundsHeight=identity.boundsHeight;current.boundsType=identity.boundsType;''');p.write_text(s,encoding='utf-8')
p=Path('src/dock.cpp');s=p.read_text(encoding='utf-8-sig');old='if(source){auto*settingsData=obs_source_get_settings(source);model=readModel(settingsData);obs_data_release(settingsData);obs_source_release(source);}'
new='if(source){auto*settingsData=obs_source_get_settings(source);const bool upgrade=obs_data_get_int(settingsData,"schema_version")<2;model=readModel(settingsData);obs_data_release(settingsData);if(upgrade){auto*upgraded=modelData(model);stageApplySettings(source,upgraded);obs_data_release(upgraded);saveTimer->start();blog(LOG_INFO,"[SpatialFX] schema1 scene upgraded to schema2");}obs_source_release(source);}'
assert old in s;s=s.replace(old,new);p.write_text(s,encoding='utf-8')
p=Path('tests/obs_qa_driver.cpp');s=p.read_text(encoding='utf-8-sig').replace('scenario!="v2regress"','scenario!="v2regress"&&scenario!="v2migrate"');s=s.replace('    if(scenario=="v2persist"){','''    if(scenario=="v2migrate"){
        click("spatial.refresh");auto*stage=stageSource();auto*d=stage?obs_source_get_settings(stage):nullptr;auto*a=d?obs_data_get_array(d,"layers"):nullptr;auto*l=a?obs_data_array_item(a,0):nullptr;
        check(d&&obs_data_get_int(d,"schema_version")==2,"actual v1 scene saved as schema2 after installed upgrade");check(l&&obs_data_get_double(l,"z")>0,"actual v1 all-flat scene receives depth after upgrade");check(d&&obs_data_get_double(d,"animation_duration")==3,"v1 three-second animation survives installed upgrade");
        obs_data_release(l);obs_data_array_release(a);obs_data_release(d);obs_source_release(stage);QTimer::singleShot(4000,qApp,finish);return;
    }
    if(scenario=="v2persist"){''');p.write_text(s,encoding='utf-8')
