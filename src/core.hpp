#pragma once
#include <array>
#include <vector>
namespace spatial {
constexpr double pi = 3.14159265358979323846;
struct Vec3 { double x=0,y=0,z=0; };
struct Camera { Vec3 position{0,0,-1500}; Vec3 rotation{}; double fov=40; };
struct Plane { Vec3 position{}; Vec3 rotation{}; double scaleX=1,scaleY=1; };
struct SourceTransform { double scaleX=1,scaleY=1,boundsWidth=0,boundsHeight=0; int boundsType=0,alignment=5,boundsAlignment=0; int left=0,right=0,top=0,bottom=0; bool cropToBounds=false; };
struct SourceLayout { double x=0,y=0,scaleX=1,scaleY=1,width=1,height=1; int left=0,right=0,top=0,bottom=0; };
SourceLayout sourceLayout(double width,double height,const SourceTransform &source);
struct Projected { double x=0,y=0,depth=0; bool visible=false; };
enum class Easing { Linear, Smooth, CubicInOut };
struct LayerKeyframe { double time=0; Plane plane{}; double opacity=1; Easing easing=Easing::Smooth; };
LayerKeyframe sampleLayer(const std::vector<LayerKeyframe> &keys,double time,bool loop);
Vec3 bindPosition(Vec3 local,const Plane &parent);
double timelineTime(double elapsed,double start,double speed,double duration,bool loop);
struct Keyframe { double time=0; Camera camera{}; Easing easing=Easing::Smooth; };
Vec3 rotate(Vec3 p, Vec3 degrees);
Vec3 inverseRotate(Vec3 p, Vec3 degrees);
Vec3 add(Vec3 a, Vec3 b);
Vec3 sub(Vec3 a, Vec3 b);
Vec3 multiply(Vec3 a,double factor);
Projected project(Vec3 world,const Camera &camera,double width,double height);
std::array<Vec3,4> corners(const Plane &plane,double width,double height);
double ease(double t,Easing mode);
Camera sample(const std::vector<Keyframe> &keys,double time,bool loop);
Camera lookAt(Camera camera,Vec3 target);
bool shadowOnPlane(Vec3 point,Vec3 light,const Plane &receiver,Vec3 &out);
Plane importPlane(double screenX,double screenY,double width,double height,double scaleX,double scaleY,double rotation,double depth,const Camera &camera,double canvasWidth,double canvasHeight);
bool shadowPoint(Vec3 point,Vec3 light,double receiverZ,Vec3 &out);
}
