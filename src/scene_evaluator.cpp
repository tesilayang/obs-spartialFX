#include "scene_evaluator.hpp"
#include "timeline_object.hpp"
#include "presets/presets.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
namespace spatial {
#include "fx_geometry.inc"
namespace {
double at(const StageModel&m,int o,const char*p,TimeUs t,bool motion){double b=baseValue(m,o,p);const auto&a=objectAnimation(m,o);auto i=a.find(p);double v=motion&&i!=a.end()?i->second.value(t,b):b;if(!motion||o==-2)return v;const auto&id=o==-1?std::string("camera"):m.layers[size_t(o)].instanceId;double offset=presets::motionOffset(m,id,p,t);return std::string(p)=="sx"||std::string(p)=="sy"||std::string(p)=="opacity"?v*(1+offset):v+offset;}
Vec3 position(const StageModel&m,int o,TimeUs t,bool motion){auto orbit=o==-1?m.cameraOrbit:m.layers[size_t(o)].orbit;if(orbit.enabled){Vec3 target{at(m,o,"tx",t,motion),at(m,o,"ty",t,motion),at(m,o,"tz",t,motion)};return add(target,rotate({0,0,-at(m,o,"radius",t,motion)},{at(m,o,"pitch",t,motion),at(m,o,"yaw",t,motion),0}));}return {at(m,o,"x",t,motion),at(m,o,"y",t,motion),at(m,o,"z",t,motion)};}
}
TimeUs contentEnd(const StageModel&m){TimeUs t=0;for(const auto&o:timelineObjects(m))t=std::max(t,lastTime(objectAnimation(m,o.handle)));for(const auto&b:m.motionBlocks)if(b.enabled)t=std::max(t,b.end);for(const auto&g:m.geometries)if(g.enabled)t=std::max(t,g.revealStart+g.revealPeriod);return t;}
Plane effectivePlane(const Layer&l){auto p=l.plane;const double d=1500+p.position.z;if(std::abs(d)>20){double f=(1500+p.position.z*l.perspective)/d;p.position.x*=f;p.position.y*=f;p.position.z*=l.perspective;p.scaleX*=f;p.scaleY*=f;}return p;}
EvaluatedScene evaluateSceneAt(const StageModel&m,TimeUs t,const std::vector<SourceSize>&sizes,bool motion,double fxSeconds){
 EvaluatedScene e;e.camera=m.camera;e.camera.position=position(m,-1,t,motion);e.camera.rotation={at(m,-1,"rx",t,motion),at(m,-1,"ry",t,motion),at(m,-1,"rz",t,motion)};e.camera.fov=std::clamp(at(m,-1,"fov",t,motion),10.0,140.0);
 if(m.lookAtTarget)e.camera=lookAt(e.camera,{at(m,-1,"tx",t,motion),at(m,-1,"ty",t,motion),at(m,-1,"tz",t,motion)});if(!m.preview)e.camera={};
 for(size_t i=0;i<m.layers.size();++i){const auto&l=m.layers[i];Plane p=l.plane;p.position=position(m,int(i),t,motion);p.rotation={at(m,int(i),"rx",t,motion),at(m,int(i),"ry",t,motion),at(m,int(i),"rz",t,motion)};p.scaleX=at(m,int(i),"sx",t,motion);p.scaleY=at(m,int(i),"sy",t,motion);double opacity=std::clamp(at(m,int(i),"opacity",t,motion),0.0,1.0);Layer fxLayer=l;fxLayer.fx=evaluateLayerFx(m,i,t,motion);e.layerFx.push_back(fxLayer.fx);fxLayer.plane=p;fxLayer.opacity=opacity;applyFxGeometry(fxLayer,fxSeconds<0?toSeconds(t):fxSeconds,e.camera);p=fxLayer.plane;opacity=fxLayer.opacity;e.authoring.push_back({p,opacity});
  if(i<sizes.size()&&sizes[i].width>0&&sizes[i].height>0){const auto w=sizes[i].width,h=sizes[i].height;if(l.followSourceSize&&l.importWidth>0&&l.importHeight>0){auto ref=sourceLayout(l.importWidth,l.importHeight,l.importTransform),cur=sourceLayout(w,h,l.importTransform);if(std::abs(ref.scaleX)>.000001&&std::abs(ref.scaleY)>.000001){p.position=add(p.position,rotate({(cur.x-ref.x)/ref.scaleX*p.scaleX,(cur.y-ref.y)/ref.scaleY*p.scaleY,0},p.rotation));p.scaleX*=cur.scaleX/ref.scaleX;p.scaleY*=cur.scaleY/ref.scaleY;}}
   else if(l.boundsType&&l.boundsWidth>0&&l.boundsHeight>0){double x=l.boundsWidth/std::max(1.0,w-l.cropLeft-l.cropRight),y=l.boundsHeight/std::max(1.0,h-l.cropTop-l.cropBottom);if(l.boundsType!=1){double f=std::min(x,y);if(l.boundsType==3)f=std::max(x,y);if(l.boundsType==4)f=x;if(l.boundsType==5)f=y;if(l.boundsType==6)f=std::min(1.0,f);x=y=f;}p.scaleX*=x;p.scaleY*=y;}}
  Layer adjusted=l;adjusted.plane=p;e.world.push_back({effectivePlane(adjusted),opacity});
 }
 std::vector<Plane>world;world.reserve(e.world.size());for(const auto&w:e.world)world.push_back(w.plane);e.geometries=presets::evaluateGeometry(m,t,world,motion);
 auto light=[&](int h){auto l=evaluateLight(m,h,t,motion);e.lightStates.push_back(l);Vec3 p=l.position;if(!l.bindUuid.empty())for(size_t i=0;i<m.layers.size();++i)if(lightMatchesLayer(l,m.layers[i])){p=bindPosition(p,e.world[i].plane);break;}e.lights.push_back(p);};light(lightHandleBase);for(size_t i=0;i<m.extraLights.size();++i)light(lightHandleBase-1-int(i));
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
