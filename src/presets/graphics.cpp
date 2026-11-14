#include "graphics.hpp"
#include <obs-module.h>
#include <graphics/vec2.h>
#include <graphics/vec3.h>
#include <graphics/vec4.h>
#include <util/bmem.h>
#include <algorithm>
#include <cmath>

namespace spatial::presets {
namespace {
// Original analytic shape shader. Embedded so the installer has no new shader asset.
constexpr const char*code=R"EFFECT(
uniform float4x4 ViewProj;
uniform texture2d image;
uniform float4 depths;
uniform int pass_mode;
uniform int shape;
uniform int mask_space;
uniform float4 dimensions;
uniform float4 pattern;
uniform float4 tint;
uniform float2 draw_extent;
uniform float invert_mask;
uniform int reveal;
uniform float reveal_progress;
uniform float reveal_detail;
uniform float3 geometry_center;
uniform float3 geometry_right;
uniform float3 geometry_down;
uniform float3 geometry_normal;
uniform float3 receiver_center;
uniform float3 receiver_right;
uniform float3 receiver_down;
uniform float4 receiver_crop;
uniform float2 receiver_size;
uniform float3 camera_position;
uniform float3 camera_right;
uniform float3 camera_down;
uniform float3 camera_forward;
uniform float focal;
sampler_state linear_sampler { Filter=Linear; AddressU=Clamp; AddressV=Clamp; };
struct VertData { float4 pos:POSITION; float2 uv:TEXCOORD0; };
VertData VSDefault(VertData v){
 float depth=lerp(lerp(depths.x,depths.y,v.uv.x),lerp(depths.z,depths.w,v.uv.x),v.uv.y);
 v.pos=mul(float4(v.pos.xyz,1.0),ViewProj)*max(depth,.001);return v;
}
float boxDistance(float2 p,float2 halfsize){
 float2 q=abs(p)-halfsize;return length(max(q,0.0))+min(max(q.x,q.y),0.0);
}
float lineDistance(float value){return abs(frac(value/pattern.x+.5)-.5)*pattern.x-dimensions.z*.5;}
float hashCell(float2 p){return frac(sin(dot(p,float2(127.1,311.7))+reveal_detail)*43758.5453);}
float starDistance(float2 p){
 // Exact polygon edges alternate between outer and inner radii.
 float angle=atan2(p.y,p.x)+1.57079632679;
 angle=angle-floor(angle/6.28318530718)*6.28318530718;
 float sector=floor(angle/.628318530718);
 float a=sector*.628318530718-1.57079632679;
 float r0=fmod(sector,2.0)<.5?1.0:.38196601125;
 float r1=1.38196601125-r0;
 float2 start=float2(cos(a),sin(a))*r0;
 float2 finish=float2(cos(a+.628318530718),sin(a+.628318530718))*r1;
 float2 edge=finish-start;
 float2 ray=p/max(length(p),.000001);
 float crossRay=ray.x*edge.y-ray.y*edge.x;
 float radius=(start.x*edge.y-start.y*edge.x)/max(abs(crossRay),.000001)*sign(crossRay);
 return length(p)-radius;
}
float revealCoverage(float2 p){
 float2 uv=p/dimensions.xy+.5;
 float2 q=(uv-.5)*2.0;
 float progress=reveal_progress;
 if(progress<=.00001)return 0.0;
 if(progress>=.99999)return 1.0;
 float metric=uv.x;
 float feather=.003;
 if(reveal==2)feather=.09;
 if(reveal==3)metric=(uv.x+uv.y)*.5;
 if(reveal==4)metric=abs(q.x);
 if(reveal==5){float2 circle=q*dimensions.xy/min(dimensions.x,dimensions.y);metric=length(circle)/length(dimensions.xy/min(dimensions.x,dimensions.y));}
 if(reveal==6)metric=length(q)/1.41421356237;
 if(reveal==7)metric=max(abs(q.x),abs(q.y));
 if(reveal==8)metric=(abs(q.x)+abs(q.y))*.5;
 if(reveal==9){float a=atan2(q.y,q.x);float r=cos(3.14159265359/6.0)/cos(fmod(a+12.5663706144,1.0471975512)-.5235987756);metric=length(q)/max(.001,r)*.62;}
 if(reveal==10){float star=starDistance(q);metric=(length(q)/max(length(q)-star,.000001))*.7;}
 if(reveal==11)metric=frac(atan2(q.y,q.x)/6.28318530718+.25);
 if(reveal==12)metric=frac(uv.x*10.0);
 if(reveal==13)metric=frac(uv.x*10.0)*.3+floor(uv.x*10.0)/10.0*.7;
 if(reveal==14)metric=frac((uv.x+uv.y*.6)*10.0);
 if(reveal==15)metric=frac(uv.x*10.0)*.15+floor(uv.y*8.0)/8.0*.85;
 float2 cell=floor(uv*float2(12.0,8.0));
 float2 local=frac(uv*float2(12.0,8.0))-.5;
 if(reveal==16)metric=max(abs(local.x),abs(local.y))*1.35+frac((cell.x+cell.y)*.5)*.65;
 if(reveal==17)metric=hashCell(cell)*.8+max(abs(local.x),abs(local.y))*.4;
 if(reveal==18)metric=length((cell+.5)/float2(12.0,8.0)-.5)*1.15+max(abs(local.x),abs(local.y))*.35;
 if(reveal==19)metric=(cell.x/12.0+cell.y/8.0)*.4+max(abs(local.x),abs(local.y))*.4;
 if(reveal==20)metric=uv.x+.055*sin(uv.y*18.8495559215+reveal_detail);
 if(reveal==21)metric=hashCell(cell)*.35+length(local)*.92;
 if(reveal==22)metric=length(local)/.70710678118;
 return 1.0-smoothstep(progress-feather,progress+feather,metric);
}
float coverage(float2 p){
 float2 halfsize=dimensions.xy*.5;
 float d=boxDistance(p,halfsize);
 if(shape==1){p.x-=pattern.z*p.y/dimensions.y*dimensions.x;d=boxDistance(p,halfsize);}
 if(shape==2||shape==3){
  float ellipse=(length(p/max(halfsize,float2(.001,.001)))-1.0)*min(halfsize.x,halfsize.y);
  d=shape==3?abs(ellipse+dimensions.z*.5)-dimensions.z*.5:ellipse;
 }
 if(shape==4)d=max(d,lineDistance(p.x-pattern.z*p.y));
 if(shape==5)d=max(d,min(lineDistance(p.x),lineDistance(p.y)));
 if(shape==6){
  float2 cell=p/pattern.x;
  float parity=frac((floor(cell.x)+floor(cell.y))*.5)*2.0;
  float2 edge=min(frac(cell),1.0-frac(cell))*pattern.x;
  d=max(d,min(edge.x,edge.y)*(parity>.5?1.0:-1.0));
 }
 if(shape==7){
  float2 normalized=p/max(halfsize,float2(.001,.001));float pd=-10000.0;
  for(int side=0;side<64;side++){
   if(side>=int(pattern.y))break;
   float angle=6.28318530718*(float(side)+.5)/pattern.y;
   pd=max(pd,dot(normalized,float2(cos(angle),sin(angle)))-cos(3.14159265359/pattern.y));
  }
  d=pd*min(halfsize.x,halfsize.y);
 }
 if(shape==8){float2 n=p/max(halfsize,float2(.001,.001));d=starDistance(n)*min(halfsize.x,halfsize.y);}
 if(shape==9){float2 n=p/max(halfsize,float2(.001,.001));float a=atan2(n.y,n.x);d=(length(n)-(.78+.1*sin(a*3.0)+.07*cos(a*5.0)))*min(halfsize.x,halfsize.y);}
 if(shape==10)d=max(boxDistance(p,halfsize),abs(p.y-sin(p.x/max(dimensions.x,.001)*12.5663706144)*dimensions.y*.22)-dimensions.y*.15);
 if(shape==11){float2 cell=(frac(p/pattern.x+.5)-.5)*pattern.x;d=max(d,length(cell)-dimensions.z);}
 if(shape==12)d=max(d,lineDistance(p.x));
 if(shape==13){float angle=atan2(p.y,p.x);float spokes=abs(frac(angle/6.28318530718*16.0+.5)-.5);d=max(length(p)-min(halfsize.x,halfsize.y),max(min(halfsize.x,halfsize.y)*.62-length(p),(spokes-.085)*min(halfsize.x,halfsize.y)));}
 if(shape==14)d=min(boxDistance(p,float2(halfsize.x,dimensions.z*.5)),boxDistance(p,float2(dimensions.z*.5,halfsize.y)));
 // Analytic reveal softness belongs only to its moving frontier.
 float boundarySoftness=reveal>0?min(dimensions.w,1.0):dimensions.w;
 float result=saturate(.5-d/max(boundarySoftness,.001));
 if(shape==15)result*=saturate(1.0-abs(p.x/max(halfsize.x,.001)));
 if(reveal>0)result*=revealCoverage(p);
 return result;
}
float maskCoverage(float2 sourceUV){
 if(dot(geometry_right,geometry_right)<.000000000001||dot(geometry_down,geometry_down)<.000000000001)return invert_mask*tint.a;
 float2 uv=(sourceUV-receiver_crop.xy)/receiver_crop.zw;
 float3 world=receiver_center+(uv.x-.5)*receiver_right+(uv.y-.5)*receiver_down;
 float3 origin=camera_position;float3 ray=world-origin;
 if(mask_space==1){origin=float3((uv-.5)*receiver_size,-10000.0);ray=float3(0,0,1);}
 if(mask_space==2){
  float3 relative=world-camera_position;float depth=dot(relative,camera_forward);
  if(depth<=10.0)return invert_mask*tint.a;
  origin=float3(dot(relative,camera_right)*focal/depth,dot(relative,camera_down)*focal/depth,-10000.0);ray=float3(0,0,1);
 }
 float denominator=dot(geometry_normal,ray);
 if(abs(denominator)<.00001)return invert_mask*tint.a;
 float along=dot(geometry_normal,geometry_center-origin)/denominator;
 if(mask_space==0&&along<=0.0)return invert_mask*tint.a;
 float3 relative=origin+along*ray-geometry_center;
 float2 p=float2(dot(relative,geometry_right)/max(dot(geometry_right,geometry_right),.000001),dot(relative,geometry_down)/max(dot(geometry_down,geometry_down),.000001));
 float amount=coverage(p);return lerp(amount,1.0-amount,invert_mask)*tint.a;
}
float4 PSDefault(VertData v):TARGET{
 if(pass_mode==1)return image.Sample(linear_sampler,v.uv)*maskCoverage(v.uv);
 float alpha=coverage((v.uv-.5)*draw_extent)*tint.a;
 return float4(tint.rgb*alpha,alpha);
}
technique Draw { pass {vertex_shader=VSDefault(v);pixel_shader=PSDefault(v);} }
)EFFECT";
void scalar(gs_effect_t*e,const char*n,double x){gs_effect_set_float(gs_effect_get_param_by_name(e,n),float(x));}
void integer(gs_effect_t*e,const char*n,int x){gs_effect_set_int(gs_effect_get_param_by_name(e,n),x);}
void two(gs_effect_t*e,const char*n,double x,double y){vec2 v;vec2_set(&v,float(x),float(y));gs_effect_set_vec2(gs_effect_get_param_by_name(e,n),&v);}
void three(gs_effect_t*e,const char*n,Vec3 x){vec3 v;vec3_set(&v,float(x.x),float(x.y),float(x.z));gs_effect_set_vec3(gs_effect_get_param_by_name(e,n),&v);}
void four(gs_effect_t*e,const char*n,double x,double y,double z,double w){vec4 v;vec4_set(&v,float(x),float(y),float(z),float(w));gs_effect_set_vec4(gs_effect_get_param_by_name(e,n),&v);}
bool finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
bool valid(const EvaluatedGeometry&g){
 const auto&s=g.geometry;
 return g.active&&s.enabled&&s.visible&&std::isfinite(g.opacity)&&finite(g.plane.position)&&finite(g.plane.rotation)&&finite(s.color)&&
  std::isfinite(s.width)&&std::isfinite(s.height)&&s.width>0&&s.height>0&&std::isfinite(s.thickness)&&std::isfinite(s.softness)&&
  std::isfinite(s.spacing)&&std::isfinite(s.sides)&&std::isfinite(s.shear)&&std::isfinite(g.plane.scaleX)&&std::isfinite(g.plane.scaleY);
}
}
struct GraphicsRenderer::Impl {
 gs_effect_t*effect=nullptr;gs_vertbuffer_t*vertices=nullptr;gs_texture_t*white=nullptr;
 std::array<std::array<gs_texrender_t*,2>,64>surfaces{};
 bool attempted=false;uint64_t allocated=0,draws=0,errors=0;
 ~Impl(){for(auto&slot:surfaces)for(auto*surface:slot)gs_texrender_destroy(surface);gs_vertexbuffer_destroy(vertices);gs_effect_destroy(effect);gs_texture_destroy(white);}
 bool resources(){
  if(attempted)return effect&&vertices&&white;attempted=true;char*error=nullptr;
  effect=gs_effect_create(code,"spatial-v36-geometry",&error);
  if(!effect){++errors;blog(LOG_ERROR,"[SpatialFX 3.6] geometry shader failed: %s",error?error:"");}bfree(error);if(!effect)return false;
  auto*data=gs_vbdata_create();data->num=4;data->points=static_cast<vec3*>(bzalloc(sizeof(vec3)*4));
  data->num_tex=1;data->tvarray=static_cast<gs_tvertarray*>(bzalloc(sizeof(gs_tvertarray)));data->tvarray[0].width=2;data->tvarray[0].array=bzalloc(sizeof(vec2)*4);
  auto*uv=static_cast<vec2*>(data->tvarray[0].array);for(size_t i=0;i<4;++i)vec2_set(&uv[i],float(i%2),float(i/2));vertices=gs_vertexbuffer_create(data,GS_DYNAMIC);
  const uint8_t pixel[4]={255,255,255,255};const uint8_t*p=pixel;white=gs_texture_create(1,1,GS_RGBA,1,&p,0);
  ++allocated;if(!vertices||!white){++errors;return false;}return true;
 }
 void uniforms(const EvaluatedGeometry&g,const Camera&camera,uint32_t canvasHeight){
  const auto&shape=g.geometry;
  integer(effect,"pass_mode",0);integer(effect,"shape",int(shape.shape));integer(effect,"mask_space",0);
  integer(effect,"reveal",shape.reveal);scalar(effect,"reveal_progress",shape.progress);scalar(effect,"reveal_detail",shape.detail);
  four(effect,"depths",1,1,1,1);four(effect,"dimensions",shape.width,shape.height,std::max(.001,shape.thickness),std::max(0.0,shape.softness));
  four(effect,"pattern",std::max(.01,shape.spacing),std::clamp(std::round(shape.sides),3.0,64.0),shape.shear,0);
  four(effect,"tint",shape.color.x,shape.color.y,shape.color.z,std::clamp(g.opacity,0.0,1.0));two(effect,"draw_extent",shape.width,shape.height);scalar(effect,"invert_mask",shape.invert?1:0);
  three(effect,"geometry_center",g.plane.position);three(effect,"geometry_right",multiply(rotate({1,0,0},g.plane.rotation),g.plane.scaleX));three(effect,"geometry_down",multiply(rotate({0,1,0},g.plane.rotation),g.plane.scaleY));three(effect,"geometry_normal",rotate({0,0,1},g.plane.rotation));
  three(effect,"receiver_center",{});three(effect,"receiver_right",{1,0,0});three(effect,"receiver_down",{0,1,0});four(effect,"receiver_crop",0,0,1,1);two(effect,"receiver_size",1,1);
  three(effect,"camera_position",camera.position);three(effect,"camera_right",rotate({1,0,0},camera.rotation));three(effect,"camera_down",rotate({0,1,0},camera.rotation));three(effect,"camera_forward",rotate({0,0,1},camera.rotation));scalar(effect,"focal",canvasHeight/(2*std::tan(camera.fov*pi/360)));
  gs_effect_set_texture(gs_effect_get_param_by_name(effect,"image"),white);
 }
 void quad(const std::array<Vec3,4>&points){auto*data=gs_vertexbuffer_get_data(vertices);for(size_t i=0;i<4;++i)vec3_set(&data->points[i],float(points[i].x),float(points[i].y),0);gs_vertexbuffer_flush(vertices);gs_load_vertexbuffer(vertices);gs_load_indexbuffer(nullptr);}
 void render(){while(gs_effect_loop(effect,"Draw"))gs_draw(GS_TRISTRIP,0,4);++draws;}
};
GraphicsRenderer::GraphicsRenderer():impl(std::make_unique<Impl>()){}GraphicsRenderer::~GraphicsRenderer()=default;
uint64_t GraphicsRenderer::allocations()const{return impl->allocated;}uint64_t GraphicsRenderer::passes()const{return impl->draws;}uint64_t GraphicsRenderer::failures()const{return impl->errors;}
bool GraphicsRenderer::draw(const EvaluatedGeometry&g,const Camera&camera,uint32_t width,uint32_t height){
 if(!valid(g)||g.opacity<=0||g.geometry.mode==Mode::Mask||std::abs(g.plane.scaleX)<1e-7||std::abs(g.plane.scaleY)<1e-7)return false;
 auto&s=*impl;if(!s.resources())return false;
 const double w=g.geometry.width*(g.geometry.shape==Shape::Parallelogram?1+std::abs(g.geometry.shear):1),h=g.geometry.height;
 const auto world=corners(g.plane,w,h);std::array<Vec3,4>points{};std::array<double,4>depths{};
 for(size_t i=0;i<4;++i){if(g.geometry.screenFixed){points[i]={world[i].x+width*.5,world[i].y+height*.5,0};depths[i]=1;}else{const auto p=project(world[i],camera,width,height);if(!p.visible)return false;points[i]={p.x,p.y,0};depths[i]=p.depth;}}
 s.uniforms(g,camera,height);two(s.effect,"draw_extent",w,h);four(s.effect,"depths",depths[0],depths[1],depths[2],depths[3]);s.quad(points);
 gs_blend_function(GS_BLEND_ONE,GS_BLEND_INVSRCALPHA);s.render();return true;
}
gs_texture_t*GraphicsRenderer::mask(size_t slot,gs_texture_t*input,const Receiver&receiver,const std::vector<EvaluatedGeometry>&geometries,const Camera&camera,uint32_t,uint32_t height){
 if(!input||slot>=impl->surfaces.size())return input;
 auto&s=*impl;gs_texture_t*current=input;size_t ping=0;
 for(const auto&g:geometries){
  if(!valid(g)||g.geometry.mode==Mode::Decoration||std::find(g.geometry.receivers.begin(),g.geometry.receivers.end(),receiver.instanceId)==g.geometry.receivers.end())continue;
  if(!s.resources())return input;
  const auto w=gs_texture_get_width(input),h=gs_texture_get_height(input);auto&surface=s.surfaces[slot][ping];
  if(!surface){surface=gs_texrender_create(GS_RGBA16F,GS_ZS_NONE);++s.allocated;}
  if(!surface){++s.errors;return input;}gs_texrender_reset(surface);
  if(!gs_texrender_begin(surface,w,h)){++s.errors;return input;}
  vec4 clear;vec4_zero(&clear);gs_clear(GS_CLEAR_COLOR,&clear,0,0);gs_ortho(0,float(w),0,float(h),-100,100);
  s.uniforms(g,camera,height);integer(s.effect,"pass_mode",1);integer(s.effect,"mask_space",g.geometry.localMask?1:g.geometry.screenFixed?2:0);
  three(s.effect,"receiver_center",receiver.plane.position);three(s.effect,"receiver_right",multiply(rotate({1,0,0},receiver.plane.rotation),receiver.width*receiver.plane.scaleX));three(s.effect,"receiver_down",multiply(rotate({0,1,0},receiver.plane.rotation),receiver.height*receiver.plane.scaleY));
  four(s.effect,"receiver_crop",receiver.left/receiver.sourceWidth,receiver.top/receiver.sourceHeight,receiver.width/receiver.sourceWidth,receiver.height/receiver.sourceHeight);two(s.effect,"receiver_size",receiver.width,receiver.height);
  gs_effect_set_texture(gs_effect_get_param_by_name(s.effect,"image"),current);s.quad({Vec3{0,0,0},Vec3{double(w),0,0},Vec3{0,double(h),0},Vec3{double(w),double(h),0}});
  gs_blend_state_push();gs_enable_blending(false);s.render();gs_blend_state_pop();gs_texrender_end(surface);current=gs_texrender_get_texture(surface);ping=1-ping;
 }
 return current;
}
}
