from pathlib import Path
p=Path("src/model.cpp")
s=p.read_text(encoding="utf-8-sig")
s=s.replace('m.camera=readCamera(d);','''m.camera=readCamera(d);
    m.target=readVec(d,"camera_target_",{});m.lookAtTarget=boolean(d,"camera_look_at",false);m.preview=boolean(d,"preview",true);
    m.animationSpeed=number(d,"animation_speed",1,.05,10);m.animationDuration=number(d,"animation_duration",6,.1,3600);
    m.focusDistance=number(d,"focus_distance",1500,10,100000);m.aperture=number(d,"aperture",0,0,20);''')
s=s.replace('m.fx.exposure=number', '''m.light.type=int(number(d,"light_type",0,0,1));m.light.size=number(d,"light_size",60,0,2000);m.light.falloff=number(d,"light_falloff",2,.1,8);m.light.softness=number(d,"light_softness",15,0,300);m.light.rim=number(d,"light_rim",1,0,5);
    m.light.visible=boolean(d,"light_visible",true);m.light.bindUuid=obs_data_get_string(d,"light_bind");m.light.receiverUuid=obs_data_get_string(d,"receiver_uuid");
    if(has(d,"light_label"))m.light.label=obs_data_get_string(d,"light_label");
    auto *extra=obs_data_get_array(d,"extra_lights");
    if(extra)for(size_t n=0;n<std::min(size_t(3),obs_data_array_count(extra));++n){auto*entry=obs_data_array_item(extra,n);
        // Additional emitter objects do not recurse through nested emitter arrays.
        auto*clean=obs_data_create();obs_data_apply(clean,entry);obs_data_erase(clean,"extra_lights");m.extraLights.push_back(readModel(clean).light);obs_data_release(clean);obs_data_release(entry);}
    obs_data_array_release(extra);
    m.fx.exposure=number''')
s=s.replace('l.plane.scaleX=number(item,"sx",1,.001,100);l.plane.scaleY=number(item,"sy",1,.001,100);','''l.plane.scaleX=number(item,"sx",1,-100,100);l.plane.scaleY=number(item,"sy",1,-100,100);
        if(std::abs(l.plane.scaleX)<.001)l.plane.scaleX=.001;if(std::abs(l.plane.scaleY)<.001)l.plane.scaleY=.001;
        l.enabled=boolean(item,"enabled",true);l.receiveLight=boolean(item,"receive_light",true);l.castShadow=boolean(item,"cast_shadow",true);l.receiveShadow=boolean(item,"receive_shadow",true);l.rimLight=boolean(item,"rim_light",true);
        l.perspective=number(item,"perspective",1,0,2);l.focusBlur=number(item,"focus_blur",1,0,4);l.depthOrder=int(number(item,"depth_order",0,-1000,1000));
        l.cropLeft=int(number(item,"crop_left",0,0,8192));l.cropRight=int(number(item,"crop_right",0,0,8192));l.cropTop=int(number(item,"crop_top",0,0,8192));l.cropBottom=int(number(item,"crop_bottom",0,0,8192));
        l.hasDefault=boolean(item,"has_default",false);l.defaultPlane.position=readVec(item,"default_",l.plane.position);l.defaultPlane.rotation=readVec(item,"default_r",l.plane.rotation);l.defaultPlane.scaleX=number(item,"default_sx",l.plane.scaleX,-100,100);l.defaultPlane.scaleY=number(item,"default_sy",l.plane.scaleY,-100,100);''')
s=s.replace('obs_data_set_int(d,"schema_version",1);','''obs_data_set_int(d,"schema_version",2);
    writeVec(d,"camera_target_",m.target);obs_data_set_bool(d,"camera_look_at",m.lookAtTarget);obs_data_set_bool(d,"preview",m.preview);
    obs_data_set_double(d,"animation_speed",m.animationSpeed);obs_data_set_double(d,"animation_duration",m.animationDuration);obs_data_set_double(d,"focus_distance",m.focusDistance);obs_data_set_double(d,"aperture",m.aperture);
    obs_data_set_int(d,"light_type",m.light.type);obs_data_set_double(d,"light_size",m.light.size);obs_data_set_double(d,"light_falloff",m.light.falloff);obs_data_set_double(d,"light_softness",m.light.softness);obs_data_set_double(d,"light_rim",m.light.rim);
    obs_data_set_bool(d,"light_visible",m.light.visible);obs_data_set_string(d,"light_bind",m.light.bindUuid.c_str());obs_data_set_string(d,"receiver_uuid",m.light.receiverUuid.c_str());obs_data_set_string(d,"light_label",m.light.label.c_str());
    auto*extra=obs_data_array_create();for(const auto&light:m.extraLights){StageModel single;single.light=light;auto*item=modelData(single);obs_data_erase(item,"extra_lights");obs_data_array_push_back(extra,item);obs_data_release(item);}obs_data_set_array(d,"extra_lights",extra);obs_data_array_release(extra);''')
