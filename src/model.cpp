#include "model.hpp"
#include "scene_evaluator.hpp"
#include "presets/presets.hpp"
#include <algorithm>
#include <cmath>
namespace spatial {
namespace {
Animation readAnimation(obs_data_t*d){Animation a;auto*arr=obs_data_get_array(d,"animation");if(arr)for(size_t i=0;i<obs_data_array_count(arr);++i){auto*r=obs_data_array_item(arr,i);std::string id=obs_data_get_string(r,"parameter");if(parameter(id)){Curve c;c.enabled=obs_data_get_bool(r,"enabled");c.locked=obs_data_get_bool(r,"locked");c.shortest=obs_data_get_bool(r,"shortest");auto*keys=obs_data_get_array(r,"keys");if(keys)for(size_t j=0;j<obs_data_array_count(keys);++j){auto*k=obs_data_array_item(keys,j);AnimKey key;try{key.time=std::stoll(obs_data_get_string(k,"time_us"));}catch(...){key.time=0;}key.value=obs_data_get_double(k,"value");key.interpolation=Interpolation(std::clamp(int(obs_data_get_int(k,"mode")),0,7));key.legacyFrom=obs_data_has_user_value(k,"legacy_from")?obs_data_get_double(k,"legacy_from"):0;key.legacyTo=obs_data_has_user_value(k,"legacy_to")?obs_data_get_double(k,"legacy_to"):1;key.outAbsolute=obs_data_get_bool(k,"out_absolute");key.inAbsolute=obs_data_get_bool(k,"in_absolute");key.outX=obs_data_has_user_value(k,"out_x")?obs_data_get_double(k,"out_x"):1.0/3;key.outY=obs_data_has_user_value(k,"out_y")?obs_data_get_double(k,"out_y"):1.0/3;key.inX=obs_data_has_user_value(k,"in_x")?obs_data_get_double(k,"in_x"):2.0/3;key.inY=obs_data_has_user_value(k,"in_y")?obs_data_get_double(k,"in_y"):2.0/3;if(!std::isfinite(key.outX))key.outX=1.0/3;if(!std::isfinite(key.outY))key.outY=1.0/3;if(!std::isfinite(key.inX))key.inX=2.0/3;if(!std::isfinite(key.inY))key.inY=2.0/3;c.put(key);obs_data_release(k);}obs_data_array_release(keys);a[id]=std::move(c);}obs_data_release(r);}obs_data_array_release(arr);return a;}
void writeAnimation(obs_data_t*d,const Animation&a){auto*arr=obs_data_array_create();for(auto&pair:a){auto*r=obs_data_create();obs_data_set_string(r,"parameter",pair.first.c_str());auto&c=pair.second;obs_data_set_bool(r,"enabled",c.enabled);obs_data_set_bool(r,"locked",c.locked);obs_data_set_bool(r,"shortest",c.shortest);auto*keys=obs_data_array_create();for(auto&k:c.keys()){auto*v=obs_data_create();obs_data_set_string(v,"time_us",std::to_string(k.time).c_str());obs_data_set_double(v,"value",k.value);obs_data_set_int(v,"mode",int(k.interpolation));obs_data_set_double(v,"legacy_from",k.legacyFrom);obs_data_set_double(v,"legacy_to",k.legacyTo);obs_data_set_bool(v,"out_absolute",k.outAbsolute);obs_data_set_bool(v,"in_absolute",k.inAbsolute);obs_data_set_double(v,"out_x",k.outX);obs_data_set_double(v,"out_y",k.outY);obs_data_set_double(v,"in_x",k.inX);obs_data_set_double(v,"in_y",k.inY);obs_data_array_push_back(keys,v);obs_data_release(v);}obs_data_set_array(r,"keys",keys);obs_data_array_release(keys);obs_data_array_push_back(arr,r);obs_data_release(r);}obs_data_set_array(d,"animation",arr);obs_data_array_release(arr);}
Orbit readOrbit(obs_data_t*d){Orbit o;o.enabled=obs_data_get_bool(d,"orbit_enabled");o.x=obs_data_get_double(d,"orbit_x");o.y=obs_data_get_double(d,"orbit_y");o.z=obs_data_get_double(d,"orbit_z");o.radius=obs_data_has_user_value(d,"orbit_radius")?obs_data_get_double(d,"orbit_radius"):1500;o.yaw=obs_data_get_double(d,"orbit_yaw");o.pitch=obs_data_get_double(d,"orbit_pitch");for(auto*p:{&o.x,&o.y,&o.z,&o.radius,&o.yaw,&o.pitch})if(!std::isfinite(*p))*p=0;o.radius=std::clamp(o.radius,0.0,100000.0);return o;}
void writeOrbit(obs_data_t*d,const Orbit&o){obs_data_set_bool(d,"orbit_enabled",o.enabled);obs_data_set_double(d,"orbit_x",o.x);obs_data_set_double(d,"orbit_y",o.y);obs_data_set_double(d,"orbit_z",o.z);obs_data_set_double(d,"orbit_radius",o.radius);obs_data_set_double(d,"orbit_yaw",o.yaw);obs_data_set_double(d,"orbit_pitch",o.pitch);}
bool has(obs_data_t *d,const char *k){return d&&(obs_data_has_user_value(d,k)||obs_data_has_default_value(d,k));}
double number(obs_data_t *d,const char *k,double fallback,double low,double high){
    const double value=has(d,k)?obs_data_get_double(d,k):fallback;
    return std::isfinite(value)?std::clamp(value,low,high):fallback;
}
bool boolean(obs_data_t *d,const char *k,bool fallback){return has(d,k)?obs_data_get_bool(d,k):fallback;}
Vec3 readVec(obs_data_t *d,const std::string &prefix,Vec3 fallback){
    return {number(d,(prefix+"x").c_str(),fallback.x,-100000,100000),number(d,(prefix+"y").c_str(),fallback.y,-100000,100000),number(d,(prefix+"z").c_str(),fallback.z,-100000,100000)};
}
void writeVec(obs_data_t *d,const std::string &prefix,Vec3 p){
    obs_data_set_double(d,(prefix+"x").c_str(),p.x);obs_data_set_double(d,(prefix+"y").c_str(),p.y);obs_data_set_double(d,(prefix+"z").c_str(),p.z);
}
Camera readCamera(obs_data_t *d){Camera c;c.position=readVec(d,"camera_",c.position);c.rotation=readVec(d,"camera_r",c.rotation);c.fov=number(d,"camera_fov",c.fov,10,140);return c;}
void writeCamera(obs_data_t *d,const Camera &c){writeVec(d,"camera_",c.position);writeVec(d,"camera_r",c.rotation);obs_data_set_double(d,"camera_fov",c.fov);}
}
bool lightMatchesLayer(const Light&light,const Layer&layer,bool receiver){
    const auto&uuid=receiver?light.receiverUuid:light.bindUuid;const auto&scene=receiver?light.receiverSceneUuid:light.bindSceneUuid;const auto item=receiver?light.receiverItemId:light.bindItemId;
    const auto&instance=receiver?light.receiverInstanceId:light.bindInstanceId;
    return uuid==layer.sourceUuid&&(scene.empty()||scene==layer.sceneUuid)&&(item==0||item==layer.itemId)&&(instance.empty()?layer.instanceNumber==1:instance==layer.instanceId);
}
bool sameSourceItem(const Layer&a,const Layer&b){return a.sourceUuid==b.sourceUuid&&a.sceneUuid==b.sceneUuid&&a.itemId==b.itemId;}
bool sameLayerInstance(const Layer&a,const Layer&b){return sameSourceItem(a,b)&&a.instanceId==b.instanceId;}
std::string layerDisplayName(const Layer&l){return l.label+(l.instanceNumber>1?" · 实例 "+std::to_string(l.instanceNumber):"");}
StageModel readModel(obs_data_t *d){
    StageModel m;
    m.width=uint32_t(number(d,"width",m.width,16,8192));m.height=uint32_t(number(d,"height",m.height,16,8192));
    m.camera=readCamera(d);
    m.target=readVec(d,"camera_target_",{});m.lookAtTarget=boolean(d,"camera_look_at",false);m.preview=boolean(d,"preview",true);
    m.animationSpeed=number(d,"animation_speed",1,.05,10);m.animationDuration=number(d,"animation_duration",6,0,9e12);
    m.focusDistance=number(d,"focus_distance",1500,10,100000);m.aperture=number(d,"aperture",0,0,20);
    m.timelinePosition=number(d,"timeline_position",0,0,9e12);m.timelinePreview=boolean(d,"timeline_preview",false);
    m.playing=boolean(d,"playing",false);m.loop=boolean(d,"loop",false);
    m.playToken=uint64_t(number(d,"play_token",0,0,9007199254740991.0));
    m.light.enabled=boolean(d,"light_enabled",m.light.enabled);m.light.shadows=boolean(d,"shadows_enabled",m.light.shadows);
    m.light.position=readVec(d,"light_",m.light.position);
    m.light.color={number(d,"light_red",1,0,1),number(d,"light_green",.9,0,1),number(d,"light_blue",.8,0,1)};
    m.light.shadowColor={number(d,"shadow_red",0,0,1),number(d,"shadow_green",0,0,1),number(d,"shadow_blue",0,0,1)};
    m.light.intensity=number(d,"light_intensity",m.light.intensity,0,10);m.light.range=number(d,"light_range",m.light.range,10,100000);
    m.light.ambient=number(d,"light_ambient",m.light.ambient,0,2);m.light.shadowOpacity=number(d,"shadow_opacity",m.light.shadowOpacity,0,1);m.light.receiverZ=number(d,"receiver_z",m.light.receiverZ,-100000,100000);
    m.light.type=int(number(d,"light_type",0,0,1));m.light.size=number(d,"light_size",60,0,2000);m.light.falloff=number(d,"light_falloff",2,.1,8);m.light.softness=number(d,"light_softness",15,0,300);m.light.rim=number(d,"light_rim",1,0,5);
    m.light.rimWidth=number(d,"light_rim_width",8,.01,100);m.light.rimSoftness=number(d,"light_rim_softness",.65,0,1);
    m.light.visible=obs_data_get_int(d,"schema_version")>=3&&boolean(d,"light_visible",false);m.light.bindUuid=obs_data_get_string(d,"light_bind");m.light.receiverUuid=obs_data_get_string(d,"receiver_uuid");
    m.light.bindSceneUuid=obs_data_get_string(d,"light_bind_scene");m.light.receiverSceneUuid=obs_data_get_string(d,"receiver_scene");m.light.bindItemId=obs_data_get_int(d,"light_bind_item");m.light.receiverItemId=obs_data_get_int(d,"receiver_item");
    m.light.bindInstanceId=obs_data_get_string(d,"light_bind_instance");m.light.receiverInstanceId=obs_data_get_string(d,"receiver_instance");
    if(has(d,"light_label"))m.light.label=obs_data_get_string(d,"light_label");
    auto *extra=obs_data_get_array(d,"extra_lights");
    if(extra)for(size_t n=0;n<std::min(size_t(3),obs_data_array_count(extra));++n){auto*entry=obs_data_array_item(extra,n);
        // Additional emitter objects do not recurse through nested emitter arrays.
        auto*clean=obs_data_create();obs_data_apply(clean,entry);obs_data_erase(clean,"extra_lights");m.extraLights.push_back(readModel(clean).light);obs_data_release(clean);obs_data_release(entry);}
    obs_data_array_release(extra);
    m.fx.enabled=obs_data_get_int(d,"schema_version")>=3&&boolean(d,"fx_enabled",false);m.fx.exposure=number(d,"fx_exposure",0,-5,5);m.fx.saturation=number(d,"fx_saturation",1,0,3);m.fx.vignette=number(d,"fx_vignette",0,0,1);
    auto *layers=obs_data_get_array(d,"layers");
    if(layers)for(size_t i=0;i<std::min(size_t(32),obs_data_array_count(layers));++i){
        obs_data_t *item=obs_data_array_item(layers,i);Layer l;
        l.sourceUuid=obs_data_get_string(item,"uuid");l.sceneUuid=obs_data_get_string(item,"scene_uuid");l.label=obs_data_get_string(item,"label");l.itemId=obs_data_get_int(item,"item_id");l.fx=readFxStack(item);
        l.instanceId=obs_data_get_string(item,"instance_id");l.instanceNumber=int(number(item,"instance_number",1,1,1000000));
        // Pre-instance scenes and old demo fixtures need deterministic, independently saved identities.
        if(l.instanceId.empty()||std::any_of(m.layers.begin(),m.layers.end(),[&](const Layer&prior){return prior.instanceId==l.instanceId;})){
            l.instanceId="legacy-"+l.sourceUuid+"-"+l.sceneUuid+"-"+std::to_string(l.itemId)+"-"+std::to_string(i);
            while(std::any_of(m.layers.begin(),m.layers.end(),[&](const Layer&prior){return prior.instanceId==l.instanceId;}))l.instanceId+="_";
        }
        l.plane.position=readVec(item,"",{});l.plane.rotation=readVec(item,"r",{});
        l.plane.scaleX=number(item,"sx",1,-100,100);l.plane.scaleY=number(item,"sy",1,-100,100);
        if(std::abs(l.plane.scaleX)<.001)l.plane.scaleX=.001;if(std::abs(l.plane.scaleY)<.001)l.plane.scaleY=.001;
        l.enabled=boolean(item,"enabled",true);l.receiveLight=boolean(item,"receive_light",true);l.castShadow=boolean(item,"cast_shadow",true);l.receiveShadow=boolean(item,"receive_shadow",true);l.rimLight=boolean(item,"rim_light",true);
        l.perspective=number(item,"perspective",1,0,2);l.focusBlur=number(item,"focus_blur",1,0,4);l.depthOrder=int(number(item,"depth_order",0,-1000,1000));
        l.cropLeft=int(number(item,"crop_left",0,0,8192));l.cropRight=int(number(item,"crop_right",0,0,8192));l.cropTop=int(number(item,"crop_top",0,0,8192));l.cropBottom=int(number(item,"crop_bottom",0,0,8192));
        l.boundsWidth=number(item,"bounds_width",0,0,100000);l.boundsHeight=number(item,"bounds_height",0,0,100000);l.boundsType=int(number(item,"bounds_type",0,0,6));
        l.followSourceSize=boolean(item,"follow_source_size",false);l.importWidth=number(item,"import_width",0,0,100000);l.importHeight=number(item,"import_height",0,0,100000);
        auto&t=l.importTransform;t.scaleX=number(item,"source_sx",1,-100,100);t.scaleY=number(item,"source_sy",1,-100,100);t.alignment=int(number(item,"source_alignment",5,0,15));t.boundsAlignment=int(number(item,"bounds_alignment",0,0,15));t.cropToBounds=boolean(item,"crop_to_bounds",false);t.boundsWidth=l.boundsWidth;t.boundsHeight=l.boundsHeight;t.boundsType=l.boundsType;t.left=l.cropLeft;t.right=l.cropRight;t.top=l.cropTop;t.bottom=l.cropBottom;
        l.hasDefault=boolean(item,"has_default",false);l.defaultPlane.position=readVec(item,"default_",l.plane.position);l.defaultPlane.rotation=readVec(item,"default_r",l.plane.rotation);l.defaultPlane.scaleX=number(item,"default_sx",l.plane.scaleX,-100,100);l.defaultPlane.scaleY=number(item,"default_sy",l.plane.scaleY,-100,100);
        l.animation=readAnimation(item);l.orbit=readOrbit(item);l.locked=boolean(item,"animation_locked",false);l.defaultOpacity=number(item,"default_opacity",number(item,"opacity",1,0,1),0,1);l.opacity=number(item,"opacity",1,0,1);l.visible=boolean(item,"visible",true);l.originalVisible=boolean(item,"original_visible",true);
        if(!l.sourceUuid.empty())m.layers.push_back(std::move(l));obs_data_release(item);
    }
    obs_data_array_release(layers);
    m.mediaUuid=obs_data_get_string(d,"media_uuid");m.mediaLinked=boolean(d,"media_linked",false);m.mediaOffset=number(d,"media_offset",0,-100000,100000);m.cameraAnimation=readAnimation(d);m.cameraOrbit=readOrbit(d);m.cameraLocked=boolean(d,"animation_locked",false);
    auto*composite=obs_data_get_obj(d,"composite");m.compositeAnimation=readAnimation(composite);m.compositeLocked=boolean(composite,"animation_locked",false);m.outputScaleX=number(composite,"sx",1,-100,100);m.outputScaleY=number(composite,"sy",1,-100,100);m.outputOpacity=number(composite,"opacity",1,0,1);obs_data_release(composite);
    m.autoKey=boolean(d,"auto_key",false);m.keyPositionsLocked=boolean(d,"key_positions_locked",false);
    if(has(d,"timeline_position_us")){try{m.timelinePosition=toSeconds(std::max(int64_t(0),std::stoll(obs_data_get_string(d,"timeline_position_us"))));}catch(...){}}
    presets::readPresets(m,d);
    m.animationDuration=toSeconds(contentEnd(m));
    return m;
}
void writeModel(const StageModel &input,obs_data_t *d){
    const StageModel &m=input;
    presets::writePresets(m,d);
    for(const char *name:{"legacy_playback_end","use_legacy_playback_range","keyframes"})obs_data_erase(d,name);
    obs_data_set_bool(d,"auto_key",m.autoKey);obs_data_set_bool(d,"key_positions_locked",m.keyPositionsLocked);
    obs_data_set_string(d,"media_uuid",m.mediaUuid.c_str());obs_data_set_bool(d,"media_linked",m.mediaLinked);obs_data_set_double(d,"media_offset",m.mediaOffset);obs_data_set_int(d,"schema_version",5);obs_data_set_bool(d,"fx_enabled",m.fx.enabled);
    writeAnimation(d,m.cameraAnimation);writeOrbit(d,m.cameraOrbit);obs_data_set_bool(d,"animation_locked",m.cameraLocked);
    auto*composite=obs_data_create();writeAnimation(composite,m.compositeAnimation);obs_data_set_bool(composite,"animation_locked",m.compositeLocked);obs_data_set_double(composite,"sx",m.outputScaleX);obs_data_set_double(composite,"sy",m.outputScaleY);obs_data_set_double(composite,"opacity",m.outputOpacity);obs_data_set_obj(d,"composite",composite);obs_data_release(composite);
    obs_data_set_string(d,"timeline_position_us",std::to_string(toTime(m.timelinePosition)).c_str());obs_data_set_double(d,"timeline_position",m.timelinePosition);obs_data_set_bool(d,"timeline_preview",m.timelinePreview);
    obs_data_set_double(d,"light_rim_width",m.light.rimWidth);obs_data_set_double(d,"light_rim_softness",m.light.rimSoftness);
    writeVec(d,"camera_target_",m.target);obs_data_set_bool(d,"camera_look_at",m.lookAtTarget);obs_data_set_bool(d,"preview",m.preview);
    obs_data_set_double(d,"animation_speed",m.animationSpeed);obs_data_set_double(d,"animation_duration",m.animationDuration);obs_data_set_double(d,"focus_distance",m.focusDistance);obs_data_set_double(d,"aperture",m.aperture);
    obs_data_set_int(d,"light_type",m.light.type);obs_data_set_double(d,"light_size",m.light.size);obs_data_set_double(d,"light_falloff",m.light.falloff);obs_data_set_double(d,"light_softness",m.light.softness);obs_data_set_double(d,"light_rim",m.light.rim);
    obs_data_set_bool(d,"light_visible",m.light.visible);obs_data_set_string(d,"light_bind",m.light.bindUuid.c_str());obs_data_set_string(d,"receiver_uuid",m.light.receiverUuid.c_str());obs_data_set_string(d,"light_label",m.light.label.c_str());
    obs_data_set_string(d,"light_bind_scene",m.light.bindSceneUuid.c_str());obs_data_set_string(d,"receiver_scene",m.light.receiverSceneUuid.c_str());obs_data_set_int(d,"light_bind_item",m.light.bindItemId);obs_data_set_int(d,"receiver_item",m.light.receiverItemId);
    obs_data_set_string(d,"light_bind_instance",m.light.bindInstanceId.c_str());obs_data_set_string(d,"receiver_instance",m.light.receiverInstanceId.c_str());
    auto*extra=obs_data_array_create();for(const auto&light:m.extraLights){StageModel single;single.light=light;auto*item=modelData(single);obs_data_erase(item,"extra_lights");obs_data_array_push_back(extra,item);obs_data_release(item);}obs_data_set_array(d,"extra_lights",extra);obs_data_array_release(extra);obs_data_set_int(d,"width",m.width);obs_data_set_int(d,"height",m.height);writeCamera(d,m.camera);
    obs_data_set_bool(d,"playing",m.playing);obs_data_set_bool(d,"loop",m.loop);obs_data_set_int(d,"play_token",int64_t(m.playToken));
    obs_data_set_bool(d,"light_enabled",m.light.enabled);obs_data_set_bool(d,"shadows_enabled",m.light.shadows);writeVec(d,"light_",m.light.position);
    obs_data_set_double(d,"light_red",m.light.color.x);obs_data_set_double(d,"light_green",m.light.color.y);obs_data_set_double(d,"light_blue",m.light.color.z);
    obs_data_set_double(d,"light_intensity",m.light.intensity);obs_data_set_double(d,"light_range",m.light.range);obs_data_set_double(d,"light_ambient",m.light.ambient);obs_data_set_double(d,"shadow_opacity",m.light.shadowOpacity);obs_data_set_double(d,"receiver_z",m.light.receiverZ);
    obs_data_set_double(d,"shadow_red",m.light.shadowColor.x);obs_data_set_double(d,"shadow_green",m.light.shadowColor.y);obs_data_set_double(d,"shadow_blue",m.light.shadowColor.z);
    obs_data_set_double(d,"fx_exposure",m.fx.exposure);obs_data_set_double(d,"fx_saturation",m.fx.saturation);obs_data_set_double(d,"fx_vignette",m.fx.vignette);
    auto *layers=obs_data_array_create();
    for(const auto &l:m.layers){auto *item=obs_data_create();obs_data_set_string(item,"uuid",l.sourceUuid.c_str());obs_data_set_string(item,"scene_uuid",l.sceneUuid.c_str());obs_data_set_string(item,"label",l.label.c_str());obs_data_set_int(item,"item_id",l.itemId);writeVec(item,"",l.plane.position);writeVec(item,"r",l.plane.rotation);obs_data_set_double(item,"sx",l.plane.scaleX);obs_data_set_double(item,"sy",l.plane.scaleY);obs_data_set_double(item,"opacity",l.opacity);obs_data_set_bool(item,"visible",l.visible);obs_data_set_bool(item,"original_visible",l.originalVisible);obs_data_set_bool(item,"enabled",l.enabled);obs_data_set_bool(item,"receive_light",l.receiveLight);obs_data_set_bool(item,"cast_shadow",l.castShadow);obs_data_set_bool(item,"receive_shadow",l.receiveShadow);obs_data_set_bool(item,"rim_light",l.rimLight);
        obs_data_set_double(item,"perspective",l.perspective);obs_data_set_double(item,"focus_blur",l.focusBlur);obs_data_set_int(item,"depth_order",l.depthOrder);obs_data_set_int(item,"crop_left",l.cropLeft);obs_data_set_int(item,"crop_right",l.cropRight);obs_data_set_int(item,"crop_top",l.cropTop);obs_data_set_int(item,"crop_bottom",l.cropBottom);
        obs_data_set_double(item,"bounds_width",l.boundsWidth);obs_data_set_double(item,"bounds_height",l.boundsHeight);obs_data_set_int(item,"bounds_type",l.boundsType);
        obs_data_set_bool(item,"follow_source_size",l.followSourceSize);obs_data_set_double(item,"import_width",l.importWidth);obs_data_set_double(item,"import_height",l.importHeight);obs_data_set_double(item,"source_sx",l.importTransform.scaleX);obs_data_set_double(item,"source_sy",l.importTransform.scaleY);obs_data_set_int(item,"source_alignment",l.importTransform.alignment);obs_data_set_int(item,"bounds_alignment",l.importTransform.boundsAlignment);obs_data_set_bool(item,"crop_to_bounds",l.importTransform.cropToBounds);
        obs_data_set_bool(item,"has_default",l.hasDefault);writeVec(item,"default_",l.defaultPlane.position);writeVec(item,"default_r",l.defaultPlane.rotation);obs_data_set_double(item,"default_sx",l.defaultPlane.scaleX);obs_data_set_double(item,"default_sy",l.defaultPlane.scaleY);
        writeFxStack(item,l.fx);obs_data_set_string(item,"instance_id",l.instanceId.c_str());obs_data_set_int(item,"instance_number",l.instanceNumber);writeAnimation(item,l.animation);writeOrbit(item,l.orbit);obs_data_set_bool(item,"animation_locked",l.locked);obs_data_set_double(item,"default_opacity",l.defaultOpacity);obs_data_array_push_back(layers,item);obs_data_release(item);}obs_data_set_array(d,"layers",layers);obs_data_array_release(layers);
}
obs_data_t *modelData(const StageModel &m){auto *d=obs_data_create();writeModel(m,d);return d;}
}

