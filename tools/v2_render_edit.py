from pathlib import Path
p=Path("src/stage.cpp");s=p.read_text(encoding="utf-8-sig")
s=s.replace('gs_effect_t *effect=nullptr;gs_vertbuffer_t *vertices=nullptr;gs_texture_t *white=nullptr;', 'gs_effect_t *effect=nullptr;gs_vertbuffer_t *vertices=nullptr;gs_texture_t *white=nullptr;gs_texrender_t *shadow=nullptr;')
s=s.replace('s.ready.store(s.vertices&&s.white);','s.shadow=gs_texrender_create(GS_RGBA,GS_ZS_NONE);\n    s.ready.store(s.vertices&&s.white&&s.shadow);')
s=s.replace('gs_texture_destroy(s->white);obs_leave_graphics();','gs_texture_destroy(s->white);gs_texrender_destroy(s->shadow);obs_leave_graphics();')
start=s.index('void render(void *data,gs_effect_t*){')
end=s.index('\nvoid destroy(void *data)',start)
s=s[:start]+r'''
void vec4param(gs_effect_t *e,const char*name,double x,double y,double z,double w){struct vec4 v;vec4_set(&v,float(x),float(y),float(z),float(w));gs_effect_set_vec4(gs_effect_get_param_by_name(e,name),&v);}
void cropParam(Stage &s,const char*name,const Layer&l,uint32_t w,uint32_t h){
    const double left=std::clamp(double(l.cropLeft)/std::max(1u,w),0.0,.999),top=std::clamp(double(l.cropTop)/std::max(1u,h),0.0,.999);
    const double right=std::clamp(double(l.cropRight)/std::max(1u,w),0.0,.999-left),bottom=std::clamp(double(l.cropBottom)/std::max(1u,h),0.0,.999-top);
    vec4param(s.effect,name,left,top,1-left-right,1-top-bottom);
}
Plane effectivePlane(const Layer &layer){
    auto p=layer.plane;const double denominator=1500+p.position.z;
    if(std::abs(denominator)>20){
        const double factor=(1500+p.position.z*layer.perspective)/denominator;
        p.position.x*=factor;p.position.y*=factor;p.position.z*=layer.perspective;p.scaleX*=factor;p.scaleY*=factor;
    }return p;
}
void planeParams(Stage&s,const char*prefix,const Plane&p,double w,double h){
    const std::string root=prefix;
    vec3param(s.effect,(root+"center").c_str(),p.position);
    vec3param(s.effect,(root+"right").c_str(),multiply(rotate({1,0,0},p.rotation),w*p.scaleX));
    vec3param(s.effect,(root+"down").c_str(),multiply(rotate({0,1,0},p.rotation),h*p.scaleY));
    vec3param(s.effect,(root+"normal").c_str(),rotate({0,0,-1},p.rotation));
}
void flatGeometry(Stage&s,uint32_t w,uint32_t h){
    auto*vb=gs_vertexbuffer_get_data(s.vertices);vec3_set(&vb->points[0],0,0,0);vec3_set(&vb->points[1],float(w),0,0);vec3_set(&vb->points[2],0,float(h),0);vec3_set(&vb->points[3],float(w),float(h),0);
    gs_vertexbuffer_flush(s.vertices);gs_load_vertexbuffer(s.vertices);gs_load_indexbuffer(nullptr);vec4param(s.effect,"depths",1,1,1,1);
}
void render(void *data,gs_effect_t*){
    auto &s=*static_cast<Stage*>(data);Read read(s);if(!read.value||!resources(s))return;
    const auto started=std::chrono::steady_clock::now();const auto &m=read.value->model;
    double time=s.elapsed*m.animationSpeed;
    if(m.keyframes.size()>1)time=m.keyframes.front().time+time*(m.keyframes.back().time-m.keyframes.front().time)/m.animationDuration;
    Camera camera=m.playing&&!m.keyframes.empty()?sample(m.keyframes,time,m.loop):m.camera;
    if(m.lookAtTarget)camera=lookAt(camera,m.target);if(!m.preview)camera={};
    const uint64_t frame=obs_get_video_frame_time();uint32_t missing=0;
    gs_blend_state_push();gs_enable_blending(true);gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
    const auto cull=gs_get_cull_mode();gs_set_cull_mode(GS_NEITHER);
    std::array<size_t,32> order{};std::array<uint32_t,32> widths{},heights{},sourceWidths{},sourceHeights{};std::array<Plane,32> planes{};size_t count=0;
    for(size_t i=0;i<m.layers.size();++i){
        const auto &l=m.layers[i];if(!l.visible||!l.enabled||l.opacity<=0)continue;
        auto *source=read.value->sources[i]->get();if(!source){++missing;continue;}
        const uint32_t w=obs_source_get_width(source),h=obs_source_get_height(source);
        if(!w||!h){obs_source_release(source);++missing;continue;}
        auto &cache=s.cache[i];
        if(cache.uuid!=l.sourceUuid){
            if(cache.texture)gs_texrender_destroy(cache.texture);cache={};cache.uuid=l.sourceUuid;cache.texture=gs_texrender_create(GS_RGBA,GS_ZS_NONE);
            const double ratio=std::min(1.0,2048.0/std::max(w,h));cache.width=std::max(1u,uint32_t(w*ratio));cache.height=std::max(1u,uint32_t(h*ratio));s.textureAllocations.fetch_add(1);
        }
        if(!cache.texture){obs_source_release(source);++missing;continue;}
        if(cache.frame!=frame){
            gs_texrender_reset(cache.texture);
            if(gs_texrender_begin(cache.texture,cache.width,cache.height)){
                struct vec4 clear;vec4_zero(&clear);gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,float(w),0,float(h),-100,100);
                gs_blend_state_push();gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);obs_source_video_render(source);gs_blend_state_pop();gs_texrender_end(cache.texture);cache.frame=frame;
            }
        }
        obs_source_release(source);if(!gs_texrender_get_texture(cache.texture)){++missing;continue;}
        sourceWidths[i]=w;sourceHeights[i]=h;widths[i]=uint32_t(std::max(1,int(w)-l.cropLeft-l.cropRight));heights[i]=uint32_t(std::max(1,int(h)-l.cropTop-l.cropBottom));planes[i]=effectivePlane(l);order[count++]=i;
    }
    std::stable_sort(order.begin(),order.begin()+count,[&](size_t a,size_t b){
        const double da=inverseRotate(sub(planes[a].position,camera.position),camera.rotation).z,db=inverseRotate(sub(planes[b].position,camera.position),camera.rotation).z;
        if(std::abs(da-db)<.01)return m.layers[a].depthOrder<m.layers[b].depthOrder;return da>db;
    });
    std::array<Light,4> lights{};size_t lightCount=0;
    auto addLight=[&](Light light){
        if(!light.enabled||!light.visible||light.intensity<=0)return;
        if(!light.bindUuid.empty())for(size_t i=0;i<m.layers.size();++i)if(m.layers[i].sourceUuid==light.bindUuid){light.position=add(planes[i].position,rotate(light.position,planes[i].rotation));break;}
        lights[lightCount++]=std::move(light);
    };
    addLight(m.light);for(const auto&light:m.extraLights)if(lightCount<4)addLight(light);
    double ambient=lightCount?0:1;for(size_t i=0;i<lightCount;++i)ambient+=lights[i].ambient;ambient=std::min(ambient,2.0);
    // Initialize all effect uniforms, including inactive branches. No stale D3D constants.
    scalar(s.effect,"opacity",1);scalar(s.effect,"ambient",ambient);scalar(s.effect,"shadow_opacity",0);scalar(s.effect,"light_intensity",0);scalar(s.effect,"light_range",1);scalar(s.effect,"light_falloff",2);scalar(s.effect,"soft_radius",0);scalar(s.effect,"caster_opacity",1);scalar(s.effect,"rim_strength",0);scalar(s.effect,"dof_radius",0);
    scalar(s.effect,"exposure",m.fx.exposure);scalar(s.effect,"saturation",m.fx.saturation);scalar(s.effect,"vignette",m.fx.vignette);
    vec3param(s.effect,"light_position",{});vec3param(s.effect,"light_color",{1,1,1});planeParams(s,"plane_",{},1,1);planeParams(s,"caster_",{},1,1);
    vec4param(s.effect,"uv_rect",0,0,1,1);vec4param(s.effect,"caster_crop",0,0,1,1);vec4param(s.effect,"depths",1,1,1,1);
    struct vec2 texel;vec2_set(&texel,1,1);gs_effect_set_vec2(gs_effect_get_param_by_name(s.effect,"texel_size"),&texel);
    gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"shadow_map"),s.white);gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"caster_image"),s.white);
    for(size_t n=0;n<count;++n){
        const size_t i=order[n];const auto&l=m.layers[i];const auto&p=planes[i];auto*texture=gs_texrender_get_texture(s.cache[i].texture);
        const auto quad=corners(p,widths[i],heights[i]);if(!geometry(s,quad,camera,m))continue;
        planeParams(s,"plane_",p,widths[i],heights[i]);cropParam(s,"uv_rect",l,sourceWidths[i],sourceHeights[i]);
        vec2_set(&texel,1.0f/s.cache[i].width,1.0f/s.cache[i].height);gs_effect_set_vec2(gs_effect_get_param_by_name(s.effect,"texel_size"),&texel);
        const double distance=inverseRotate(sub(p.position,camera.position),camera.rotation).z;
        scalar(s.effect,"dof_radius",std::min(20.0,std::abs(distance-m.focusDistance)/std::max(distance,10.0)*m.aperture*l.focusBlur));
        scalar(s.effect,"opacity",l.opacity);scalar(s.effect,"ambient",l.receiveLight?ambient:1);
        gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);draw(s,texture,0);
        if(!l.receiveLight)continue;
        for(size_t j=0;j<lightCount;++j){
            const auto&light=lights[j];vec3param(s.effect,"light_position",light.position);vec3param(s.effect,"light_color",light.color);
            scalar(s.effect,"light_intensity",light.intensity);scalar(s.effect,"light_range",light.range);scalar(s.effect,"light_falloff",light.falloff);
            scalar(s.effect,"soft_radius",light.softness+(light.type?light.size*.5:light.size*.05));scalar(s.effect,"shadow_opacity",light.shadowOpacity);
            bool hasMask=false;
            if(l.receiveShadow&&light.shadows&&(light.receiverUuid.empty()||light.receiverUuid==l.sourceUuid)){
                gs_texrender_reset(s.shadow);
                if(gs_texrender_begin(s.shadow,512,512)){
                    struct vec4 clear;vec4_zero(&clear);gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,512,0,512,-100,100);flatGeometry(s,512,512);gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
                    for(size_t k=0;k<count;++k){const size_t c=order[k];const auto&caster=m.layers[c];if(c==i||!caster.castShadow)continue;
                        // Only planes between the emitter and this receiver can occlude it.
                        const auto delta=sub(planes[c].position,p.position);const auto normal=rotate({0,0,-1},p.rotation);const auto dot=[](Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
                        if(dot(delta,normal)*dot(sub(light.position,p.position),normal)<=0)continue;
                        planeParams(s,"caster_",planes[c],widths[c],heights[c]);cropParam(s,"caster_crop",caster,sourceWidths[c],sourceHeights[c]);scalar(s.effect,"caster_opacity",caster.opacity);gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"caster_image"),gs_texrender_get_texture(s.cache[c].texture));draw(s,texture,1);
                    }
                    gs_texrender_end(s.shadow);hasMask=true;
                }
            }
            if(!geometry(s,quad,camera,m))continue;
            gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"shadow_map"),hasMask?gs_texrender_get_texture(s.shadow):s.white);
            if(!hasMask)scalar(s.effect,"shadow_opacity",0);
            scalar(s.effect,"rim_strength",l.rimLight?light.rim:0);
            gs_blend_function(GS_BLEND_ONE,GS_BLEND_ONE);draw(s,texture,2);
        }
    }
    gs_load_vertexbuffer(nullptr);gs_set_cull_mode(cull);gs_blend_state_pop();s.missing.store(missing);s.renders.fetch_add(1);s.renderNanoseconds.fetch_add(uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count()));
}
'''+s[end:]
# enabled sources must stay activated while hidden or zero-size so window capture can recover.
s=s.replace('std::stable_sort(order.begin(),order.begin()+count,','std::sort(order.begin(),order.begin()+count,') # fixed array, no per-frame temporary allocation
p.write_text(s,encoding="utf-8")
