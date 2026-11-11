#include "core.hpp"
#include <algorithm>
#include <cmath>
namespace spatial {
Vec3 add(Vec3 a,Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 sub(Vec3 a,Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 multiply(Vec3 a,double f) { return {a.x*f,a.y*f,a.z*f}; }
namespace {
Vec3 rx(Vec3 p,double a){const double c=std::cos(a),s=std::sin(a);return {p.x,c*p.y-s*p.z,s*p.y+c*p.z};}
Vec3 ry(Vec3 p,double a){const double c=std::cos(a),s=std::sin(a);return {c*p.x+s*p.z,p.y,-s*p.x+c*p.z};}
Vec3 rz(Vec3 p,double a){const double c=std::cos(a),s=std::sin(a);return {c*p.x-s*p.y,s*p.x+c*p.y,p.z};}
bool finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
double lerp(double a,double b,double t){return a+(b-a)*t;}
double angle(double a,double b,double t){return a+std::remainder(b-a,360.0)*t;}
}
Vec3 rotate(Vec3 p,Vec3 degrees){return rz(ry(rx(p,degrees.x*pi/180),degrees.y*pi/180),degrees.z*pi/180);}
Vec3 inverseRotate(Vec3 p,Vec3 degrees){return rx(ry(rz(p,-degrees.z*pi/180),-degrees.y*pi/180),-degrees.x*pi/180);}
Projected project(Vec3 world,const Camera &camera,double width,double height) {
    if(!finite(world)||!finite(camera.position)||!finite(camera.rotation)||!std::isfinite(camera.fov)||camera.fov<1||camera.fov>179||width<=0||height<=0)return {};
    const Vec3 view=inverseRotate(sub(world,camera.position),camera.rotation);
    if(!finite(view)||view.z<=10)return {};
    const double focal=height/(2*std::tan(camera.fov*pi/360));
    const double x=width*.5+view.x*focal/view.z,y=height*.5+view.y*focal/view.z;
    if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>1e8||std::abs(y)>1e8)return {};
    return {x,y,view.z,true};
}
std::array<Vec3,4> corners(const Plane &plane,double width,double height) {
    const double x=width*plane.scaleX*.5,y=height*plane.scaleY*.5;
    return {add(plane.position,rotate({-x,-y,0},plane.rotation)),add(plane.position,rotate({x,-y,0},plane.rotation)),add(plane.position,rotate({-x,y,0},plane.rotation)),add(plane.position,rotate({x,y,0},plane.rotation))};
}
double ease(double t,Easing mode) {
    t=std::clamp(t,0.0,1.0);
    if(mode==Easing::Smooth)return t*t*(3-2*t);
    if(mode==Easing::CubicInOut)return t<.5?4*t*t*t:1-std::pow(-2*t+2,3)/2;
    return t;
}
Camera sample(const std::vector<Keyframe> &keys,double time,bool loop) {
    if(keys.empty())return {};
    if(keys.size()==1)return keys.front().camera;
    const double start=keys.front().time,end=keys.back().time,duration=end-start;
    if(loop&&duration>0&&time>=start)time=start+std::fmod(time-start,duration);
    if(time<=start)return keys.front().camera;
    if(time>=end)return keys.back().camera;
    const auto right=std::upper_bound(keys.begin(),keys.end(),time,[](double t,const Keyframe &k){return t<k.time;});
    if(right==keys.begin()||right==keys.end())return keys.back().camera;
    const auto &a=*(right-1),&b=*right;
    const double span=b.time-a.time,t=span>0?ease((time-a.time)/span,a.easing):1;
    Camera result;
    result.position={lerp(a.camera.position.x,b.camera.position.x,t),lerp(a.camera.position.y,b.camera.position.y,t),lerp(a.camera.position.z,b.camera.position.z,t)};
    result.rotation={angle(a.camera.rotation.x,b.camera.rotation.x,t),angle(a.camera.rotation.y,b.camera.rotation.y,t),angle(a.camera.rotation.z,b.camera.rotation.z,t)};
    result.fov=lerp(a.camera.fov,b.camera.fov,t);return result;
}
bool shadowPoint(Vec3 point,Vec3 light,double receiverZ,Vec3 &out) {
    const double dz=point.z-light.z;
    if(!finite(point)||!finite(light)||!std::isfinite(receiverZ)||std::abs(dz)<1e-6)return false;
    const double t=(receiverZ-light.z)/dz;
    if(t<1||!std::isfinite(t))return false;
    out=add(light,multiply(sub(point,light),t));return finite(out);
}
Camera lookAt(Camera camera,Vec3 target){
    const auto delta=sub(target,camera.position);
    const double horizontal=std::hypot(delta.x,delta.z);
    if(horizontal<1e-6&&std::abs(delta.y)<1e-6)return camera;
    camera.rotation.x=-std::atan2(delta.y,horizontal)*180/pi;
    camera.rotation.y=std::atan2(delta.x,delta.z)*180/pi;
    return camera;
}
bool shadowOnPlane(Vec3 point,Vec3 light,const Plane &receiver,Vec3 &out){
    if(!finite(point)||!finite(light)||!finite(receiver.position)||!finite(receiver.rotation))return false;
    const auto n=rotate({0,0,1},receiver.rotation),ray=sub(point,light);
    const auto dot=[](Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
    const double denominator=dot(n,ray);
    if(std::abs(denominator)<1e-6)return false;
    const double t=dot(n,sub(receiver.position,light))/denominator;
    if(t<1||t>1000||!std::isfinite(t))return false;
    out=add(light,multiply(ray,t));return finite(out);
}
Plane importPlane(double screenX,double screenY,double,double,double sx,double sy,double rotation,double depth,const Camera &camera,double canvasWidth,double canvasHeight){
    Plane p;const double distance=std::max(20.0,depth-camera.position.z);
    const double focal=canvasHeight/(2*std::tan(camera.fov*pi/360));
    const double compensation=distance/focal;
    p.position=add(camera.position,rotate({(screenX-canvasWidth*.5)*compensation,(screenY-canvasHeight*.5)*compensation,distance},camera.rotation));
    // Compose R_camera * R_sourceZ, then extract the XYZ Euler convention used by rotate().
    const auto right=rotate(rotate({1,0,0},{0,0,rotation}),camera.rotation);
    const auto down=rotate(rotate({0,1,0},{0,0,rotation}),camera.rotation);
    const auto normal=rotate({0,0,1},camera.rotation);
    const double yaw=std::asin(std::clamp(-right.z,-1.0,1.0));
    if(std::abs(std::cos(yaw))>1e-7)p.rotation={std::atan2(down.z,normal.z)*180/pi,yaw*180/pi,std::atan2(right.y,right.x)*180/pi};
    else p.rotation={std::atan2(-normal.y,down.y)*180/pi,yaw*180/pi,0};p.scaleX=sx*compensation;p.scaleY=sy*compensation;return p;
}

}

namespace spatial {
SourceLayout sourceLayout(double w,double h,const SourceTransform &t){
    SourceLayout r;r.left=t.left;r.right=t.right;r.top=t.top;r.bottom=t.bottom;
    r.width=std::max(1.0,w-t.left-t.right);r.height=std::max(1.0,h-t.top-t.bottom);
    r.scaleX=t.scaleX;r.scaleY=t.scaleY;double ox=0,oy=0;
    auto ax=[](int flags,double size){return (flags&2)?size:((flags&1)?0.0:double(int(size)/2));};
    auto ay=[](int flags,double size){return (flags&8)?size:((flags&4)?0.0:double(int(size)/2));};
    if(t.boundsType&&t.boundsWidth>0&&t.boundsHeight>0){
        double width=r.width*std::abs(r.scaleX),height=r.height*std::abs(r.scaleY);int type=t.boundsType;
        if(type==6&&(width>t.boundsWidth||height>t.boundsHeight))type=2;
        if(type==1){r.scaleX=std::copysign(t.boundsWidth/r.width,r.scaleX);r.scaleY=std::copysign(t.boundsHeight/r.height,r.scaleY);}
        else if(type!=6){double factor=std::min(t.boundsWidth/std::max(width,.001),t.boundsHeight/std::max(height,.001));if(type==3)factor=std::max(t.boundsWidth/std::max(width,.001),t.boundsHeight/std::max(height,.001));if(type==4)factor=t.boundsWidth/std::max(width,.001);if(type==5)factor=t.boundsHeight/std::max(height,.001);r.scaleX*=factor;r.scaleY*=factor;}
        width=r.width*r.scaleX;height=r.height*r.scaleY;
        const double dx=t.boundsWidth-std::abs(width),dy=t.boundsHeight-std::abs(height);
        ox=ax(t.boundsAlignment,int(-dx));oy=ay(t.boundsAlignment,int(-dy));
        if(t.cropToBounds&&(type==3||type==4||type==5)&&(dx<-.1||dy<-.1)){
            const bool x=dx<-.1,flipped=(x?width:height)<0;const double diff=x?dx:dy,scale=x?r.scaleX:r.scaleY;
            const int mask=x?3:12,align=t.boundsAlignment&mask;const double over=std::abs(diff/scale);
            double tl=(align&(1|4))?0:((align&(2|8))?over:over*.5),br=over-tl;
            double &origin=x?ox:oy;if(flipped){if(!align)origin*=2;else if(align&(1|4))origin-=diff;std::swap(tl,br);}else origin=0;
            if(x){r.left+=int(std::round(tl));r.right+=int(std::round(br));}else{r.top+=int(std::round(tl));r.bottom+=int(std::round(br));}
        }
        ox+=width<0?width:0;oy+=height<0?height:0;
        ox+=ax(t.alignment,int(t.boundsWidth));oy+=ay(t.alignment,int(t.boundsHeight));
    }else{ox=ax(t.alignment,int(r.width*r.scaleX));oy=ay(t.alignment,int(r.height*r.scaleY));}
    r.width=std::max(1.0,w-r.left-r.right);r.height=std::max(1.0,h-r.top-r.bottom);
    r.x=r.width*r.scaleX*.5-ox;r.y=r.height*r.scaleY*.5-oy;return r;
}
}

namespace spatial {
double timelineTime(double elapsed,double start,double speed,double duration,bool loop){
    double t=std::max(0.0,start+elapsed*speed);duration=std::max(.01,duration);
    return loop?std::fmod(t,duration):std::min(t,duration);
}
Vec3 bindPosition(Vec3 local,const Plane &parent){return add(parent.position,rotate({local.x*parent.scaleX,local.y*parent.scaleY,local.z},parent.rotation));}
LayerKeyframe sampleLayer(const std::vector<LayerKeyframe> &keys,double time,bool loop){
    if(keys.empty())return {};if(keys.size()==1)return keys.front();
    const double first=keys.front().time,last=keys.back().time;
    if(loop&&last>first)time=first+std::fmod(std::max(0.0,time-first),last-first);
    if(time<=first)return keys.front();if(time>=last)return keys.back();
    auto end=std::upper_bound(keys.begin(),keys.end(),time,[](double t,const LayerKeyframe& k){return t<k.time;});
    const auto& b=*end;const auto& a=*(end-1);const double t=ease((time-a.time)/std::max(.000001,b.time-a.time),a.easing);
    auto mix=[t](double x,double y){return x+(y-x)*t;};
    auto angles=[&](double x,double y){return mix(x,x+std::remainder(y-x,360.0));};
    LayerKeyframe r=a;r.time=time;r.plane.position={mix(a.plane.position.x,b.plane.position.x),mix(a.plane.position.y,b.plane.position.y),mix(a.plane.position.z,b.plane.position.z)};
    r.plane.rotation={angles(a.plane.rotation.x,b.plane.rotation.x),angles(a.plane.rotation.y,b.plane.rotation.y),angles(a.plane.rotation.z,b.plane.rotation.z)};
    r.plane.scaleX=mix(a.plane.scaleX,b.plane.scaleX);r.plane.scaleY=mix(a.plane.scaleY,b.plane.scaleY);r.opacity=mix(a.opacity,b.opacity);return r;
}
}
