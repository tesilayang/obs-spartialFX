#pragma once
#include "types.hpp"
#include <graphics/graphics.h>
#include <array>
#include <memory>

namespace spatial::presets {
struct Receiver {
 std::string instanceId;
 Plane plane;
 double sourceWidth=1,sourceHeight=1;
 double width=1,height=1,left=0,top=0;
};
// GPU calls are confined to the stage render/destroy graphics context.
// Source slots are owned by Stage; geometry never consumes a source slot.
class GraphicsRenderer {
public:
 GraphicsRenderer();~GraphicsRenderer();
 bool draw(const EvaluatedGeometry&,const Camera&,uint32_t canvasWidth,uint32_t canvasHeight);
 gs_texture_t*mask(size_t slot,gs_texture_t*,const Receiver&,
                  const std::vector<EvaluatedGeometry>&,const Camera&,uint32_t canvasWidth,uint32_t canvasHeight);
 uint64_t allocations()const;uint64_t passes()const;uint64_t failures()const;
private:
 struct Impl;std::unique_ptr<Impl>impl;
};
}
