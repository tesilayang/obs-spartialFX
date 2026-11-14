#pragma once
#include "model.hpp"
#include <graphics/graphics.h>
#include <array>
#include <memory>
namespace spatial {
struct FxRenderContext {
 uint64_t frame=0;double elapsed=0;uint32_t width=1,height=1;Vec3 position{};
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
