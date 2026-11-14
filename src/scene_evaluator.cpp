#include "scene_evaluator.hpp"
#include "presets/presets.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace spatial {
#include "fx_geometry.inc"
Animation& objectAnimation(StageModel&m,int o){return o==-1?m.cameraAnimation:o==-2?m.compositeAnimation:o<=-3?m.geometries.at(size_t(-3-int64_t(o))).animation:m.layers.at(size_t(o)).animation;}
const Animation& objectAnimation(const StageModel&m,int o){return o==-1?m.cameraAnimation:o==-2?m.compositeAnimation:o<=-3?m.geometries.at(size_t(-3-int64_t(o))).animation:m.layers.at(size_t(o)).animation;}
bool objectLocked(const StageModel&m,int o){return o==-1?m.cameraLocked:o==-2?m.compositeLocked:o<=-3?m.geometries.at(size_t(-3-int64_t(o))).locked:m.layers.at(size_t(o)).locked;}
namespace {
double* address(StageModel&m,int o,const std::string&id){
 if(o<=-3){const auto i=size_t(-3-int64_t(o));if(i>=m.geometries.size())return nullptr;auto&g=m.geometries[i];if(id=="x")return &g.plane.position.x;if(id=="y")return &g.plane.position.y;if(id=="z")return &g.plane.position.z;if(id=="rx")return &g.plane.rotation.x;if(id=="ry")return &g.plane.rotation.y;if(id=="rz")return &g.plane.rotation.z;if(id=="sx")return &g.plane.scaleX;if(id=="sy")return &g.plane.scaleY;if(id=="opacity")return &g.opacity;return nullptr;}
 if(o==-2){if(id=="sx")return &m.outputScaleX;if(id=="sy")return &m.outputScaleY;if(id=="opacity")return &m.outputOpacity;return nullptr;}
 Vec3*p=o==-1?&m.camera.position:&m.layers.at(size_t(o)).plane.position;Vec3*r=o==-1?&m.camera.rotation:&m.layers.at(size_t(o)).plane.rotation;
 if(id=="x")return &p->x;if(id=="y")return &p->y;if(id=="z")return &p->z;if(id=="rx")return &r->x;if(id=="ry")return &r->y;if(id=="rz")return &r->z;
 if(o==-1&&id=="fov")return &m.camera.fov;
 if(o>=0){auto&l=m.layers[size_t(o)];if(id=="sx")return &l.plane.scaleX;if(id=="sy")return &l.plane.scaleY;if(id=="opacity")return &l.opacity;}
 auto&orbit=o==-1?m.cameraOrbit:m.layers[size_t(o)].orbit;
 if(id=="tx")return o==-1?&m.target.x:&orbit.x;if(id=="ty")return o==-1?&m.target.y:&orbit.y;if(id=="tz")return o==-1?&m.target.z:&orbit.z;
 if(id=="radius")return &orbit.radius;if(id=="yaw")return &orbit.yaw;if(id=="pitch")return &orbit.pitch;return nullptr;
}
double at(const StageModel&m,int o,const char*p,TimeUs t,bool motion){double b=baseValue(m,o,p);const auto&a=objectAnimation(m,o);auto i=a.find(p);double v=motion&&i!=a.end()?i->second.value(t,b):b;if(!motion||o==-2)return v;const auto&id=o==-1?std::string("camera"):m.layers[size_t(o)].instanceId;double offset=presets::motionOffset(m,id,p,t);return std::string(p)=="sx"||std::string(p)=="sy"||std::string(p)=="opacity"?v*(1+offset):v+offset;}
Vec3 position(const StageModel&m,int o,TimeUs t,bool motion){auto orbit=o==-1?m.cameraOrbit:m.layers[size_t(o)].orbit;if(orbit.enabled){Vec3 target{at(m,o,"tx",t,motion),at(m,o,"ty",t,motion),at(m,o,"tz",t,motion)};return add(target,rotate({0,0,-at(m,o,"radius",t,motion)},{at(m,o,"pitch",t,motion),at(m,o,"yaw",t,motion),0}));}return {at(m,o,"x",t,motion),at(m,o,"y",t,motion),at(m,o,"z",t,motion)};}
}
double baseValue(const StageModel&m,int o,const std::string&p){auto*v=address(const_cast<StageModel&>(m),o,p);return v?*v:0;}
void setBaseValue(StageModel&m,int o,const std::string&p,double v){auto*meta=parameter(p);if(!meta||!std::isfinite(v))return;auto*ptr=address(m,o,p);if(ptr)*ptr=std::clamp(v,meta->minimum,meta->maximum);}
TimeUs contentEnd(const StageModel&m){TimeUs t=std::max(lastTime(m.cameraAnimation),lastTime(m.compositeAnimation));for(auto&l:m.layers)t=std::max(t,lastTime(l.animation));for(const auto&g:m.geometries)t=std::max(t,lastTime(g.animation));for(const auto&b:m.motionBlocks)t=std::max(t,b.end);for(const auto&p:m.presetInstances)t=std::max(t,p.end);return t;}
Plane effectivePlane(const Layer&l){auto p=l.plane;const double d=1500+p.position.z;if(std::abs(d)>20){double f=(1500+p.position.z*l.perspective)/d;p.position.x*=f;p.position.y*=f;p.position.z*=l.perspective;p.scaleX*=f;p.scaleY*=f;}return p;}
EvaluatedScene evaluateSceneAt(const StageModel&m,TimeUs t,const std::vector<SourceSize>&sizes,bool motion,double fxSeconds){
 EvaluatedScene e;e.camera=m.camera;e.camera.position=position(m,-1,t,motion);e.camera.rotation={at(m,-1,"rx",t,motion),at(m,-1,"ry",t,motion),at(m,-1,"rz",t,motion)};e.camera.fov=std::clamp(at(m,-1,"fov",t,motion),10.0,140.0);
 if(m.lookAtTarget)e.camera=lookAt(e.camera,{at(m,-1,"tx",t,motion),at(m,-1,"ty",t,motion),at(m,-1,"tz",t,motion)});if(!m.preview)e.camera={};
 for(size_t i=0;i<m.layers.size();++i){const auto&l=m.layers[i];Plane p=l.plane;p.position=position(m,int(i),t,motion);p.rotation={at(m,int(i),"rx",t,motion),at(m,int(i),"ry",t,motion),at(m,int(i),"rz",t,motion)};p.scaleX=at(m,int(i),"sx",t,motion);p.scaleY=at(m,int(i),"sy",t,motion);double opacity=std::clamp(at(m,int(i),"opacity",t,motion),0.0,1.0);Layer fxLayer=l;fxLayer.plane=p;fxLayer.opacity=opacity;applyFxGeometry(fxLayer,fxSeconds<0?toSeconds(t):fxSeconds,e.camera);p=fxLayer.plane;opacity=fxLayer.opacity;e.authoring.push_back({p,opacity});
  if(i<sizes.size()&&sizes[i].width>0&&sizes[i].height>0){const auto w=sizes[i].width,h=sizes[i].height;if(l.followSourceSize&&l.importWidth>0&&l.importHeight>0){auto ref=sourceLayout(l.importWidth,l.importHeight,l.importTransform),cur=sourceLayout(w,h,l.importTransform);if(std::abs(ref.scaleX)>.000001&&std::abs(ref.scaleY)>.000001){p.position=add(p.position,rotate({(cur.x-ref.x)/ref.scaleX*p.scaleX,(cur.y-ref.y)/ref.scaleY*p.scaleY,0},p.rotation));p.scaleX*=cur.scaleX/ref.scaleX;p.scaleY*=cur.scaleY/ref.scaleY;}}
   else if(l.boundsType&&l.boundsWidth>0&&l.boundsHeight>0){double x=l.boundsWidth/std::max(1.0,w-l.cropLeft-l.cropRight),y=l.boundsHeight/std::max(1.0,h-l.cropTop-l.cropBottom);if(l.boundsType!=1){double f=std::min(x,y);if(l.boundsType==3)f=std::max(x,y);if(l.boundsType==4)f=x;if(l.boundsType==5)f=y;if(l.boundsType==6)f=std::min(1.0,f);x=y=f;}p.scaleX*=x;p.scaleY*=y;}}
  Layer adjusted=l;adjusted.plane=p;e.world.push_back({effectivePlane(adjusted),opacity});
 }
 std::vector<Plane>world;world.reserve(e.world.size());for(const auto&w:e.world)world.push_back(w.plane);e.geometries=presets::evaluateGeometry(m,t,world,motion);
 auto light=[&](const Light&l){Vec3 p=l.position;if(!l.bindUuid.empty())for(size_t i=0;i<m.layers.size();++i)if(lightMatchesLayer(l,m.layers[i])){p=bindPosition(p,e.world[i].plane);break;}e.lights.push_back(p);};light(m.light);for(auto&l:m.extraLights)light(l);
 e.outputScaleX=at(m,-2,"sx",t,motion);e.outputScaleY=at(m,-2,"sy",t,motion);e.outputOpacity=std::clamp(at(m,-2,"opacity",t,motion),0.0,1.0);return e;
}
Vec3 newLightPosition(const StageModel&m,TimeUs t,const std::vector<SourceSize>&sizes,bool motion,double fxSeconds){
 const auto e=evaluateSceneAt(m,t,sizes,motion,fxSeconds);
 double nearest=std::numeric_limits<double>::infinity(),left=nearest,top=nearest,right=-nearest,bottom=-nearest;
 for(size_t i=0;i<m.layers.size();++i){
  const auto&l=m.layers[i];if(!l.visible||!l.enabled||e.world[i].opacity<=0)continue;
  const double w=i<sizes.size()?sizes[i].width:l.importWidth,h=i<sizes.size()?sizes[i].height:l.importHeight;
  if(w<=0||h<=0||!std::isfinite(w)||!std::isfinite(h))continue;
  double width=std::max(1.0,w-l.cropLeft-l.cropRight),height=std::max(1.0,h-l.cropTop-l.cropBottom);
  if(l.followSourceSize&&l.importWidth>0&&l.importHeight>0){const auto layout=sourceLayout(w,h,l.importTransform);width=layout.width;height=layout.height;}
  auto quad=corners(e.world[i].plane,width,height);bool valid=true;
  for(auto&v:quad){v=inverseRotate(sub(v,e.camera.position),e.camera.rotation);if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||v.z<=10){valid=false;break;}}
  if(!valid)continue; // Match the render pipeline's near-plane rejection.
  for(const auto&v:quad){nearest=std::min(nearest,v.z);left=std::min(left,v.x/v.z);right=std::max(right,v.x/v.z);top=std::min(top,v.y/v.z);bottom=std::max(bottom,v.y/v.z);}
 }
 double depth=500,x=0,y=0;
 if(std::isfinite(nearest)){
  depth=nearest-std::min(std::max(50.0,nearest*.1),(nearest-10)*.5);
  const double halfY=std::tan(e.camera.fov*pi/360),halfX=halfY*double(m.width)/std::max(1u,m.height);
  x=std::clamp((left+right)*.5,-halfX*.8,halfX*.8);y=std::clamp((top+bottom)*.5,-halfY*.8,halfY*.8);
 }
 return add(e.camera.position,rotate({x*depth,y*depth,depth},e.camera.rotation));
}
}
