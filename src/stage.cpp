#include "stage.hpp"
#include "model.hpp"
#include "fx_renderer.hpp"
#include "fx_dependencies.hpp"
#include "fx_motion.hpp"
#include "fx_stage.hpp"
#include "scene_evaluator.hpp"
#include "presets/graphics.hpp"
#include "fx_blur.hpp"
#include "shadow_blur.hpp"
#include <obs-module.h>
#include <graphics/graphics.h>
#include <graphics/vec2.h>
#include <graphics/vec3.h>
#include <graphics/vec4.h>
#include <util/bmem.h>
#include <util/platform.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_set>
namespace spatial {
namespace {
struct WeakSource {
    obs_weak_source_t *weak=nullptr;
    explicit WeakSource(obs_source_t *source){if(source)weak=obs_source_get_weak_source(source);}
    ~WeakSource(){if(weak)obs_weak_source_release(weak);}
    WeakSource(const WeakSource&)=delete;WeakSource&operator=(const WeakSource&)=delete;
    obs_source_t *get()const{auto *s=weak?obs_weak_source_get_source(weak):nullptr;if(s&&obs_source_removed(s)){obs_source_release(s);return nullptr;}return s;}
};
struct Snapshot {
    StageModel model;
    std::vector<std::unique_ptr<WeakSource>> sources;std::unique_ptr<WeakSource> media;
};
struct Cache {std::string uuid;gs_texrender_t *texture=nullptr;uint32_t width=0,height=0;uint64_t frame=UINT64_MAX;};
struct Stage {
    obs_source_t *owner=nullptr;
    std::atomic<Snapshot*> published{nullptr};
    std::atomic<bool> uiUpdateQueued{false};
    std::atomic<double> clockPosition{0};std::atomic<bool> clockPlaying{false},clockPreview{false};std::atomic<uint64_t> clockToken{0};
    std::atomic<uint32_t> readers{0};
    std::vector<Snapshot*> retired; // Only touched by serialized update/destroy callbacks.
    std::atomic<uint32_t> width{1920},height{1080};
    std::array<Cache,32> cache;
    std::unique_ptr<FxRenderer> fx=std::make_unique<FxRenderer>();
    std::unique_ptr<presets::GraphicsRenderer> graphics=std::make_unique<presets::GraphicsRenderer>();
    struct MotionCache {std::string instance;Projected previous{},current{};uint64_t frame=0;bool valid=false;};
    std::array<MotionCache,32>motion;double effectEpoch=0;
    gs_effect_t *effect=nullptr,*shadowKernel=nullptr;gs_vertbuffer_t *vertices=nullptr;gs_texture_t *white=nullptr;gs_texrender_t *shadow=nullptr,*casterShadow=nullptr,*composite=nullptr;
    std::unique_ptr<FxBlur> shadowBlur;std::string shadowKernelSource;uint64_t shadowAllocations=0,shadowPasses=0,shadowFailures=0;bool shadowKernelAttempted=false;
    bool effectAttempted=false;uint64_t lastToken=UINT64_MAX;double elapsed=0;
    std::atomic<uint64_t> renders{0},renderNanoseconds{0},textureAllocations{0},fxAllocations{0},fxPasses{0},fxFailures{0};
    std::atomic<uint32_t> missing{0};std::atomic<double> timeline{0},effectTime{0};std::atomic<bool> ready{false};
};
// Readers never wait for the writer. Sequentially consistent entry protects old snapshots
// until a later update observes a quiescent state; reclamation never waits in video_render.
struct Read {
    Stage &stage;Snapshot *value;
    explicit Read(Stage &s):stage(s){stage.readers.fetch_add(1);value=stage.published.load();}
    ~Read(){stage.readers.fetch_sub(1);}
};
void reclaim(Stage &stage){if(stage.readers.load()==0){for(auto *p:stage.retired)delete p;stage.retired.clear();}}
std::unordered_set<std::string> activeIds(const Snapshot *snapshot){
    std::unordered_set<std::string> result;
    if(snapshot){const auto required=fxCaptureLayers(snapshot->model.layers);for(size_t i=0;i<snapshot->sources.size();++i)if(required[i]&&snapshot->sources[i]->weak)result.insert(snapshot->model.layers[i].sourceUuid);}
    return result;
}
void applySettings(void *data,obs_data_t *settings){
    auto &s=*static_cast<Stage*>(data);
    try {
        auto next=std::make_unique<Snapshot>();next->model=readModel(settings);
#if SPATIAL_PHASE < 3
        next->model.playing=false;
#endif
#if SPATIAL_PHASE < 4
        next->model.light.enabled=false;next->model.fx={};
#endif
        if(next->model.mediaLinked){auto*media=obs_get_source_by_uuid(next->model.mediaUuid.c_str());next->media=std::make_unique<WeakSource>(media);obs_source_release(media);}
        auto *old=s.published.load();const auto oldIds=activeIds(old);std::unordered_set<std::string> activated,rejected;
        const auto required=fxCaptureLayers(next->model.layers);size_t layerIndex=0;
        for(auto &layer:next->model.layers){
            auto *source=obs_get_source_by_uuid(layer.sourceUuid.c_str());
            const char *id=source?obs_source_get_unversioned_id(source):"";
            bool allowed=source&&!obs_source_removed(source)&&source!=s.owner&&obs_source_get_type(source)==OBS_SOURCE_TYPE_INPUT&&std::string(id)!="scene"&&std::string(id)!="group"&&std::string(id)!=stageId;
            if(allowed&&required[layerIndex]&&!oldIds.count(layer.sourceUuid)){
                if(rejected.count(layer.sourceUuid))allowed=false;
                else if(!activated.count(layer.sourceUuid)){
                    allowed=obs_source_add_active_child(s.owner,source);
                    (allowed?activated:rejected).insert(layer.sourceUuid);
                }
            }
            next->sources.push_back(std::make_unique<WeakSource>(allowed?source:nullptr));
            obs_source_release(source);
            ++layerIndex;
        }
        const auto newIds=activeIds(next.get());
        if(old)for(const auto &id:oldIds)if(!newIds.count(id)){auto *source=obs_get_source_by_uuid(id.c_str());if(source){obs_source_remove_active_child(s.owner,source);obs_source_release(source);}}
        s.clockPosition.store(next->model.timelinePosition);s.clockPlaying.store(next->model.playing);s.clockPreview.store(next->model.timelinePreview);s.clockToken.store(next->model.playToken);s.width.store(next->model.width);s.height.store(next->model.height);
        old=s.published.exchange(next.release());if(old)s.retired.push_back(old);reclaim(s);
    } catch(const std::exception &error){blog(LOG_ERROR,"[SpatialFX] update rejected: %s",error.what());}
}
void update(void *data,obs_data_t*){
    auto &s=*static_cast<Stage*>(data);
    // OBS mutates public settings before calling this on the video thread.
    // Do not read that shared hash/arrays. Native edits and owned proc requests
    // use the private UI model; generic settings are creation/legacy data only.
    if(!s.uiUpdateQueued.exchange(true))blog(LOG_WARNING,"[SpatialFX] generic live settings update ignored; use native Dock or spatial_fx_set_state proc API");
}
void procSetState(void *data,calldata_t *request){
    auto &s=*static_cast<Stage*>(data);auto *settings=static_cast<obs_data_t*>(calldata_ptr(request,"settings"));if(!settings)return;
    struct Pending{obs_weak_source_t *weak;obs_data_t *settings;};
    auto *copy=obs_data_create();obs_data_apply(copy,settings);auto *pending=new Pending{obs_source_get_weak_source(s.owner),copy};
    obs_queue_task(OBS_TASK_UI,[](void *opaque){auto *p=static_cast<Pending*>(opaque);auto *source=obs_weak_source_get_source(p->weak);if(source&&!obs_source_removed(source))stageApplySettings(source,p->settings);obs_source_release(source);obs_weak_source_release(p->weak);obs_data_release(p->settings);delete p;},pending,false);
}
void procGetState(void *data,calldata_t *request){auto &s=*static_cast<Stage*>(data);calldata_set_ptr(request,"settings",stageGetSettings(s.owner));}
void procMetrics(void *data,calldata_t*request){auto&s=*static_cast<Stage*>(data);calldata_set_int(request,"fx_allocations",int64_t(s.fxAllocations.load()));calldata_set_int(request,"fx_passes",int64_t(s.fxPasses.load()));calldata_set_int(request,"fx_failures",int64_t(s.fxFailures.load()));calldata_set_int(request,"renders",int64_t(s.renders.load()));calldata_set_float(request,"render_ms",s.renders.load()?double(s.renderNanoseconds.load())/s.renders.load()/1e6:0);}

void enumerate(void *data,obs_source_enum_proc_t callback,void *param,bool all){
    auto &s=*static_cast<Stage*>(data);Read read(s);if(!read.value)return;
    // A source referenced by several planes is enumerated once to balance activation counts.
    const auto required=fxCaptureLayers(read.value->model.layers);
    for(size_t i=0;i<read.value->sources.size();++i){
        if(!all&&!required[i])continue;
        bool duplicate=false;
        for(size_t j=0;j<i;++j)if((all||required[j])&&read.value->model.layers[j].sourceUuid==read.value->model.layers[i].sourceUuid){duplicate=true;break;}
        if(duplicate)continue;
        auto *source=read.value->sources[i]->get();if(source){callback(s.owner,source,param);obs_source_release(source);}
    }
}
void tick(void *data,float seconds){
    auto &s=*static_cast<Stage*>(data);Read read(s);if(!read.value)return;
    const auto token=s.clockToken.load();if(s.lastToken!=token){s.lastToken=token;s.elapsed=0;}
    if(s.clockPlaying.load()&&std::isfinite(seconds))s.elapsed+=std::clamp(double(seconds),0.0,1.0);
    // Evaluated timeline is published by render(), never the raw elapsed clock.
}
void vec3param(gs_effect_t *e,const char *name,Vec3 value){struct vec3 v;vec3_set(&v,float(value.x),float(value.y),float(value.z));gs_effect_set_vec3(gs_effect_get_param_by_name(e,name),&v);}
void scalar(gs_effect_t *e,const char *name,double value){gs_effect_set_float(gs_effect_get_param_by_name(e,name),float(value));}
bool resources(Stage &s){
    if(s.effectAttempted)return s.effect&&s.vertices;
    s.effectAttempted=true;
    char *path=obs_module_file("effects/spatial.effect"),*error=nullptr;
    if(path)s.effect=gs_effect_create_from_file(path,&error);
    if(!s.effect)blog(LOG_ERROR,"[SpatialFX] effect compilation failed: %s",error?error:"effect file missing");
    bfree(path);bfree(error);if(!s.effect)return false;
    auto *vb=gs_vbdata_create();vb->num=4;vb->points=static_cast<vec3*>(bzalloc(4*sizeof(vec3)));
    vb->num_tex=1;vb->tvarray=static_cast<gs_tvertarray*>(bzalloc(sizeof(gs_tvertarray)));vb->tvarray[0].width=2;vb->tvarray[0].array=bzalloc(4*sizeof(vec2));
    auto *uv=static_cast<vec2*>(vb->tvarray[0].array);vec2_set(&uv[0],0,0);vec2_set(&uv[1],1,0);vec2_set(&uv[2],0,1);vec2_set(&uv[3],1,1);
    s.vertices=gs_vertexbuffer_create(vb,GS_DYNAMIC);
    const uint8_t pixel[4]={255,255,255,255};const uint8_t *pixels=pixel;s.white=gs_texture_create(1,1,GS_RGBA,1,&pixels,0);
    s.shadow=gs_texrender_create(GS_RGBA,GS_ZS_NONE);s.composite=gs_texrender_create(GS_RGBA,GS_ZS_NONE);
    s.ready.store(s.vertices&&s.white&&s.shadow);blog(LOG_INFO,"[SpatialFX] reusable GPU effect and quad created in video_render");
    return s.vertices&&s.white;
}
bool geometry(Stage &s,const std::array<Vec3,4>&points,const Camera &camera,const StageModel &m){
    std::array<Projected,4> p;for(size_t i=0;i<4;++i){p[i]=project(points[i],camera,m.width,m.height);if(!p[i].visible)return false;}
    auto *vb=gs_vertexbuffer_get_data(s.vertices);for(size_t i=0;i<4;++i)vec3_set(&vb->points[i],float(p[i].x),float(p[i].y),0);
    gs_vertexbuffer_flush(s.vertices);gs_load_vertexbuffer(s.vertices);gs_load_indexbuffer(nullptr);
    struct vec4 depths;vec4_set(&depths,float(p[0].depth),float(p[1].depth),float(p[2].depth),float(p[3].depth));gs_effect_set_vec4(gs_effect_get_param_by_name(s.effect,"depths"),&depths);
    return true;
}
void draw(Stage &s,gs_texture_t *texture,int mode){
    gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"image"),texture);
    gs_effect_set_int(gs_effect_get_param_by_name(s.effect,"pass_mode"),mode);
    while(gs_effect_loop(s.effect,"Draw"))gs_draw(GS_TRISTRIP,0,4);
}
bool shadowResources(Stage&s){
    if(!s.shadowKernelAttempted){
        s.shadowKernelAttempted=true;char*error=nullptr;
        if(!s.shadowKernelSource.empty())s.shadowKernel=gs_effect_create(s.shadowKernelSource.c_str(),"spatial-shadow-blur",&error);
        if(!s.shadowKernel){++s.shadowFailures;blog(LOG_ERROR,"[SpatialFX] shadow blur compilation failed: %s",error?error:"effect missing");}
        bfree(error);
    }
    if(!s.casterShadow){s.casterShadow=gs_texrender_create(GS_RGBA,GS_ZS_NONE);++s.shadowAllocations;}
    if(!s.shadowBlur)s.shadowBlur=std::make_unique<FxBlur>();
    return s.shadowKernel&&s.casterShadow;
}

