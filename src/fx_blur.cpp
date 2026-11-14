#include "fx_blur.hpp"
#include <graphics/vec2.h>
#include <graphics/vec4.h>
#include <algorithm>
#include <cmath>
namespace spatial {
namespace {
void scalar(gs_effect_t*e,const char*n,double x){if(auto*p=gs_effect_get_param_by_name(e,n))gs_effect_set_float(p,float(x));}
void pair(gs_effect_t*e,const char*n,double x,double y){if(auto*p=gs_effect_get_param_by_name(e,n)){vec2 v;vec2_set(&v,float(x),float(y));gs_effect_set_vec2(p,&v);}}
template<class Target>gs_texture_t*pass(Target&target,gs_effect_t*e,const char*tech,gs_texture_t*input,
                 uint32_t w,uint32_t h,uint64_t&alloc,uint64_t&draws){
 if(!target.value)target.value=gs_texrender_create(GS_RGBA16F,GS_ZS_NONE);
 gs_texrender_reset(target.value);if(!gs_texrender_begin(target.value,w,h))return nullptr;
 // Count actual backing allocations, including a resize, not just wrappers.
 if(target.width!=w||target.height!=h){++alloc;target.width=w;target.height=h;}
 vec4 zero;vec4_zero(&zero);gs_clear(GS_CLEAR_COLOR,&zero,0,0);gs_ortho(0,float(w),0,float(h),-100,100);
 if(auto*p=gs_effect_get_param_by_name(e,"image"))gs_effect_set_texture(p,input);
 gs_blend_state_push();gs_enable_blending(false);
 while(gs_effect_loop(e,tech))gs_draw_sprite(input,0,w,h);
 gs_blend_state_pop();gs_texrender_end(target.value);++draws;return gs_texrender_get_texture(target.value);
}
}
FxBlur::~FxBlur(){for(auto&x:reduction)for(auto&t:x)gs_texrender_destroy(t.value);for(auto&x:axis)for(auto&y:x)for(auto&t:y)gs_texrender_destroy(t.value);}
gs_texture_t*FxBlur::render(gs_effect_t*e,gs_texture_t*input,uint32_t width,uint32_t height,
 double radius,int algorithm,int edge,double aperture,double rotation,double threshold,double boost,int fieldMode,const std::array<double,4>&field,
 uint64_t&alloc,uint64_t&draws,double radiusY){
 if(radiusY<0)radiusY=radius;
 if(std::max(radius,radiusY)<=.0001)return input;
 auto*current=input;uint32_t w=width,h=height;size_t levels=0,xLevel=0,yLevel=0;
 const double limit=algorithm==2?8.0:12.5;
 // Consecutive 2x area averages eliminate aliasing before reducing the kernel.
 // Fixed per-level targets avoid dimension changes and allocations every frame.
 while(levels<6){
  const bool reduceX=radius*double(w)/width>limit&&w>1;
  const bool reduceY=radiusY*double(h)/height>limit&&h>1;
  if(!reduceX&&!reduceY)break;
  const auto nw=reduceX?(w+1)/2:w,nh=reduceY?(h+1)/2:h;
  if(reduceX)++xLevel;if(reduceY)++yLevel;
  pair(e,"pixel_size",1.0/w,1.0/h);
  pair(e,"reduce_axis",reduceX?1:0,reduceY?1:0);
  current=pass(reduction[xLevel][yLevel],e,"Reduce",current,nw,nh,alloc,draws);
  if(!current)return nullptr;w=nw;h=nh;++levels;
 }
 // Each pass rebinds uniforms: OBS clears parameters at technique end.
 const int count=algorithm==2?1:2;
 for(int direction=0;direction<count;++direction){
  pair(e,"pixel_size",1.0/w,1.0/h);
  pair(e,"kernel_radius",radius*double(w)/width,radiusY*double(h)/height);
  pair(e,"axis",direction==0?1:0,direction==1?1:0);
  scalar(e,"algorithm",algorithm);scalar(e,"edge_mode",edge);scalar(e,"aperture",aperture);
  scalar(e,"rotation",rotation);scalar(e,"threshold",threshold);scalar(e,"boost",boost);
  scalar(e,"field_mode",fieldMode);pair(e,"canvas_size",width,height);
  if(auto*p=gs_effect_get_param_by_name(e,"field_params")){vec4 v;vec4_set(&v,float(field[0]),float(field[1]),float(field[2]),float(field[3]));gs_effect_set_vec4(p,&v);}
  current=pass(axis[xLevel][yLevel][direction],e,algorithm==2?"Aperture":(algorithm==3?"Dilate":"Convolve"),current,w,h,alloc,draws);
  if(!current)return nullptr;
 }
 return current;
}
}
