#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <vector>
namespace spatial {
using TimeUs=int64_t;
inline TimeUs toTime(double s){return !std::isfinite(s)||s<=0?0:TimeUs(std::llround(std::min(s,double(INT64_MAX/1000000))*1000000));}
inline double toSeconds(TimeUs t){return double(t)/1000000;}
enum class Interpolation {Linear,LegacySmooth,LegacyCubic,EaseIn,EaseOut,EaseBoth,Hold,Bezier};
struct AnimKey {
 TimeUs time=0;double value=0;Interpolation interpolation=Interpolation::Linear;
 double outX=1.0/3,outY=1.0/3,inX=2.0/3,inY=2.0/3;bool outAbsolute=false,inAbsolute=false;double legacyFrom=0,legacyTo=1;
};
class Curve {
 std::shared_ptr<std::vector<AnimKey>> storage=std::make_shared<std::vector<AnimKey>>();
 std::vector<AnimKey>& writable(){if(storage.use_count()!=1)storage=std::make_shared<std::vector<AnimKey>>(*storage);return *storage;}
public:
 bool enabled=false,locked=false,shortest=false;
 const std::vector<AnimKey>& keys()const{return *storage;}
 void clear(){storage=std::make_shared<std::vector<AnimKey>>();enabled=false;}
 static double cubic(double p,double q,double t){double u=1-t;return 3*u*u*t*p+3*u*t*t*q+t*t*t;}
 static double root(double x,double a,double b){double lo=0,hi=1;for(int i=0;i<45;++i){double m=(lo+hi)*.5;if(cubic(a,b,m)<x)lo=m;else hi=m;}return (lo+hi)*.5;}
 void put(AnimKey k){if(!std::isfinite(k.value)||k.time<0)return;k.outX=std::clamp(k.outX,0.0,1.0);k.inX=std::clamp(k.inX,0.0,1.0);auto&v=writable();auto p=std::lower_bound(v.begin(),v.end(),k.time,[](auto&a,auto t){return a.time<t;});if(p!=v.end()&&p->time==k.time)*p=k;else v.insert(p,k);}
 bool erase(TimeUs t){auto&v=writable();auto p=std::lower_bound(v.begin(),v.end(),t,[](auto&a,auto x){return a.time<x;});if(p==v.end()||p->time!=t)return false;v.erase(p);return true;}
 bool contains(TimeUs t)const{auto&v=keys();auto p=std::lower_bound(v.begin(),v.end(),t,[](auto&a,auto x){return a.time<x;});return p!=v.end()&&p->time==t;}
 double value(TimeUs t,double base)const{
  const auto&v=keys();if(!enabled||v.empty())return base;if(t<=v.front().time)return v.front().value;if(t>=v.back().time)return v.back().value;
  auto p=std::upper_bound(v.begin(),v.end(),t,[](auto x,auto&a){return x<a.time;});const auto&a=*(p-1),&b=*p;
  double x=double(t-a.time)/double(b.time-a.time),y=x;
  switch(a.interpolation){case Interpolation::LegacySmooth:y=x*x*(3-2*x);break;case Interpolation::LegacyCubic: {auto f=[](double u){return u<.5?4*u*u*u:1-std::pow(-2*u+2,3)/2;};double lo=f(a.legacyFrom),hi=f(a.legacyTo);y=std::abs(hi-lo)>1e-15?(f(a.legacyFrom+x*(a.legacyTo-a.legacyFrom))-lo)/(hi-lo):x;}break;case Interpolation::EaseIn:y=x*x;break;case Interpolation::EaseOut:y=1-(1-x)*(1-x);break;case Interpolation::EaseBoth:y=x*x*(3-2*x);break;case Interpolation::Hold:y=0;break;case Interpolation::Bezier:{double u=root(x,a.outX,std::max(a.outX,b.inX)),inv=1-u;double end=shortest?a.value+std::remainder(b.value-a.value,360.0):b.value;double q1=a.outAbsolute?a.outY:a.value+(end-a.value)*a.outY,q2=b.inAbsolute?b.inY:a.value+(end-a.value)*b.inY;return inv*inv*inv*a.value+3*inv*inv*u*q1+3*inv*u*u*q2+u*u*u*end;}default:break;}
  const double delta=shortest?std::remainder(b.value-a.value,360.0):b.value-a.value;return a.value+delta*y;
 }
 // Exact subdivision: normalized handles are split with de Casteljau.
 void split(TimeUs t,double base){
  if(contains(t))return;const auto&v=keys();auto p=std::upper_bound(v.begin(),v.end(),t,[](auto x,auto&a){return x<a.time;});
  AnimKey n{t,value(t,base)};
  if(enabled&&p!=v.begin()&&p!=v.end()){
   AnimKey a=*(p-1),b=*p;n.interpolation=a.interpolation;
   if(a.interpolation!=Interpolation::Hold){
    double ox=a.outX,oy=a.outY,ix=b.inX,iy=b.inY;
    if(a.interpolation==Interpolation::Linear){ox=oy=1.0/3;ix=iy=2.0/3;}
    else if(a.interpolation==Interpolation::LegacySmooth||a.interpolation==Interpolation::EaseBoth){ox=1.0/3;oy=0;ix=2.0/3;iy=1;}
    else if(a.interpolation==Interpolation::EaseIn){ox=1.0/3;oy=0;ix=2.0/3;iy=1.0/3;}
    else if(a.interpolation==Interpolation::EaseOut){ox=1.0/3;oy=2.0/3;ix=2.0/3;iy=1;}
    else if(a.interpolation==Interpolation::LegacyCubic){double x=double(t-a.time)/double(b.time-a.time),middle=a.legacyFrom+x*(a.legacyTo-a.legacyFrom);n.legacyFrom=middle;n.legacyTo=a.legacyTo;a.legacyTo=middle;put(a);put(n);return;}
    double x=double(t-a.time)/double(b.time-a.time),u=root(x,ox,std::max(ox,ix));
    double av=a.value,bv=shortest?av+std::remainder(b.value-av,360.0):b.value;
    double q1=a.interpolation==Interpolation::Bezier&&a.outAbsolute?a.outY:av+(bv-av)*oy,q2=a.interpolation==Interpolation::Bezier&&b.inAbsolute?b.inY:av+(bv-av)*iy;
    auto mix=[&](double c,double d){return c+(d-c)*u;};
    double ax=mix(0,ox),ay=mix(av,q1),bx=mix(ox,ix),by=mix(q1,q2),cx=mix(ix,1),cy=mix(q2,bv);
    double dx=mix(ax,bx),dy=mix(ay,by),ex=mix(bx,cx),ey=mix(by,cy);
    a.interpolation=n.interpolation=Interpolation::Bezier;a.outX=ax/x;a.outY=ay;a.outAbsolute=true;n.inX=dx/x;n.inY=dy;n.inAbsolute=true;n.outX=(ex-x)/(1-x);n.outY=ey;n.outAbsolute=true;b.inX=(cx-x)/(1-x);b.inY=cy;b.inAbsolute=true;put(a);put(b);
   }
  }put(n);
 }
};
using Animation=std::map<std::string,Curve>;
// UI and evaluator share the same angular endpoint and absolute handle units.
inline double segmentEndValue(const AnimKey&a,const AnimKey&b,bool shortest){return shortest?a.value+std::remainder(b.value-a.value,360.0):b.value;}
inline void materializeHandles(AnimKey&a,AnimKey&b,bool shortest){
 if(a.interpolation!=Interpolation::Bezier){
  a.outX=1.0/3;b.inX=2.0/3;a.outY=1.0/3;b.inY=2.0/3;a.outAbsolute=b.inAbsolute=false;
  if(a.interpolation==Interpolation::EaseIn){a.outY=0;b.inY=1.0/3;}
  if(a.interpolation==Interpolation::EaseOut){a.outY=2.0/3;b.inY=1;}
  if(a.interpolation==Interpolation::EaseBoth||a.interpolation==Interpolation::LegacySmooth){a.outY=0;b.inY=1;}
 }
 const double delta=segmentEndValue(a,b,shortest)-a.value;
 if(!a.outAbsolute)a.outY=a.value+delta*a.outY;
 if(!b.inAbsolute)b.inY=a.value+delta*b.inY;
 a.outAbsolute=b.inAbsolute=true;
}
struct Orbit {bool enabled=false;double x=0,y=0,z=0,radius=1500,yaw=0,pitch=0;};
struct Parameter {const char*id;const char*label;double minimum,maximum,sliderMin,sliderMax;};
inline const std::vector<Parameter>& parameters(){static const std::vector<Parameter>p={
 {"x","位置 X",-100000,100000,-2000,2000},{"y","位置 Y",-100000,100000,-2000,2000},{"z","深度 Z",-100000,100000,-2000,2000},
 {"rx","俯仰 / 自转 X°",-100000,100000,-360,360},{"ry","偏航 / 自转 Y°",-100000,100000,-360,360},{"rz","翻滚 / 自转 Z°",-100000,100000,-360,360},
 {"sx","水平缩放 / 倍率",-100,100,-3,3},{"sy","垂直缩放 / 倍率",-100,100,-3,3},{"opacity","不透明度 / 0～1",0,1,0,1},{"fov","垂直视野 FOV°",10,140,10,140},
 {"tx","目标 X",-100000,100000,-2000,2000},{"ty","目标 Y",-100000,100000,-2000,2000},{"tz","目标 Z",-100000,100000,-2000,2000},
 {"radius","环绕半径",0,100000,0,3000},{"yaw","环绕水平角°",-100000,100000,-360,360},{"pitch","环绕俯仰角°",-100000,100000,-360,360}};return p;}
inline const Parameter* parameter(const std::string&id){for(auto&p:parameters())if(id==p.id)return &p;return nullptr;}
inline TimeUs lastTime(const Animation&a){TimeUs t=0;for(auto&v:a)if(!v.second.keys().empty())t=std::max(t,v.second.keys().back().time);return t;}
}
