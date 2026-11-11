#pragma once
#include "model.hpp"
#include <graphics/graphics.h>
#include <array>
#include <memory>
namespace spatial {
struct FxRenderContext {
 uint64_t frame=0;double elapsed=0;uint32_t width=1,height=1;Vec3 position{};Light light{};
 Vec3 right{1,0,0},down{0,1,0},normal{0,0,-1},cameraPosition{0,0,-1500};
 double uvCenterX=.5,uvCenterY=.5;
 double motionX=0,motionY=0;gs_texture_t*auxiliary=nullptr;
};
// Every method touching graphics is called inside Stage::video_render/destroy.
class FxRenderer {
public:
 FxRenderer();~FxRenderer();
 void beginFrame(const std::vector<Layer>&);
 gs_texture_t*process(const Layer&,gs_texture_t*,const FxRenderContext&,const std::vector<gs_texture_t*>&);
 uint64_t allocations()const;uint64_t passes()const;uint64_t failures()const;
private:
 struct Impl;std::unique_ptr<Impl>impl;
};
}
