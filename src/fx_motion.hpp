#pragma once
#include "core.hpp"
#include <cmath>
namespace spatial {
inline Vec3 motionUvFromScreen(Vec3 center,Vec3 right,Vec3 down,const Camera&camera,double width,double height,double dx,double dy){
 constexpr double epsilon=.0001;
 const auto r0=project(sub(center,multiply(right,epsilon)),camera,width,height),r1=project(add(center,multiply(right,epsilon)),camera,width,height);
 const auto d0=project(sub(center,multiply(down,epsilon)),camera,width,height),d1=project(add(center,multiply(down,epsilon)),camera,width,height);
 if(!r0.visible||!r1.visible||!d0.visible||!d1.visible)return {};
 const double a=(r1.x-r0.x)/(2*epsilon),c=(r1.y-r0.y)/(2*epsilon),b=(d1.x-d0.x)/(2*epsilon),d=(d1.y-d0.y)/(2*epsilon),det=a*d-b*c;
 const double scale=std::hypot(a,c)*std::hypot(b,d);
 if(scale<1e-12||std::abs(det)<scale*1e-6)return {};
 Vec3 result{(d*dx-b*dy)/det,(a*dy-c*dx)/det,0};
 return std::isfinite(result.x)&&std::isfinite(result.y)?result:Vec3{};
}
}