void vec4param(gs_effect_t *e,const char*name,double x,double y,double z,double w){struct vec4 v;vec4_set(&v,float(x),float(y),float(z),float(w));gs_effect_set_vec4(gs_effect_get_param_by_name(e,name),&v);}
void cropParam(Stage &s,const char*name,const Layer&l,uint32_t w,uint32_t h){
    const double left=std::clamp(double(l.cropLeft)/std::max(1u,w),0.0,.999),top=std::clamp(double(l.cropTop)/std::max(1u,h),0.0,.999);
    const double right=std::clamp(double(l.cropRight)/std::max(1u,w),0.0,.999-left),bottom=std::clamp(double(l.cropBottom)/std::max(1u,h),0.0,.999-top);
    vec4param(s.effect,name,left,top,1-left-right,1-top-bottom);
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
    const double start=s.clockPosition.load();double time=s.clockPlaying.load()?timelineTime(s.elapsed,start,m.animationSpeed,playbackEnd(m),m.loop):start;
    if(m.mediaLinked&&s.clockPreview.load()&&read.value->media){auto*media=read.value->media->get();if(media){auto state=obs_source_media_get_state(media);if(state!=OBS_MEDIA_STATE_NONE&&state!=OBS_MEDIA_STATE_STOPPED)time=std::clamp(obs_source_media_get_time(media)/1000.0+m.mediaOffset,0.0,playbackEnd(m));else time=start;obs_source_release(media);}}
    s.timeline.store(time);
    const bool evaluate=s.clockPlaying.load()||s.clockPreview.load();
    std::vector<SourceSize> sourceSizes;sourceSizes.reserve(m.layers.size());
    for(auto&weak:read.value->sources){auto*source=weak->get();sourceSizes.push_back({source?double(obs_source_get_width(source)):0,source?double(obs_source_get_height(source)):0});obs_source_release(source);}
    const double frameTime=double(obs_get_video_frame_time())/1e9;if(s.effectEpoch==0)s.effectEpoch=frameTime;const double fxTime=std::max(0.0,frameTime-s.effectEpoch);
    s.effectTime.store(fxTime);
    const auto evaluated=evaluateSceneAt(m,toTime(time),sourceSizes,evaluate,fxTime);const auto camera=evaluated.camera;
    const auto widescreen=stageWidescreen(m,&evaluated);
    const bool composite=widescreen.enabled||std::abs(evaluated.outputScaleX-1)>1e-9||std::abs(evaluated.outputScaleY-1)>1e-9||std::abs(evaluated.outputOpacity-1)>1e-9;
    if(composite){gs_texrender_reset(s.composite);if(!gs_texrender_begin(s.composite,m.width,m.height))return;struct vec4 clear;vec4_zero(&clear);gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,float(m.width),0,float(m.height),-100,100);}
    const uint64_t frame=obs_get_video_frame_time();uint32_t missing=0;
    gs_blend_state_push();gs_enable_blending(true);gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);
    const auto cull=gs_get_cull_mode();gs_set_cull_mode(GS_NEITHER);
    std::array<size_t,32> order{};std::array<uint32_t,32> widths{},heights{},sourceWidths{},sourceHeights{};std::array<Plane,32> planes{};size_t count=0;std::array<Layer,32> drawLayers{};
    const auto required=fxCaptureLayers(m.layers);std::vector<gs_texture_t*>raw(m.layers.size(),nullptr);
    for(size_t i=0;i<m.layers.size();++i){
        Layer l=m.layers[i];l.plane=evaluated.authoring[i].plane;l.opacity=evaluated.authoring[i].opacity;planes[i]=evaluated.world[i].plane;if(!required[i])continue;
        auto *source=read.value->sources[i]->get();if(!source){++missing;continue;}
        const uint32_t w=obs_source_get_width(source),h=obs_source_get_height(source);
        if(!w||!h){obs_source_release(source);++missing;continue;}
        auto &cache=s.cache[i];
        if(cache.uuid!=l.sourceUuid){
            if(cache.texture)gs_texrender_destroy(cache.texture);cache={};cache.uuid=l.sourceUuid;cache.texture=gs_texrender_create(GS_RGBA,GS_ZS_NONE);
            const double ratio=std::min(1.0,2048.0/std::max(w,h));cache.width=std::max(1u,uint32_t(w*ratio));cache.height=std::max(1u,uint32_t(h*ratio));s.textureAllocations.fetch_add(1);
        }
        if(!cache.texture){obs_source_release(source);++missing;continue;}
        const double ratio=std::min(1.0,2048.0/std::max(w,h));
        const uint32_t captureW=std::max(1u,uint32_t(w*ratio)),captureH=std::max(1u,uint32_t(h*ratio));
        if(cache.width!=captureW||cache.height!=captureH){cache.width=captureW;cache.height=captureH;cache.frame=UINT64_MAX;}
        if(cache.frame!=frame){
            gs_texrender_reset(cache.texture);
            if(gs_texrender_begin(cache.texture,cache.width,cache.height)){
                struct vec4 clear;vec4_zero(&clear);gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,float(w),0,float(h),-100,100);
                gs_blend_state_push();gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);obs_source_video_render(source);gs_blend_state_pop();gs_texrender_end(cache.texture);cache.frame=frame;
            }
        }
        obs_source_release(source);if(!gs_texrender_get_texture(cache.texture)){++missing;continue;}
        raw[i]=gs_texrender_get_texture(cache.texture);
        if(!l.visible||!l.enabled||l.opacity<=0)continue;
        drawLayers[i]=l;
        if(l.followSourceSize&&l.importWidth>0&&l.importHeight>0){
            const auto current=sourceLayout(w,h,l.importTransform),ref=sourceLayout(l.importWidth,l.importHeight,l.importTransform);
            auto &d=drawLayers[i];auto&p=d.plane;
            if(std::abs(ref.scaleX)>.00001&&std::abs(ref.scaleY)>.00001){
                p.position=add(p.position,rotate({(current.x-ref.x)/ref.scaleX*p.scaleX,(current.y-ref.y)/ref.scaleY*p.scaleY,0},p.rotation));
                p.scaleX*=current.scaleX/ref.scaleX;p.scaleY*=current.scaleY/ref.scaleY;
            }
            d.cropLeft=current.left;d.cropRight=current.right;d.cropTop=current.top;d.cropBottom=current.bottom;
        }
        const auto &dl=drawLayers[i];
        sourceWidths[i]=w;sourceHeights[i]=h;widths[i]=uint32_t(std::max(1,int(w)-dl.cropLeft-dl.cropRight));heights[i]=uint32_t(std::max(1,int(h)-dl.cropTop-dl.cropBottom));planes[i]=effectivePlane(dl);
        if(!l.followSourceSize&&l.boundsType&&l.boundsWidth>0&&l.boundsHeight>0){
            double x=l.boundsWidth/widths[i],y=l.boundsHeight/heights[i];
            if(l.boundsType!=1){double factor=std::min(x,y);if(l.boundsType==3)factor=std::max(x,y);if(l.boundsType==4)factor=x;if(l.boundsType==5)factor=y;if(l.boundsType==6)factor=std::min(1.0,factor);x=y=factor;}
            planes[i].scaleX*=x;planes[i].scaleY*=y;
        }planes[i]=evaluated.world[i].plane;order[count++]=i;
    }
    // Capture every input before resolving references; forward references never read stale surfaces.
    std::vector<gs_texture_t*>textures(m.layers.size(),nullptr),casterTextures(m.layers.size(),nullptr);
    std::array<double,32>paddingX{},paddingY{};
    s.fx->beginFrame(m.layers);
    std::array<bool,32> drawn{};for(size_t n=0;n<count;++n)drawn[order[n]]=true;
    // Hidden/missing frames break the motion history; reappearing is not motion.
    for(size_t i=0;i<s.motion.size();++i)if(!drawn[i])s.motion[i]={};
    for(size_t n=0;n<count;++n){const auto i=order[n];auto&motion=s.motion[i];auto center=project(planes[i].position,camera,m.width,m.height);
      if(motion.instance!=m.layers[i].instanceId){motion={};motion.instance=m.layers[i].instanceId;}
      if(motion.frame!=frame){motion.previous=motion.current;motion.valid=motion.frame!=0;motion.current=center;motion.frame=frame;}
      FxRenderContext context;context.frame=frame;context.elapsed=fxTime;context.width=s.cache[i].width;context.height=s.cache[i].height;context.position=planes[i].position;
      context.sourceWidth=sourceWidths[i];context.sourceHeight=sourceHeights[i];
      context.right=multiply(rotate({1,0,0},planes[i].rotation),sourceWidths[i]*planes[i].scaleX);context.down=multiply(rotate({0,1,0},planes[i].rotation),sourceHeights[i]*planes[i].scaleY);context.normal=rotate({0,0,-1},planes[i].rotation);
      context.cameraPosition=camera.position;context.uvCenterX=.5+double(drawLayers[i].cropLeft-drawLayers[i].cropRight)/(2*sourceWidths[i]);context.uvCenterY=.5+double(drawLayers[i].cropTop-drawLayers[i].cropBottom)/(2*sourceHeights[i]);
      context.uvSpanX=double(widths[i])/sourceWidths[i];context.uvSpanY=double(heights[i])/sourceHeights[i];
      if(motion.valid&&center.visible&&motion.previous.visible){const auto uv=motionUvFromScreen(planes[i].position,context.right,context.down,camera,m.width,m.height,center.x-motion.previous.x,center.y-motion.previous.y);context.motionX=uv.x;context.motionY=uv.y;}
      context.cropLeft=double(drawLayers[i].cropLeft)/sourceWidths[i];context.cropRight=double(drawLayers[i].cropRight)/sourceWidths[i];
      context.cropTop=double(drawLayers[i].cropTop)/sourceHeights[i];context.cropBottom=double(drawLayers[i].cropBottom)/sourceHeights[i];
      const auto result=s.fx->process(drawLayers[i],raw[i],context,raw);
      textures[i]=result.texture;casterTextures[i]=result.caster;paddingX[i]=result.paddingX;paddingY[i]=result.paddingY;
    }
    // Apply all receiver masks after FX capture; referenced source inputs stay raw.
    // Every mask multiplies premultiplied RGB and alpha, with multiple masks intersecting.
    for(size_t n=0;n<count;++n){const auto i=order[n];const auto&l=drawLayers[i];
        presets::Receiver receiver;receiver.instanceId=l.instanceId;receiver.plane=planes[i];receiver.sourceWidth=sourceWidths[i]+2*paddingX[i];receiver.sourceHeight=sourceHeights[i]+2*paddingY[i];
        receiver.width=widths[i];receiver.height=heights[i];receiver.left=l.cropLeft+paddingX[i];receiver.top=l.cropTop+paddingY[i];
        const bool separateCaster=casterTextures[i]&&casterTextures[i]!=textures[i];
        textures[i]=s.graphics->mask(i,textures[i]?textures[i]:raw[i],receiver,evaluated.geometries,camera,m.width,m.height);
        if(separateCaster){
            receiver.sourceWidth=sourceWidths[i];receiver.sourceHeight=sourceHeights[i];receiver.left=l.cropLeft;receiver.top=l.cropTop;
            casterTextures[i]=s.graphics->mask(i+32,casterTextures[i],receiver,evaluated.geometries,camera,m.width,m.height);
        }else casterTextures[i]=textures[i];
    }
    std::sort(order.begin(),order.begin()+count,[&](size_t a,size_t b){
        const double da=inverseRotate(sub(planes[a].position,camera.position),camera.rotation).z,db=inverseRotate(sub(planes[b].position,camera.position),camera.rotation).z;
        if(da==db)return m.layers[a].depthOrder<m.layers[b].depthOrder;return da>db;
    });
    std::array<Light,4> lights{};size_t lightCount=0;
    auto resolveLight=[&](Light&light){if(light.bindUuid.empty())return true;for(size_t i=0;i<m.layers.size();++i)if(lightMatchesLayer(light,m.layers[i])){light.position=bindPosition(light.position,planes[i]);return true;}return false;};
    auto addLight=[&](Light light){
        if(!m.fx.enabled||!light.enabled)return;
        if(!resolveLight(light))return;
        lights[lightCount++]=std::move(light);
    };
    addLight(m.light);for(const auto&light:m.extraLights)if(lightCount<4)addLight(light);
    const double ambient=lightCount?0:1;
    double ambientSum=0;for(size_t i=0;i<lightCount;++i)ambientSum+=lights[i].ambient;
    const double ambientScale=ambientSum>2?2/ambientSum:1; // Preserve the previous total-fill ceiling.
    // Initialize all effect uniforms, including inactive branches. No stale D3D constants.
    scalar(s.effect,"opacity",1);scalar(s.effect,"ambient",ambient);scalar(s.effect,"shadow_opacity",0);scalar(s.effect,"light_intensity",0);scalar(s.effect,"light_range",1);scalar(s.effect,"light_falloff",2);scalar(s.effect,"soft_radius",0);scalar(s.effect,"light_extent",0);scalar(s.effect,"caster_opacity",1);scalar(s.effect,"rim_strength",0);scalar(s.effect,"rim_width",8);scalar(s.effect,"rim_softness",.65);scalar(s.effect,"dof_radius",0);
    scalar(s.effect,"exposure",m.fx.enabled?m.fx.exposure:0);scalar(s.effect,"saturation",m.fx.enabled?m.fx.saturation:1);scalar(s.effect,"vignette",m.fx.enabled?m.fx.vignette:0);
    vec4param(s.effect,"output_transform",1,1,1,0);scalar(s.effect,"widescreen_amount",0);
    vec3param(s.effect,"light_position",{});vec3param(s.effect,"light_color",{1,1,1});vec3param(s.effect,"shadow_color",{});planeParams(s,"plane_",{},1,1);planeParams(s,"caster_",{},1,1);
    vec4param(s.effect,"uv_rect",0,0,1,1);vec4param(s.effect,"caster_crop",0,0,1,1);vec4param(s.effect,"receiver_shadow_rect",0,0,1,1);vec4param(s.effect,"depths",1,1,1,1);
    struct vec2 texel;vec2_set(&texel,1,1);gs_effect_set_vec2(gs_effect_get_param_by_name(s.effect,"texel_size"),&texel);
    gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"shadow_map"),s.white);gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"caster_image"),s.white);
    gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"rim_image"),s.white);
    vec4param(s.effect,"rim_rect",0,0,1,1);gs_effect_set_vec2(gs_effect_get_param_by_name(s.effect,"rim_texel_size"),&texel);
    vec4param(s.effect,"rim_crop",0,0,1,1);
    std::array<Light,4> markers{};size_t markerCount=0,markerIndex=0;
    auto addMarker=[&](Light light){if(!m.fx.enabled||!light.visible)return;if(!resolveLight(light))return;markers[markerCount++]=std::move(light);};
    addMarker(m.light);for(const auto&light:m.extraLights)addMarker(light);
    auto depth=[&](Vec3 position){return inverseRotate(sub(position,camera.position),camera.rotation).z;};
    std::vector<size_t>geometryOrder;geometryOrder.reserve(evaluated.geometries.size());
    for(size_t i=0;i<evaluated.geometries.size();++i){const auto&g=evaluated.geometries[i];if(g.active&&g.geometry.enabled&&g.geometry.visible&&g.opacity>0&&g.geometry.mode!=presets::Mode::Mask)geometryOrder.push_back(i);}
    std::stable_sort(geometryOrder.begin(),geometryOrder.end(),[&](size_t a,size_t b){const auto&ga=evaluated.geometries[a];const auto&gb=evaluated.geometries[b];const auto da=depth(ga.plane.position),db=depth(gb.plane.position);return da==db?ga.geometry.depthOrder<gb.geometry.depthOrder:da>db;});
    size_t geometryIndex=0;
    auto drawGeometryBefore=[&](double cutoff,int orderAtDepth){while(geometryIndex<geometryOrder.size()){
        const auto&g=evaluated.geometries[geometryOrder[geometryIndex]];const auto d=depth(g.plane.position);
        if(d<cutoff||(d==cutoff&&g.geometry.depthOrder>orderAtDepth))break;
        s.graphics->draw(g,camera,m.width,m.height);++geometryIndex;
    }};
    std::sort(markers.begin(),markers.begin()+markerCount,[&](const Light&a,const Light&b){return depth(a.position)>depth(b.position);});
    auto drawMarker=[&](const Light&light){Plane p;p.position=light.position;p.rotation=camera.rotation;const double size=std::max(8.0,light.size);if(!geometry(s,corners(p,size,size),camera,m))return;vec3param(s.effect,"light_color",light.color);scalar(s.effect,"light_intensity",light.intensity);scalar(s.effect,"light_extent",light.type?1:0);gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);draw(s,s.white,3);};
    for(size_t n=0;n<count;++n){
        while(markerIndex<markerCount&&depth(markers[markerIndex].position)>depth(planes[order[n]].position)){drawGeometryBefore(depth(markers[markerIndex].position),0);drawMarker(markers[markerIndex++]);}
        const size_t i=order[n];const auto&l=drawLayers[i];const auto&p=planes[i];auto*texture=textures[i]?textures[i]:raw[i];
        drawGeometryBefore(depth(p.position),l.depthOrder);
        const auto quad=corners(p,widths[i]+2*paddingX[i],heights[i]+2*paddingY[i]);if(!geometry(s,quad,camera,m))continue;
        planeParams(s,"plane_",p,widths[i]+2*paddingX[i],heights[i]+2*paddingY[i]);
        vec4param(s.effect,"uv_rect",double(l.cropLeft)/(sourceWidths[i]+2*paddingX[i]),double(l.cropTop)/(sourceHeights[i]+2*paddingY[i]),
                  (widths[i]+2*paddingX[i])/(sourceWidths[i]+2*paddingX[i]),(heights[i]+2*paddingY[i])/(sourceHeights[i]+2*paddingY[i]));
        vec2_set(&texel,1.0f/gs_texture_get_width(texture),1.0f/gs_texture_get_height(texture));gs_effect_set_vec2(gs_effect_get_param_by_name(s.effect,"texel_size"),&texel);
        vec4param(s.effect,"receiver_shadow_rect",paddingX[i]/(widths[i]+2*paddingX[i]),paddingY[i]/(heights[i]+2*paddingY[i]),
                  widths[i]/(widths[i]+2*paddingX[i]),heights[i]/(heights[i]+2*paddingY[i]));
        const double distance=inverseRotate(sub(p.position,camera.position),camera.rotation).z;
        scalar(s.effect,"dof_radius",std::min(20.0,std::abs(distance-m.focusDistance)/std::max(distance,10.0)*(m.fx.enabled?m.aperture:0)*l.focusBlur));
        scalar(s.effect,"opacity",l.opacity);scalar(s.effect,"ambient",l.receiveLight?ambient:1);
        gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);draw(s,texture,0);
        if(!l.receiveLight)continue;
        for(size_t j=0;j<lightCount;++j){
            const auto&light=lights[j];vec3param(s.effect,"light_position",light.position);vec3param(s.effect,"light_color",light.color);vec3param(s.effect,"shadow_color",light.shadowColor);
            scalar(s.effect,"ambient",light.ambient*ambientScale);
            scalar(s.effect,"light_intensity",light.intensity);scalar(s.effect,"light_range",light.range);scalar(s.effect,"light_falloff",light.falloff);scalar(s.effect,"light_extent",light.type?light.size*.5:0);
            scalar(s.effect,"soft_radius",light.softness+(light.type?light.size*.5:light.size*.05));scalar(s.effect,"shadow_opacity",light.shadowOpacity);
            bool hasMask=false;
            if(l.receiveShadow&&light.shadows&&(light.receiverUuid.empty()||lightMatchesLayer(light,l,true))){
                // Clear once, then union each continuously blurred caster into this light mask.
                gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"shadow_map"),s.white);
                gs_texrender_reset(s.shadow);
                if(gs_texrender_begin(s.shadow,512,512)){
                    struct vec4 clear;vec4_zero(&clear);gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_texrender_end(s.shadow);hasMask=true;
                    for(size_t k=0;k<count;++k){const size_t c=order[k];const auto&caster=drawLayers[c];if(c==i||!caster.castShadow)continue;
                        const auto delta=sub(planes[c].position,p.position);const auto normal=rotate({0,0,-1},p.rotation);const auto dot=[](Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
                        if(dot(delta,normal)*dot(sub(light.position,p.position),normal)<=0)continue;
                        if(!shadowResources(s))continue;
                        const double radius=light.softness+(light.type?light.size*.5:light.size*.05);
                        const auto blur=shadowBlurRadius(p,planes[c].position,light.position,radius,widths[i],heights[i]);
                        // Padded receiver capture includes silhouettes just outside its boundary.
                        const double expansionX=1+2*blur[0]/512,expansionY=1+2*blur[1]/512;
                        gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"image"),s.white);
                        gs_texrender_reset(s.casterShadow);
                        if(!gs_texrender_begin(s.casterShadow,512,512)){++s.shadowFailures;continue;}
                        vec4_zero(&clear);gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,512,0,512,-100,100);flatGeometry(s,512,512);
                        planeParams(s,"plane_",p,widths[i]*expansionX,heights[i]*expansionY);
                        planeParams(s,"caster_",planes[c],widths[c],heights[c]);cropParam(s,"caster_crop",caster,sourceWidths[c],sourceHeights[c]);scalar(s.effect,"caster_opacity",caster.opacity);
                        gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"caster_image"),casterTextures[c]?casterTextures[c]:raw[c]);
                        gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);draw(s,s.white,1);gs_texrender_end(s.casterShadow);++s.shadowPasses;
                        auto*mask=s.shadowBlur->render(s.shadowKernel,gs_texrender_get_texture(s.casterShadow),512,512,blur[0]/expansionX,0,2,0,0,0,0,0,{},s.shadowAllocations,s.shadowPasses,blur[1]/expansionY);
                        if(!mask){++s.shadowFailures;continue;}
                        gs_texrender_reset(s.shadow);
                        if(gs_texrender_begin(s.shadow,512,512)){
                            gs_ortho(0,512,0,512,-100,100);flatGeometry(s,512,512);
                            vec4param(s.effect,"uv_rect",0,0,1/expansionX,1/expansionY);
                            gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);draw(s,mask,5);gs_texrender_end(s.shadow);++s.shadowPasses;
                        }else ++s.shadowFailures;
                    }
                    planeParams(s,"plane_",p,widths[i]+2*paddingX[i],heights[i]+2*paddingY[i]);
        vec4param(s.effect,"uv_rect",double(l.cropLeft)/(sourceWidths[i]+2*paddingX[i]),double(l.cropTop)/(sourceHeights[i]+2*paddingY[i]),
                  (widths[i]+2*paddingX[i])/(sourceWidths[i]+2*paddingX[i]),(heights[i]+2*paddingY[i])/(sourceHeights[i]+2*paddingY[i]));
                }
            }
            if(!geometry(s,quad,camera,m))continue;
            gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"shadow_map"),hasMask?gs_texrender_get_texture(s.shadow):s.white);
            if(!hasMask)scalar(s.effect,"shadow_opacity",0);
            scalar(s.effect,"rim_strength",l.rimLight?light.rim:0);scalar(s.effect,"rim_width",light.rimWidth);scalar(s.effect,"rim_softness",light.rimSoftness);
            // Reuse the masked pre-optics surface; exclude local FX shadow alpha.
            auto*rimSurface=casterTextures[i]?casterTextures[i]:raw[i];
            gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"rim_image"),rimSurface);
            cropParam(s,"rim_crop",l,sourceWidths[i],sourceHeights[i]);
            vec4param(s.effect,"rim_rect",paddingX[i]/(sourceWidths[i]+2*paddingX[i]),paddingY[i]/(sourceHeights[i]+2*paddingY[i]),
                      sourceWidths[i]/(sourceWidths[i]+2*paddingX[i]),sourceHeights[i]/(sourceHeights[i]+2*paddingY[i]));
            struct vec2 rimTexel;vec2_set(&rimTexel,1.0f/gs_texture_get_width(rimSurface),1.0f/gs_texture_get_height(rimSurface));
            gs_effect_set_vec2(gs_effect_get_param_by_name(s.effect,"rim_texel_size"),&rimTexel);
            gs_blend_function(GS_BLEND_ONE,GS_BLEND_ONE);draw(s,texture,2);
        }
    }
    while(markerIndex<markerCount){drawGeometryBefore(depth(markers[markerIndex].position),0);drawMarker(markers[markerIndex++]);}
    while(geometryIndex<geometryOrder.size())s.graphics->draw(evaluated.geometries[geometryOrder[geometryIndex++]],camera,m.width,m.height);
    gs_load_vertexbuffer(nullptr);gs_set_cull_mode(cull);gs_blend_state_pop();
    if(composite){
        gs_texrender_end(s.composite);gs_blend_state_push();gs_enable_blending(true);gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);flatGeometry(s,m.width,m.height);
        if(!widescreen.enabled){
            auto*vb=gs_vertexbuffer_get_data(s.vertices);for(size_t i=0;i<4;++i){vb->points[i].x=float(m.width*.5+(vb->points[i].x-m.width*.5)*evaluated.outputScaleX);vb->points[i].y=float(m.height*.5+(vb->points[i].y-m.height*.5)*evaluated.outputScaleY);}gs_vertexbuffer_flush(s.vertices);
        }
        vec4param(s.effect,"output_transform",widescreen.enabled?evaluated.outputScaleX:1,widescreen.enabled?evaluated.outputScaleY:1,widescreen.enabled?widescreen.visibleFraction:1,widescreen.transparent?1:0);
        scalar(s.effect,"widescreen_amount",widescreen.enabled?widescreen.amount:0);scalar(s.effect,"opacity",evaluated.outputOpacity);
        draw(s,gs_texrender_get_texture(s.composite),4);gs_load_vertexbuffer(nullptr);gs_blend_state_pop();
    }
    s.fxAllocations.store(s.fx->allocations()+s.graphics->allocations()+s.shadowAllocations);s.fxPasses.store(s.fx->passes()+s.graphics->passes()+s.shadowPasses);s.fxFailures.store(s.fx->failures()+s.graphics->failures()+s.shadowFailures);s.missing.store(missing);s.renders.fetch_add(1);s.renderNanoseconds.fetch_add(uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count()));
}

