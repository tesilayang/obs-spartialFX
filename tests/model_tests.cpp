#include "model.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace spatial;
void expect(bool condition,const char *label) { if(!condition){std::cerr<<"FAIL: "<<label<<'\n';std::exit(1);} }
int main() {
    auto*copies=obs_data_create_from_json(R"({"layers":[{"uuid":"shared","scene_uuid":"scene","item_id":5,"instance_id":"a","instance_number":1,"x":10},{"uuid":"shared","scene_uuid":"scene","item_id":5,"instance_id":"b","instance_number":2,"x":90}],"light_bind":"shared","light_bind_scene":"scene","light_bind_item":5,"light_bind_instance":"b","receiver_uuid":"shared","receiver_scene":"scene","receiver_item":5,"receiver_instance":"a"})");
    auto copyModel=readModel(copies);auto*copyData=modelData(copyModel);auto*copyArray=obs_data_get_array(copyData,"layers");auto*copyB=obs_data_array_item(copyArray,1);
    expect(std::string(obs_data_get_string(copyB,"instance_id"))=="b"&&obs_data_get_int(copyB,"instance_number")==2,"same OBS item instances preserve unique identity and display number");
    expect(!lightMatchesLayer(copyModel.light,copyModel.layers[0])&&lightMatchesLayer(copyModel.light,copyModel.layers[1]),"light resolves exact stage instance rather than first shared item");
    expect(lightMatchesLayer(copyModel.light,copyModel.layers[0],true)&&!lightMatchesLayer(copyModel.light,copyModel.layers[1],true),"receiver resolves exact stage instance");
    expect(copyModel.layers[0].plane.position.x==10&&copyModel.layers[1].plane.position.x==90,"shared input preserves independent instance transforms");
    obs_data_release(copyB);obs_data_array_release(copyArray);obs_data_release(copyData);obs_data_release(copies);
    auto *defaults=modelData(StageModel{});
    expect(obs_data_has_user_value(defaults,"fx_enabled")&&!obs_data_get_bool(defaults,"fx_enabled"),"v3 persists explicit FX OFF default");obs_data_release(defaults);
    StageModel motion;motion.fx.enabled=true;motion.timelinePosition=1.25;motion.timelinePreview=true;motion.light.visible=true;motion.light.rimWidth=12.34;motion.light.rimSoftness=.73;Layer animated;animated.sourceUuid="moving";animated.animation["y"].enabled=true;animated.animation["y"].put({toTime(2),34});motion.layers.push_back(animated);auto*data3=modelData(motion);auto round3=readModel(data3);expect(round3.fx.enabled&&round3.timelinePreview&&round3.timelinePosition==1.25,"v3 master and preview persist");expect(round3.layers[0].animation["y"].keys().size()==1&&round3.layers[0].animation["y"].keys()[0].value==34,"per layer motion persists");expect(round3.light.visible&&round3.light.rimWidth==12.34,"v3 visible emitter and soft rim persist");obs_data_set_int(data3,"schema_version",2);auto legacy3=readModel(data3);expect(!legacy3.fx.enabled&&!legacy3.light.visible,"v2 upgrade forces FX and new emitter entity OFF");obs_data_release(data3);
    StageModel precise;precise.light.bindUuid="same-source";precise.light.bindSceneUuid="scene";precise.light.bindItemId=99;precise.light.receiverUuid="same-source";precise.light.receiverSceneUuid="scene";precise.light.receiverItemId=101;auto*identity=modelData(precise);auto preciseOut=readModel(identity);expect(preciseOut.light.bindItemId==99&&preciseOut.light.receiverItemId==101&&preciseOut.light.bindSceneUuid=="scene","duplicate-source light identity persists");Layer boundLayer;boundLayer.sourceUuid="same-source";boundLayer.sceneUuid="scene";boundLayer.itemId=99;expect(lightMatchesLayer(preciseOut.light,boundLayer),"exact duplicate binding resolves chosen item");boundLayer.itemId=101;expect(!lightMatchesLayer(preciseOut.light,boundLayer)&&lightMatchesLayer(preciseOut.light,boundLayer,true),"same UUID different item cannot steal bound light or receiver");obs_data_release(identity);
    StageModel input;
    input.camera.position={-25,42,-1780};input.camera.rotation={10,40,13};input.camera.fov=55;
    input.light.enabled=true;input.light.intensity=2.4;input.fx.exposure=1;
    Layer layer;layer.sourceUuid="stable-source-uuid";layer.sceneUuid="stable-scene-uuid";layer.itemId=48;layer.label="中文图层";layer.plane.position={10,20,300};layer.plane.rotation={12,22,32};layer.opacity=.65;input.layers.push_back(layer);
    input.cameraAnimation["x"].enabled=true;input.cameraAnimation["x"].put({toTime(3),input.camera.position.x});input.autoKey=true;input.keyPositionsLocked=true;
    obs_data_t *data=modelData(input);
    const char *json=obs_data_get_json(data);
    obs_data_t *reloaded=obs_data_create_from_json(json);
    auto output=readModel(reloaded);
    expect(output.layers.size()==1&&output.layers[0].sourceUuid==layer.sourceUuid,"UUID survives JSON roundtrip");
    expect(output.layers[0].sceneUuid==layer.sceneUuid&&output.layers[0].itemId==48,"parent scene and scene item identity survive");
    expect(output.layers[0].label==layer.label,"UTF8 Chinese label survives");
    expect(output.layers[0].plane.rotation.y==22&&output.layers[0].opacity==.65,"plane properties roundtrip");
    expect(output.cameraAnimation["x"].keys().size()==1&&output.cameraAnimation["x"].keys()[0].time==toTime(3),"keyframes roundtrip");
    expect(output.camera.position.z==-1780&&output.camera.fov==55,"camera roundtrip");
    expect(output.light.enabled&&output.light.intensity==2.4&&output.fx.exposure==1,"lighting and FX roundtrip");
    obs_data_release(reloaded);obs_data_release(data);
    data=obs_data_create_from_json("{\"width\":-10,\"height\":900000,\"camera_fov\":0,\"fx_saturation\":-10,\"layers\":[{\"uuid\":\"\"}]}");
    output=readModel(data);
    expect(output.width>=16&&output.width<=8192&&output.height<=8192,"hostile dimensions bounded");
    expect(output.camera.fov>=10&&output.fx.saturation>=0,"invalid lens and FX bounded");
    expect(output.layers.empty(),"empty UUID rejected");obs_data_release(data);
    data=obs_data_create();output=readModel(data);
    expect(output.width==1920&&output.height==1080&&output.camera.position.z==-1500,"missing settings retain defaults");obs_data_release(data);
        data=obs_data_create_from_json(R"({"schema_version":2,"animation_speed":2,"animation_duration":8,"focus_distance":1200,"aperture":4,"preview":false,"camera_target_x":30,"camera_look_at":true,"layers":[{"uuid":"v2","enabled":false,"receive_light":false,"cast_shadow":false,"receive_shadow":false,"rim_light":true,"perspective":0.5,"depth_order":4,"crop_left":20}],"light_type":1,"light_size":90,"light_falloff":1.3,"light_softness":14,"light_visible":false,"light_bind":"bound","receiver_uuid":"v2","extra_lights":[{"light_enabled":true,"light_type":1,"light_intensity":3}]})");
    output=readModel(data);auto *round=modelData(output);auto*array=obs_data_get_array(round,"layers");auto*item=obs_data_array_item(array,0);
    expect(!obs_data_get_bool(item,"enabled")&&!obs_data_get_bool(item,"receive_light")&&obs_data_get_bool(item,"rim_light"),"per-layer lighting flags survive roundtrip");
    expect(obs_data_get_double(item,"perspective")==.5&&obs_data_get_int(item,"crop_left")==20,"perspective and capture crop persist");
    expect(obs_data_get_double(round,"animation_speed")==2&&obs_data_get_double(round,"animation_duration")==0,"motion speed persists; no-key content length is zero");
    expect(obs_data_get_double(round,"light_size")==90&&obs_data_get_double(round,"light_falloff")==1.3,"area light geometry persists");
    auto*lights=obs_data_get_array(round,"extra_lights");expect(obs_data_array_count(lights)==1,"additional lights persist");
    expect(obs_data_get_bool(round,"camera_look_at")&&obs_data_get_double(round,"camera_target_x")==30,"camera target persists");
    obs_data_array_release(lights);obs_data_release(item);obs_data_array_release(array);obs_data_release(round);obs_data_release(data);
    data=obs_data_create_from_json(R"({"schema_version":3,"animation_duration":258.72,"legacy_playback_end":258.72,"use_legacy_playback_range":true,"keyframes":[{"time":0},{"time":258.72}],"layers":[{"uuid":"old","motion":[{"time":20}]}]})");
    output=readModel(data);expect(output.cameraAnimation.empty()&&output.layers[0].animation.empty()&&playbackEnd(output)==0,"old grouped animation and playback range are not supported");
    writeModel(output,data);expect(!obs_data_has_user_value(data,"legacy_playback_end")&&!obs_data_has_user_value(data,"use_legacy_playback_range")&&!obs_data_has_user_value(data,"keyframes"),"write removes obsolete grouped/range fields");obs_data_release(data);
    expect(!StageModel{}.autoKey&&!StageModel{}.keyPositionsLocked,"new scenes default explicit manual keys and unlocked key times");
    auto *flags=modelData(input);auto flagRound=readModel(flags);obs_data_release(flags);expect(flagRound.autoKey&&flagRound.keyPositionsLocked,"auto-key and key-time lock persist in schema4");
    std::cout<<"Model persistence checks passed\n";
}


