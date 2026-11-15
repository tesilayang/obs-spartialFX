#pragma once
#include "model.hpp"
#include <graphics/graphics.h>
#include <array>
#include <memory>
#include <cmath>
namespace spatial {
struct FxClockSample {double elapsed=0;uint64_t epoch=0;bool advance=false;};
// Render-thread policy only: no graphics resources, UI access, or wall-clock reads.
class FxPlaybackClock {
 bool initialized=false,timeline=false;uint64_t lastFrame=0,lastToken=0;
 double lastTime=0,lastStart=0;FxClockSample sample;
public:
 FxClockSample update(uint64_t frame,double wallElapsed,double timelineTime,bool useTimeline,bool playing,double speed,uint64_t token,double start){
  if(initialized&&frame==lastFrame)return sample;
  const double elapsed=std::max(0.0,useTimeline?timelineTime:wallElapsed);
  const double wallDelta=initialized&&frame>=lastFrame?double(frame-lastFrame)/1e9:0;
  const double delta=elapsed-lastTime;
  bool reset=!initialized||useTimeline!=timeline;
  if(initialized&&useTimeline&&timeline){
   reset=reset||token!=lastToken||std::abs(start-lastStart)>.000001||delta<-.000001;
   // Paused time changes are explicit scrubs. While playing, tolerate a few
   // frames of media-clock jitter, but reject discontinuities in either direction.
   if(!playing)reset=reset||std::abs(delta)>.000001;
   else reset=reset||std::abs(delta-wallDelta*speed)>std::max(.05,wallDelta*.25);
  }
  if(reset)++sample.epoch;
  sample.elapsed=elapsed;sample.advance=reset||delta>.000001;
  initialized=true;timeline=useTimeline;lastFrame=frame;lastToken=token;lastTime=elapsed;lastStart=start;return sample;
 }
};
struct FxRenderContext {
 uint64_t frame=0;double elapsed=0;uint32_t width=1,height=1;Vec3 position{};
 uint64_t historyEpoch=0;bool advanceHistory=true,timelineClock=false;
 uint32_t sourceWidth=1,sourceHeight=1;
 Vec3 right{1,0,0},down{0,1,0},normal{0,0,-1},cameraPosition{0,0,-1500};
 double uvCenterX=.5,uvCenterY=.5;
 double uvSpanX=1,uvSpanY=1;
 double motionX=0,motionY=0;gs_texture_t*auxiliary=nullptr;
 double cropLeft=0,cropTop=0,cropRight=0,cropBottom=0;
};
struct FxRenderResult {
 gs_texture_t*texture=nullptr;
 // Surface before local optical composites, used by global casters and rim contours.
 gs_texture_t*caster=nullptr;
 double paddingX=0,paddingY=0; // original source pixels, not world units
};
// Every method touching graphics is called inside Stage::video_render/destroy.
class FxRenderer {
public:
 FxRenderer();~FxRenderer();
 void beginFrame(const std::vector<Layer>&);
 FxRenderResult process(const Layer&,gs_texture_t*,const FxRenderContext&,const std::vector<gs_texture_t*>&);
 uint64_t allocations()const;uint64_t passes()const;uint64_t failures()const;
private:
 struct Impl;std::unique_ptr<Impl>impl;
};
}
