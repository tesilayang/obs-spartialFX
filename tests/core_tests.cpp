#include "core.hpp"
#include "fx_motion.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace spatial;
int checks=0,failed=0;
void require(bool yes,const char *label) { ++checks; if(!yes){++failed;std::cerr<<"FAIL: "<<label<<'\n';} }
bool close(double a,double b,double eps=1e-7) { return std::abs(a-b)<eps; }
int main() {
    Camera motionCamera;motionCamera.fov=90;motionCamera.position.z=-1000;
    auto motion=motionUvFromScreen({},{400,0,0},{0,300,0},motionCamera,1920,1080,21.6,0);
    require(close(motion.x,.1)&&close(motion.y,0),"one-frame motion maps screen pixels to scaled source UV");
    motion=motionUvFromScreen({},{200,0,0},{0,150,0},motionCamera,1920,1080,21.6,0);
    require(close(motion.x,.2),"half-size layer doubles UV displacement for same screen motion");
    motion=motionUvFromScreen({},{0,400,0},{-300,0,0},motionCamera,1920,1080,0,21.6);
    require(close(motion.x,.1)&&close(motion.y,0),"rotated layer motion follows projected local axes");
    motion=motionUvFromScreen({},{0,0,400},{0,300,0},motionCamera,1920,1080,21.6,0);
    require(close(motion.x,0)&&close(motion.y,0),"edge-on motion basis safely disables blur");
    LayerKeyframe la,lb;la.time=0;lb.time=2;la.plane.rotation.z=170;lb.plane.rotation.z=-170;lb.plane.position.x=200;lb.opacity=.2;la.easing=Easing::Linear;
    auto pose=sampleLayer({la,lb},1,false);
    require(close(pose.plane.position.x,100)&&close(pose.opacity,.6),"layer timeline interpolates pose and opacity");
    require(close(std::abs(pose.plane.rotation.z),180),"layer rotation takes shortest arc");
    Plane parent;parent.position={100,20,30};parent.rotation.z=90;parent.scaleX=2;parent.scaleY=3;
    auto bound=bindPosition({10,4,-5},parent);
    require(close(bound.x,88)&&close(bound.y,40)&&close(bound.z,25),"bound light follows parent translation rotation and scale");
    require(close(timelineTime(9,1,1,6,true),4),"global timeline loops all tracks together");
    require(close(timelineTime(9,1,1,6,false),6),"global timeline clamps at work area end");
    Camera camera; camera.fov=90;camera.position={0,0,-1000};
    auto center=project({0,0,0},camera,1920,1080);
    require(center.visible&&close(center.x,960)&&close(center.y,540),"center projects to canvas center");
    auto near=project({100,0,0},camera,1920,1080);
    auto far=project({100,0,1000},camera,1920,1080);
    require(close(near.x-960,2*(far.x-960)),"depth produces physical parallax");
    require(!project({0,0,-1000},camera,1920,1080).visible,"near-plane singularity culled");
    require(!project({0,0,-2000},camera,1920,1080).visible,"behind camera culled");
    require(!project({0,0,0},camera,0,1080).visible,"zero canvas rejected");
    camera.position.x=100;
    require(project({0,0,0},camera,1920,1080).x<960,"camera X moves world opposite");
    for(int x=-180;x<=180;x+=30) {
        Vec3 original{11,-52,91},angles{double(x),double(x/2),double(-x)};
        auto roundtrip=inverseRotate(rotate(original,angles),angles);
        require(close(roundtrip.x,original.x)&&close(roundtrip.y,original.y)&&close(roundtrip.z,original.z),"XYZ rotation has inverse");
    }
    Plane plane;plane.position={100,50,10};plane.scaleX=2;plane.scaleY=3;
    auto quad=corners(plane,100,60);
    require(close(quad[0].x,0)&&close(quad[0].y,-40)&&close(quad[3].x,200)&&close(quad[3].y,140),"scaled plane corners");
    require(close(ease(0,Easing::Smooth),0)&&close(ease(1,Easing::Smooth),1),"smooth easing endpoints");
    require(close(ease(0.25,Easing::Linear),0.25),"linear easing");
    require(close(ease(0.25,Easing::CubicInOut),0.0625),"cubic easing");
    std::vector<Keyframe> keys(2);keys[0].time=0;keys[0].easing=Easing::Linear;keys[1].time=2;
    keys[0].camera.position.x=0;keys[1].camera.position.x=200;
    keys[0].camera.rotation.y=170;keys[1].camera.rotation.y=-170;
    require(close(sample(keys,1,false).position.x,100),"keyframe interpolation at midpoint");
    require(close(std::abs(sample(keys,1,false).rotation.y),180),"shortest angular interpolation");
    require(close(sample(keys,-1,false).position.x,0),"negative time clamps first frame");
    require(close(sample(keys,9,false).position.x,200),"end clamps last frame");
    require(close(sample(keys,3,true).position.x,100),"loop wraps time");
    require(close(sample({},1,false).position.z,-1500),"empty animation returns default camera");
    Vec3 shadow;
    require(shadowPoint({100,0,0},{0,0,-100},100,shadow)&&close(shadow.x,200)&&close(shadow.z,100),"point light projects silhouette onto receiver");
    require(!shadowPoint({0,0,-100},{0,0,-100},100,shadow),"parallel shadow ray rejected");
    require(!shadowPoint({0,0,200},{0,0,-100},100,shadow),"receiver before object does not cast shadow");
        Camera aimed;aimed.position={100,0,-1000};aimed=lookAt(aimed,{0,0,0});
    auto target=project({0,0,0},aimed,1920,1080);require(target.visible&&close(target.x,960,1e-5),"target tracking centers world point");
    Plane receiver;receiver.position={0,0,100};receiver.rotation={0,20,0};
    require(shadowOnPlane({100,0,0},{0,0,-100},receiver,shadow)&&close(sub(shadow,receiver.position).x*std::sin(20*pi/180)+sub(shadow,receiver.position).z*std::cos(20*pi/180),0,1e-6),"shadow intersects tilted receiver plane");
    auto imported=importPlane(200,300,640,360,1,1,0,500,Camera{},1920,1080);
    auto importedCenter=project(imported.position,Camera{},1920,1080);
    require(close(importedCenter.x,200)&&close(importedCenter.y,300),"depth assignment preserves source screen center");
    auto importedCorners=corners(imported,640,360);auto tl=project(importedCorners[0],Camera{},1920,1080);auto br=project(importedCorners[3],Camera{},1920,1080);
    require(close(br.x-tl.x,640)&&close(br.y-tl.y,360),"depth compensation preserves source footprint");
    Camera turned;turned.rotation={20,60,5};
    auto angled=importPlane(960,540,640,360,1,1,90,300,turned,1920,1080);
    auto aq=corners(angled,640,360);auto a0=project(aq[0],turned,1920,1080),a1=project(aq[1],turned,1920,1080),a2=project(aq[2],turned,1920,1080);
    require(close(a1.x-a0.x,0,1e-5)&&close(a1.y-a0.y,640,1e-5)&&close(a2.x-a0.x,-360,1e-5),"import composes source rotation in tilted camera local coordinates");
    SourceTransform transform;auto pending=sourceLayout(1920,1080,transform),ready=sourceLayout(640,480,transform);
    require(close(pending.x-ready.x,640)&&close(pending.y-ready.y,300),"pending source dimension change preserves top-left anchor via center correction");
    transform.boundsType=2;transform.boundsWidth=1000;transform.boundsHeight=1000;transform.boundsAlignment=5;
    auto bounded=sourceLayout(640,360,transform);
    require(close(bounded.x,500)&&close(bounded.y,281.25),"inner bounds top alignment keeps actual material center");
    transform.boundsType=3;transform.cropToBounds=true;transform.boundsAlignment=0;
    auto cropped=sourceLayout(640,360,transform);
    require(cropped.left>0&&cropped.right>0&&close(cropped.height,360),"outer bounds crops alpha image to receiver rectangle");
    std::cout<<checks-failed<<'/'<<checks<<" checks passed; failures="<<failed<<'\n';
    return failed?EXIT_FAILURE:EXIT_SUCCESS;
}

