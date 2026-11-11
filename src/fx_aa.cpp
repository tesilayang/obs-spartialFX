#include "fx_aa.hpp"
#include "third_party/smaa_area.hpp"
#include "third_party/smaa_search.hpp"
#include <graphics/vec2.h>
#include <graphics/vec4.h>
namespace spatial {
namespace {
void image(gs_effect_t*e,const char*n,gs_texture_t*t){if(auto*p=gs_effect_get_param_by_name(e,n))gs_effect_set_texture(p,t);}
void scalar(gs_effect_t*e,const char*n,double v){if(auto*p=gs_effect_get_param_by_name(e,n))gs_effect_set_float(p,float(v));}
void pair(gs_effect_t*e,const char*n,double x,double y){if(auto*p=gs_effect_get_param_by_name(e,n)){vec2 v;vec2_set(&v,float(x),float(y));gs_effect_set_vec2(p,&v);}}
}
SmaaTables::~SmaaTables(){gs_texture_destroy(area);gs_texture_destroy(search);}
bool SmaaTables::prepare(uint64_t&alloc){
 if(!area){const uint8_t*p=areaTexBytes;area=gs_texture_create(AREATEX_WIDTH,AREATEX_HEIGHT,GS_R8G8,1,&p,0);if(area)++alloc;}
 if(!search){const uint8_t*p=searchTexBytes;search=gs_texture_create(SEARCHTEX_WIDTH,SEARCHTEX_HEIGHT,GS_R8,1,&p,0);if(search)++alloc;}
 return area&&search;
}
FxAA::~FxAA(){for(auto&t:targets)gs_texrender_destroy(t.value);}
gs_texture_t*FxAA::render(gs_effect_t*e,SmaaTables&tables,gs_texture_t*input,
 uint32_t width,uint32_t height,double threshold,int quality,double strength,uint64_t&alloc,uint64_t&draws){
 if(!e||!input||!width||!height||!tables.prepare(alloc))return nullptr;
 const char*techniques[]={"Edges","Weights","Resolve"};
 for(size_t i=0;i<targets.size();++i){auto&t=targets[i];
  if(!t.value)t.value=gs_texrender_create(i==0?GS_R8G8:GS_RGBA,GS_ZS_NONE);
  if(!t.value)return nullptr;gs_texrender_reset(t.value);if(!gs_texrender_begin(t.value,width,height))return nullptr;
  if(t.width!=width||t.height!=height){++alloc;t.width=width;t.height=height;}
  vec4 clear;vec4_zero(&clear);gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,float(width),0,float(height),-100,100);
  // Technique end clears bindings, so every pass binds all of its own inputs.
  image(e,"image",input);image(e,"edges_image",i?gs_texrender_get_texture(targets[0].value):input);
  image(e,"weights_image",i==2?gs_texrender_get_texture(targets[1].value):input);
  image(e,"area_image",tables.area);image(e,"search_image",tables.search);
  pair(e,"pixel_size",1.0/width,1.0/height);pair(e,"canvas_size",width,height);
  scalar(e,"threshold",threshold);scalar(e,"search_steps",quality?8:4);scalar(e,"strength",strength);
  gs_blend_state_push();gs_enable_blending(false);while(gs_effect_loop(e,techniques[i]))gs_draw_sprite(input,0,width,height);
  gs_blend_state_pop();gs_texrender_end(t.value);++draws;
 }
 return gs_texrender_get_texture(targets[2].value);
}
}
