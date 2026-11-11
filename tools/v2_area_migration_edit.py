from pathlib import Path
p=Path("src/model.cpp");s=p.read_text(encoding="utf-8-sig");s=s.replace('    return m;\n}', '''    if(!has(d,"animation_duration")&&m.keyframes.size()>1)m.animationDuration=std::max(.1,m.keyframes.back().time-m.keyframes.front().time);
    // v1 assigned every imported plane Z=0. Upgrade that flat layout once,
    // compensating camera-relative scale/position so the saved view does not jump.
    if(obs_data_get_int(d,"schema_version")<2&&m.layers.size()>1&&std::all_of(m.layers.begin(),m.layers.end(),[](const Layer&l){return std::abs(l.plane.position.z)<1e-6;})){
        const double distance=-m.camera.position.z;
        if(distance>20)for(size_t i=0;i<m.layers.size();++i){auto&l=m.layers[i];const double z=double(m.layers.size()-i-1)*180,t=(distance+z)/distance;
            l.plane.position=add(m.camera.position,multiply(sub(l.plane.position,m.camera.position),t));l.plane.scaleX*=t;l.plane.scaleY*=t;l.depthOrder=int(i);l.defaultPlane=l.plane;l.hasDefault=true;}
    }
    return m;
}''')
# model test target links model.cpp only; migration arithmetic uses no core linkage.
s=s.replace('l.plane.position=add(m.camera.position,multiply(sub(l.plane.position,m.camera.position),t));','l.plane.position={m.camera.position.x+(l.plane.position.x-m.camera.position.x)*t,m.camera.position.y+(l.plane.position.y-m.camera.position.y)*t,m.camera.position.z+(l.plane.position.z-m.camera.position.z)*t};')
p.write_text(s,encoding="utf-8")
p=Path("src/stage.cpp");s=p.read_text(encoding="utf-8-sig").replace('scalar(s.effect,"soft_radius",0);','scalar(s.effect,"soft_radius",0);scalar(s.effect,"light_extent",0);')
s=s.replace('scalar(s.effect,"light_falloff",light.falloff);','scalar(s.effect,"light_falloff",light.falloff);scalar(s.effect,"light_extent",light.type?light.size*.5:0);')
p.write_text(s,encoding="utf-8")
p=Path("data/effects/spatial.effect");s=p.read_text(encoding="utf-8-sig").replace('uniform float soft_radius;','uniform float soft_radius;\nuniform float light_extent;')
s=s.replace('float4 PSDefault(VertData v) : TARGET {', '''float directAmount(float3 world,float2 offset) {
    float3 ray=light_position+float3(offset*light_extent,0)-world;
    float distance=max(length(ray),.001);
    return pow(saturate(1.0-distance/max(light_range,1.0)),light_falloff)*max(dot(normalize(plane_normal),ray/distance),0.0);
}
float4 PSDefault(VertData v) : TARGET {''')
s=s.replace('float3 lit=(rgb*diffuse+rim)*light_color*light_intensity*attenuation*visibility;', '''float amount=directAmount(world,float2(0,0));
    if(light_extent>.01)amount=(amount+directAmount(world,float2(-.8,-.8))+directAmount(world,float2(.8,-.8))+directAmount(world,float2(-.8,.8))+directAmount(world,float2(.8,.8)))/5.0;
    float3 lit=(rgb*amount+rim*attenuation)*light_color*light_intensity*visibility;''')
# remove unused diffuse/normal optimization concern: plane_normal still used in directAmount.
s=s.replace('    float diffuse=max(dot(normalize(plane_normal),ray/distance),0.0);\n','')
p.write_text(s,encoding="utf-8")
p=Path("src/dock.cpp");s=p.read_text(encoding="utf-8-sig").replace('initV2(layerForm,cameraForm,lightForm);','''// Legacy v1 receiver-Z data remains readable; v2 receives on actual selectable materials.
    lightFields[7]->hide();if(auto*label=lightForm->labelForField(lightFields[7]))label->hide();
    layerFields[6]->setRange(-100,100);layerFields[7]->setRange(-100,100);
    initV2(layerForm,cameraForm,lightForm);''');p.write_text(s,encoding="utf-8")
p=Path("src/dock_actions.cpp");s=p.read_text(encoding="utf-8-sig").replace('model.fx={};model.light={};','model.fx={};model.light={};model.extraLights.clear();model.lookAtTarget=false;model.target={};')
s=s.replace('current.visible=identity.visible;','current.visible=identity.visible;current.defaultPlane=identity.defaultPlane;current.hasDefault=identity.hasDefault;')
p.write_text(s,encoding="utf-8")
