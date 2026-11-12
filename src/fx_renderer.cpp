#include "fx_renderer.hpp"
#include "fx_shader.hpp"
#include "fx_lighting.hpp"
#include "fx_blur.hpp"
#include "fx_aa.hpp"
#include <obs-module.h>
#include <graphics/vec2.h>
#include <graphics/vec3.h>
#include <graphics/vec4.h>
#include <util/bmem.h>
#include <map>
#include <set>
#include <fstream>
#include <sstream>
#include <cmath>
#include <filesystem>
namespace spatial {
namespace {
std::string file(const std::string&name){char*path=obs_module_file(name.c_str());std::ifstream in(path?std::filesystem::u8path(path):std::filesystem::path(),std::ios::binary);std::ostringstream s;s<<in.rdbuf();bfree(path);return s.str();}
void scalar(gs_effect_t*e,const char*n,double v){if(auto*p=gs_effect_get_param_by_name(e,n))gs_effect_set_float(p,float(v));}
void two(gs_effect_t*e,const char*n,double a,double b){if(auto*p=gs_effect_get_param_by_name(e,n)){vec2 v;vec2_set(&v,float(a),float(b));gs_effect_set_vec2(p,&v);}}
void three(gs_effect_t*e,const char*n,Vec3 a){if(auto*p=gs_effect_get_param_by_name(e,n)){vec3 v;vec3_set(&v,float(a.x),float(a.y),float(a.z));gs_effect_set_vec3(p,&v);}}
void four(gs_effect_t*e,const char*n,double a,double b,double c,double d){if(auto*p=gs_effect_get_param_by_name(e,n)){vec4 v;vec4_set(&v,float(a),float(b),float(c),float(d));gs_effect_set_vec4(p,&v);}}
void texture(gs_effect_t*e,const char*n,gs_texture_t*t){if(auto*p=gs_effect_get_param_by_name(e,n))gs_effect_set_texture(p,t);}
struct Surface {
 gs_texrender_t*value=nullptr;Surface()=default;Surface(const Surface&)=delete;Surface&operator=(const Surface&)=delete;
 ~Surface(){release();}void release(){gs_texrender_destroy(value);value=nullptr;}bool begin(uint32_t w,uint32_t h,uint64_t&alloc){if(!value){value=gs_texrender_create(GS_RGBA16F,GS_ZS_NONE);++alloc;}gs_texrender_reset(value);if(!gs_texrender_begin(value,w,h))return false;vec4 clear;vec4_zero(&clear);gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,float(w),0,float(h),-100,100);return true;}
};
struct History {
 gs_texture_t*previous=nullptr,*next=nullptr;uint32_t width=0,height=0;uint64_t frame=0;bool valid=false,pending=false;double time=0,nextTime=0;
 ~History(){gs_texture_destroy(previous);gs_texture_destroy(next);}
 void prepare(uint32_t w,uint32_t h,uint64_t current,uint64_t&alloc){
  // History is copied from temporalRaw; formats must match for D3D CopyResource.
  if(w!=width||h!=height){gs_texture_destroy(previous);gs_texture_destroy(next);previous=gs_texture_create(w,h,GS_RGBA16F,1,nullptr,0);next=gs_texture_create(w,h,GS_RGBA16F,1,nullptr,0);alloc+=2;width=w;height=h;valid=false;pending=false;}
  if(frame!=current){if(pending){std::swap(previous,next);valid=true;time=nextTime;}frame=current;pending=false;}
 }
};
struct Asset {gs_texture_t*texture=nullptr;std::shared_ptr<const FxPixels>pixels;~Asset(){gs_texture_destroy(texture);}};
bool denseBlur(const FxDefinition&d){return d.group==1&&(d.shader==0||d.shader==1||d.shader==2||d.shader==4||d.shader==5);}
struct LayerResources {std::array<Surface,2>ping;Surface temporalRaw,glowExtract;std::unique_ptr<FxBlur>blur;std::unique_ptr<FxAA>aa;std::map<std::string,std::unique_ptr<History>>history;std::map<std::string,std::unique_ptr<Asset>>assets;};
}
struct FxRenderer::Impl {
 std::map<std::string,gs_effect_t*>effects;std::set<std::string>attempted,needed;std::map<std::string,std::unique_ptr<LayerResources>>layers;std::set<std::string>validIds;
 std::vector<std::string>instanceOrder;uint64_t allocated=0,draws=0,errors=0,frameStamp=UINT64_MAX,frameIndex=0;
 std::unique_ptr<SmaaTables>smaaTables;
 ~Impl(){for(auto&entry:effects)gs_effect_destroy(entry.second);}
 gs_effect_t*blurEffect(){
  const std::string key="dense-blur";auto&compiled=effects[key];if(attempted.count(key))return compiled;attempted.insert(key);
  auto source=file("effects/fx_blur.body");char*error=nullptr;compiled=gs_effect_create(source.c_str(),"spatial-dense-blur",&error);
  if(!compiled){++errors;blog(LOG_ERROR,"[SpatialFX 3.4] dense blur failed: %s",error?error:"missing shader");}bfree(error);return compiled;
 }
 gs_effect_t*effect(const FxDefinition&def){
  auto&compiled=effects[def.id];if(attempted.count(def.id))return compiled;attempted.insert(def.id);
  if(def.id=="FX02.2"){
   auto source=file("effects/fx_smaa.body");char*error=nullptr;compiled=gs_effect_create(source.c_str(),"spatial-smaa-1x",&error);
   if(!compiled){++errors;blog(LOG_ERROR,"[SpatialFX 3.4.2] SMAA shader failed: %s",error?error:"missing shader");}bfree(error);return compiled;
  }
  auto common=file("effects/fx_common.body"),body=file("effects/fx"+(def.group<10?std::string("0"):std::string())+std::to_string(def.group)+".body"),footer=file("effects/fx_footer.body");
  // Specialize before D3D compilation: unused sibling branches must disappear.
  char*error=nullptr;
  if(!body.empty()&&!footer.empty()&&specializeFxShader(common,body,def.shader)){
   // OBS compiles each declared technique. Keep only this child's render path.
   std::istringstream footerLines(footer);std::ostringstream selectedFooter;std::string footerLine;
   while(std::getline(footerLines,footerLine)){
    if(footerLine.rfind("technique ",0)==0){
     const bool keepTechnique=def.history?(footerLine.rfind("technique CaptureHistory ",0)==0||footerLine.rfind("technique MixHistory ",0)==0):footerLine.rfind("technique Draw ",0)==0;
     if(!keepTechnique)continue;
    }
    selectedFooter<<footerLine<<'\n';
   }
   footer=selectedFooter.str();
   blog(LOG_INFO,"[SpatialFX 3.4] compiling %s",def.id.c_str());
   compiled=gs_effect_create((common+"\n"+body+"\n"+footer).c_str(),("spatial-fx-"+def.id).c_str(),&error);
  }
  if(!compiled){++errors;blog(LOG_ERROR,"[SpatialFX 3.4] %s shader failed: %s",def.id.c_str(),error?error:"missing effect assets or selector");}
  else blog(LOG_INFO,"[SpatialFX 3.4] %s shader compiled on demand",def.id.c_str());bfree(error);return compiled;
 }
};
FxRenderer::FxRenderer():impl(std::make_unique<Impl>()){}FxRenderer::~FxRenderer()=default;
uint64_t FxRenderer::allocations()const{return impl->allocated;}uint64_t FxRenderer::passes()const{return impl->draws;}uint64_t FxRenderer::failures()const{return impl->errors;}
void FxRenderer::beginFrame(const std::vector<Layer>&layers){auto&s=*impl;s.validIds.clear();s.needed.clear();s.instanceOrder.clear();for(auto&l:layers){s.instanceOrder.push_back(l.instanceId);bool pixel=false;for(auto&d:fxCatalog())if(!d.geometry&&!d.stage&&activeFx(l.fx,d)){pixel=true;s.needed.insert(d.id);if(denseBlur(d)||d.id=="FX04.0")s.needed.insert("dense-blur");}if(pixel)s.validIds.insert(l.instanceId);}
 for(auto it=s.layers.begin();it!=s.layers.end();)if(!s.validIds.count(it->first))it=s.layers.erase(it);else++it;
 for(auto it=s.effects.begin();it!=s.effects.end();)if(!s.needed.count(it->first)){gs_effect_destroy(it->second);s.attempted.erase(it->first);it=s.effects.erase(it);}else++it;
 if(!s.needed.count("FX02.2"))s.smaaTables.reset();
}
gs_texture_t*FxRenderer::process(const Layer&layer,gs_texture_t*original,const FxRenderContext&ctx,const std::vector<gs_texture_t*>&references){
 auto&s=*impl;if(!original||!s.validIds.count(layer.instanceId))return original;if(s.frameStamp!=ctx.frame){s.frameStamp=ctx.frame;++s.frameIndex;}auto&slot=s.layers[layer.instanceId];if(!slot)slot=std::make_unique<LayerResources>();auto&res=*slot;auto*current=original;size_t ping=0;std::set<std::string>historyActive,assetsActive;bool blurActive=false,aaActive=false,glowActive=false;
 for(auto&def:fxCatalog()){
  auto*f=activeFx(layer.fx,def);if(!f||def.geometry||def.stage)continue;auto*e=s.effect(def);if(!e)continue;auto*auxiliary=ctx.auxiliary;bool auxValid=false;
  if(def.id=="FX02.2"){
   aaActive=true;if(!res.aa)res.aa=std::make_unique<FxAA>();if(!s.smaaTables)s.smaaTables=std::make_unique<SmaaTables>();
   auto*result=res.aa->render(e,*s.smaaTables,current,ctx.width,ctx.height,f->values[1],int(f->values[0]),f->strength,s.allocated,s.draws);
   if(result)current=result;else ++s.errors;continue;
  }
  if(!f->reference.empty()){for(size_t i=0;i<s.instanceOrder.size()&&i<references.size();++i)if(s.instanceOrder[i]==f->reference&&references[i]){auxiliary=references[i];auxValid=true;break;}}
  if(f->pixels){assetsActive.insert(f->id);auto&a=res.assets[f->id];if(!a)a=std::make_unique<Asset>();if(a->pixels!=f->pixels){gs_texture_destroy(a->texture);const uint8_t*p=f->pixels->pixels.data();a->texture=gs_texture_create(f->pixels->width,f->pixels->height,GS_RGBA,1,&p,0);++s.allocated;a->pixels=f->pixels;}if(a->texture){auxiliary=a->texture;auxValid=true;}}
  if(def.asset==FxAsset::Lut&&!auxValid)continue;
  History*history=nullptr;if(def.history){historyActive.insert(f->id);auto&h=res.history[f->id];if(!h)h=std::make_unique<History>();history=h.get();history->prepare(ctx.width,ctx.height,ctx.frame,s.allocated);}
  // OBS tracks texture dependencies before D3D removes sibling branches.
  // Bind a valid image even for children which do not use the dense blur path.
  texture(e,"blur_image",current);scalar(e,"blur_minimum_radius",1);
  if(def.id=="FX04.0"){
   auto*kernel=s.blurEffect();if(!kernel)continue;blurActive=true;glowActive=true;
   if(!res.blur)res.blur=std::make_unique<FxBlur>();
   // Extract before reduction: tiny highlights must survive area averaging.
   // Both modes share a dense Gaussian field instead of isolated disk taps.
   if(!res.glowExtract.begin(ctx.width,ctx.height,s.allocated)){++s.errors;continue;}
   texture(kernel,"image",current);scalar(kernel,"threshold",f->values[2]);
   gs_blend_state_push();gs_enable_blending(false);
   while(gs_effect_loop(kernel,"ExtractGlow"))gs_draw_sprite(current,0,ctx.width,ctx.height);
   gs_blend_state_pop();gs_texrender_end(res.glowExtract.value);++s.draws;
   auto*bright=gs_texrender_get_texture(res.glowExtract.value);
   auto*blurred=res.blur->render(kernel,bright,ctx.width,ctx.height,f->values[1],0,2,0,0,0,0,0,{},s.allocated,s.draws);
   if(!blurred){++s.errors;continue;}texture(e,"blur_image",blurred);
  }
  if(denseBlur(def)){
   auto*kernel=s.blurEffect();if(!kernel)continue;blurActive=true;if(!res.blur)res.blur=std::make_unique<FxBlur>();
   const int algorithm=def.shader==4?2:(def.shader==0?int(f->values[1]):0);
   const int edge=def.shader==4?int(f->values[5]):(def.shader==0?int(f->values[2]):0);
   const int fieldMode=def.shader==2?1:(def.shader==5?2:0);
   auto*blurred=res.blur->render(kernel,current,ctx.width,ctx.height,f->values[0],algorithm,edge,f->values[1],f->values[4],f->values[2],f->values[3],fieldMode,{f->values[1],f->values[2],f->values[3],f->values[4]},s.allocated,s.draws);
   if(!blurred){++s.errors;continue;}texture(e,"blur_image",blurred);
   scalar(e,"blur_minimum_radius",std::max(double(ctx.width)/gs_texture_get_width(blurred),double(ctx.height)/gs_texture_get_height(blurred)));
  }
  texture(e,"image",current);texture(e,"auxiliary",auxiliary?auxiliary:current);texture(e,"history_image",history&&history->valid?history->previous:current);texture(e,"effect_result",current);two(e,"pixel_size",1.0/ctx.width,1.0/ctx.height);two(e,"canvas_size",ctx.width,ctx.height);two(e,"motion_delta",ctx.motionX,ctx.motionY);scalar(e,"elapsed",ctx.elapsed);scalar(e,"frame_index",double(s.frameIndex%16777216));scalar(e,"history_valid",history&&history->valid?1:0);scalar(e,"history_time",history?history->time:ctx.elapsed);scalar(e,"frame_delta",history&&history->valid?std::max(0.0,ctx.elapsed-history->time):1.0/60);scalar(e,"auxiliary_valid",auxValid?1:0);scalar(e,"strength",f->strength);
  const char*names[]={"p0","p1","p2","p3","p4","p5","p6","p7"};for(size_t j=0;j<8;++j)four(e,names[j],f->values[j*4],f->values[j*4+1],f->values[j*4+2],f->values[j*4+3]);four(e,"tint",f->color.x,f->color.y,f->color.z,1);four(e,"tint2",f->color2.x,f->color2.y,f->color2.z,1);const auto materialLight=localFxLighting(ctx.position,multiply(ctx.right,ctx.uvSpanX),multiply(ctx.down,ctx.uvSpanY),ctx.normal);three(e,"lamp_position",materialLight.position);three(e,"lamp_color",materialLight.color);scalar(e,"lamp_intensity",materialLight.intensity);scalar(e,"lamp_range",materialLight.range);three(e,"layer_position",ctx.position);three(e,"camera_position",ctx.cameraPosition);three(e,"layer_right",ctx.right);three(e,"layer_down",ctx.down);three(e,"layer_normal",ctx.normal);two(e,"layer_uv_center",ctx.uvCenterX,ctx.uvCenterY);scalar(e,"lut_size",f->pixels?f->pixels->lutSize:0);three(e,"lut_min",f->pixels?f->pixels->minimum:Vec3{});three(e,"lut_max",f->pixels?f->pixels->maximum:Vec3{1,1,1});
  if(history){
   // Keep raw temporal state separate from the display ping targets and amount.
   if(!res.temporalRaw.begin(ctx.width,ctx.height,s.allocated)){++s.errors;continue;}
   gs_blend_state_push();gs_enable_blending(false);while(gs_effect_loop(e,"CaptureHistory"))gs_draw_sprite(current,0,ctx.width,ctx.height);gs_blend_state_pop();gs_texrender_end(res.temporalRaw.value);
   auto*rawResult=gs_texrender_get_texture(res.temporalRaw.value);if(!rawResult){++s.errors;continue;}++s.draws;
   if(history->next){gs_copy_texture(history->next,rawResult);history->pending=true;history->nextTime=ctx.elapsed;}
   // gs_technique_end clears every parameter. MixHistory is a new technique.
   texture(e,"image",current);scalar(e,"strength",f->strength);
   texture(e,"effect_result",rawResult);
  }
  if(!res.ping[ping].begin(ctx.width,ctx.height,s.allocated)){++s.errors;continue;}
  gs_blend_state_push();gs_enable_blending(false);while(gs_effect_loop(e,history?"MixHistory":"Draw"))gs_draw_sprite(current,0,ctx.width,ctx.height);gs_blend_state_pop();gs_texrender_end(res.ping[ping].value);auto*result=gs_texrender_get_texture(res.ping[ping].value);if(result){current=result;ping=1-ping;++s.draws;}
 }
 for(auto it=res.history.begin();it!=res.history.end();)if(!historyActive.count(it->first))it=res.history.erase(it);else++it;
 if(historyActive.empty())res.temporalRaw.release();
 if(!blurActive)res.blur.reset();
 if(!glowActive)res.glowExtract.release();
 if(!aaActive)res.aa.reset();
 for(auto it=res.assets.begin();it!=res.assets.end();)if(!assetsActive.count(it->first))it=res.assets.erase(it);else++it;
 return current;
}
}
