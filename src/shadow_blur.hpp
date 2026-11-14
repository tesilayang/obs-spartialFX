#pragma once
#include "core.hpp"
#include <algorithm>
#include <cmath>
namespace spatial {
// Receiver-local mask footprint, using the same world planes as ray projection.
inline std::array<double,2> shadowBlurRadius(const Plane&receiver,Vec3 caster,Vec3 light,double radius,double width,double height){
 const auto normal=rotate({0,0,1},receiver.rotation);
 const auto dot=[](Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
 const double gap=std::abs(dot(sub(caster,receiver.position),normal));
 const double emitter=std::abs(dot(sub(light,caster),normal));
 if(!std::isfinite(gap)||!std::isfinite(emitter)||!std::isfinite(radius)||radius<=0||emitter<.001)return {};
 const double spread=radius*gap/emitter;
 const double x=spread*512/std::max(.001,std::abs(width*receiver.scaleX));
 const double y=spread*512/std::max(.001,std::abs(height*receiver.scaleY));
 return {std::clamp(x,0.0,8192.0),std::clamp(y,0.0,8192.0)};
}
}