s=s.replace('obs_data_array_push_back(layers,item);','''obs_data_set_bool(item,"enabled",l.enabled);obs_data_set_bool(item,"receive_light",l.receiveLight);obs_data_set_bool(item,"cast_shadow",l.castShadow);obs_data_set_bool(item,"receive_shadow",l.receiveShadow);obs_data_set_bool(item,"rim_light",l.rimLight);
        obs_data_set_double(item,"perspective",l.perspective);obs_data_set_double(item,"focus_blur",l.focusBlur);obs_data_set_int(item,"depth_order",l.depthOrder);obs_data_set_int(item,"crop_left",l.cropLeft);obs_data_set_int(item,"crop_right",l.cropRight);obs_data_set_int(item,"crop_top",l.cropTop);obs_data_set_int(item,"crop_bottom",l.cropBottom);
        obs_data_set_bool(item,"has_default",l.hasDefault);writeVec(item,"default_",l.defaultPlane.position);writeVec(item,"default_r",l.defaultPlane.rotation);obs_data_set_double(item,"default_sx",l.defaultPlane.scaleX);obs_data_set_double(item,"default_sy",l.defaultPlane.scaleY);
        obs_data_array_push_back(layers,item);''')
p.write_text(s,encoding="utf-8")
p=Path("src/core.hpp");s=p.read_text(encoding="utf-8-sig");s=s.replace('bool shadowPoint', '''Camera lookAt(Camera camera,Vec3 target);
bool shadowOnPlane(Vec3 point,Vec3 light,const Plane &receiver,Vec3 &out);
Plane importPlane(double screenX,double screenY,double width,double height,double scaleX,double scaleY,double rotation,double depth,const Camera &camera,double canvasWidth,double canvasHeight);
bool shadowPoint''');p.write_text(s,encoding="utf-8")
p=Path("src/core.cpp");s=p.read_text(encoding="utf-8-sig");pos=s.rfind("\n}")
s=s[:pos]+'''
Camera lookAt(Camera camera,Vec3 target){
    const auto delta=sub(target,camera.position);
    const double horizontal=std::hypot(delta.x,delta.z);
    if(horizontal<1e-6&&std::abs(delta.y)<1e-6)return camera;
    camera.rotation.x=-std::atan2(delta.y,horizontal)*180/pi;
    camera.rotation.y=std::atan2(delta.x,delta.z)*180/pi;
    return camera;
}
bool shadowOnPlane(Vec3 point,Vec3 light,const Plane &receiver,Vec3 &out){
    if(!finite(point)||!finite(light)||!finite(receiver.position)||!finite(receiver.rotation))return false;
    const auto n=rotate({0,0,1},receiver.rotation),ray=sub(point,light);
    const auto dot=[](Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    const double denominator=dot(n,ray);
    if(std::abs(denominator)<1e-6)return false;
    const double t=dot(n,sub(receiver.position,light))/denominator;
    if(t<1||t>1000||!std::isfinite(t))return false;
    out=add(light,multiply(ray,t));return finite(out);
}
Plane importPlane(double screenX,double screenY,double,double,double sx,double sy,double rotation,double depth,const Camera &camera,double canvasWidth,double canvasHeight){
    Plane p;const double distance=std::max(20.0,depth-camera.position.z);
    const double focal=canvasHeight/(2*std::tan(camera.fov*pi/360));
    const double compensation=distance/focal;
    p.position=add(camera.position,rotate({(screenX-canvasWidth*.5)*compensation,(screenY-canvasHeight*.5)*compensation,distance},camera.rotation));
    p.rotation=camera.rotation;p.rotation.z+=rotation;p.scaleX=sx*compensation;p.scaleY=sy*compensation;return p;
}
'''+s[pos:];p.write_text(s,encoding="utf-8")