void destroy(void *data){
    auto *s=static_cast<Stage*>(data);auto *snapshot=s->published.exchange(nullptr);
    // OBS has detached this source before destroy; callbacks/readers have completed.
    obs_enter_graphics();s->fx.reset();s->graphics.reset();s->shadowBlur.reset();gs_effect_destroy(s->shadowKernel);gs_texrender_destroy(s->casterShadow);for(auto &cache:s->cache)gs_texrender_destroy(cache.texture);gs_vertexbuffer_destroy(s->vertices);gs_effect_destroy(s->effect);gs_texture_destroy(s->white);gs_texrender_destroy(s->shadow);gs_texrender_destroy(s->composite);obs_leave_graphics();
    delete snapshot;for(auto *old:s->retired)delete old;
    blog(LOG_INFO,"[SpatialFX] stage destroyed; renders=%llu cached_texture_allocations=%llu",(unsigned long long)s->renders.load(),(unsigned long long)s->textureAllocations.load());delete s;
}
void loaded(void *data,obs_data_t*){
    auto &s=*static_cast<Stage*>(data);auto *weak=obs_source_get_weak_source(s.owner);
    obs_queue_task(OBS_TASK_UI,[](void *opaque){auto *weak=static_cast<obs_weak_source_t*>(opaque);auto *source=obs_weak_source_get_source(weak);
        if(source&&!obs_source_removed(source)){
            auto *privateSettings=obs_source_get_private_settings(source);auto *saved=obs_data_get_obj(privateSettings,"spatial_fx_model");
            auto *upgraded=saved?modelData(readModel(saved)):stageGetSettings(source);stageApplySettings(source,upgraded);
            obs_data_release(upgraded);obs_data_release(saved);obs_data_release(privateSettings);
            blog(LOG_INFO,"[SpatialFX] private schema5 scene model restored");
        }obs_source_release(source);obs_weak_source_release(weak);
    },weak,false);
}
void *create(obs_data_t *settings,obs_source_t *source){
    auto *s=new Stage;
    // Load source text during creation; video_render only compiles cached text.
    char*path=obs_module_file("effects/fx_blur.body");char*code=path?os_quick_read_utf8_file(path):nullptr;
    if(code)s->shadowKernelSource=code;bfree(code);bfree(path);
    s->owner=source;applySettings(s,settings);auto *privateSettings=obs_source_get_private_settings(source);auto *copy=modelData(readModel(settings));obs_data_set_obj(privateSettings,"spatial_fx_model",copy);obs_data_release(copy);obs_data_release(privateSettings);
    auto *handler=obs_source_get_proc_handler(source);proc_handler_add(handler,"void spatial_fx_get_metrics(out int fx_allocations, out int fx_passes, out int fx_failures, out int renders, out float render_ms)",procMetrics,s);proc_handler_add(handler,"void spatial_fx_set_state(in ptr settings)",procSetState,s);proc_handler_add(handler,"void spatial_fx_get_state(out ptr settings)",procGetState,s);return s;
}
}
obs_data_t *stageGetSettings(obs_source_t *source){
    if(!source||obs_source_removed(source))return obs_data_create();auto *stage=static_cast<Stage*>(obs_obj_get_data(source));if(!stage)return obs_data_create();
    Read read(*stage);if(!read.value)return obs_data_create();auto m=read.value->model;m.timelinePosition=stage->clockPosition.load();m.playing=stage->clockPlaying.load();m.timelinePreview=stage->clockPreview.load();m.playToken=stage->clockToken.load();return modelData(m);
}
void stageApplyClock(obs_source_t*source,double time,bool playing,bool preview,uint64_t token){
    if(!source||obs_source_removed(source)||std::string(obs_source_get_unversioned_id(source))!=stageId)return;auto*s=static_cast<Stage*>(obs_obj_get_data(source));if(!s)return;
    s->clockPosition.store(time);s->clockPlaying.store(playing);s->clockPreview.store(preview);s->clockToken.store(token);
    auto*privateData=obs_source_get_private_settings(source);auto*d=obs_data_get_obj(privateData,"spatial_fx_model");if(d){obs_data_set_double(d,"timeline_position",time);obs_data_set_string(d,"timeline_position_us",std::to_string(toTime(time)).c_str());obs_data_set_bool(d,"timeline_preview",preview);obs_data_set_bool(d,"playing",playing);obs_data_set_int(d,"play_token",int64_t(token));}obs_data_release(d);obs_data_release(privateData);
}
void stageApplySettings(obs_source_t *source,obs_data_t *settings){
    if(!source||obs_source_removed(source)||std::string(obs_source_get_unversioned_id(source))!=stageId)return;
    auto *stage=static_cast<Stage*>(obs_obj_get_data(source));if(!stage)return;
    // Native edits never mutate public context.settings. OBS/WebSocket can traverse
    // that object on worker/video threads without taking the plugin's UI lock.
    // The private model is saved by OBS frontend on UI, then rendered through RCU.
    auto *privateSettings=obs_source_get_private_settings(source);auto *copy=obs_data_create();obs_data_apply(copy,settings);
    obs_data_set_obj(privateSettings,"spatial_fx_model",copy);applySettings(stage,copy);obs_data_release(copy);obs_data_release(privateSettings);
}
void stagePreviewSettings(obs_source_t *source,obs_data_t *settings){
    if(!source||!settings||obs_source_removed(source)||std::string(obs_source_get_unversioned_id(source))!=stageId)return;
    auto*stage=static_cast<Stage*>(obs_obj_get_data(source));if(stage)applySettings(stage,settings);
}
StageStats stageStats(obs_source_t *source){
    if(!source||std::string(obs_source_get_unversioned_id(source))!=stageId)return {};
    auto *s=static_cast<Stage*>(obs_obj_get_data(source));if(!s)return {};
    return {s->renders.load(),s->renderNanoseconds.load(),s->textureAllocations.load(),s->missing.load(),s->timeline.load(),s->ready.load(),s->fxAllocations.load(),s->fxPasses.load(),s->fxFailures.load(),s->effectTime.load()};
}
}
void register_spatial_stage(){
    obs_source_info info{};info.id=spatial::stageId;info.type=OBS_SOURCE_TYPE_INPUT;info.output_flags=OBS_SOURCE_VIDEO|OBS_SOURCE_CUSTOM_DRAW|OBS_SOURCE_CAP_DONT_SHOW_PROPERTIES;
    info.get_name=[](void*){return "Spatial FX Stage · 空间舞台";};info.create=spatial::create;info.destroy=spatial::destroy;info.update=spatial::update;info.load=spatial::loaded;
    info.get_width=[](void *d){return static_cast<spatial::Stage*>(d)->width.load();};info.get_height=[](void *d){return static_cast<spatial::Stage*>(d)->height.load();};info.video_tick=spatial::tick;info.video_render=spatial::render;
    info.enum_active_sources=[](void*d,obs_source_enum_proc_t c,void*p){spatial::enumerate(d,c,p,false);};info.enum_all_sources=[](void*d,obs_source_enum_proc_t c,void*p){spatial::enumerate(d,c,p,true);};
    info.get_properties=[](void*){auto*p=obs_properties_create();obs_properties_add_text(p,"help","请在 OBS Spatial FX 停靠面板管理图层、摄像机、灯光和预设。",OBS_TEXT_INFO);obs_properties_add_int(p,"width","舞台宽度",16,8192,16);obs_properties_add_int(p,"height","舞台高度",16,8192,16);return p;};
    info.get_defaults=[](obs_data_t*d){obs_data_set_default_int(d,"width",1920);obs_data_set_default_int(d,"height",1080);};
    obs_register_source(&info);
}




