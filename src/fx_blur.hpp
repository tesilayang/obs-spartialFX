#pragma once
#include <graphics/graphics.h>
#include <array>
#include <cstdint>
namespace spatial {
// Owned by one layer, reused across effects and frames. Graphics context only.
class FxBlur {
public:
 ~FxBlur();
 gs_texture_t*render(gs_effect_t*,gs_texture_t*,uint32_t,uint32_t,double radius,
                    int algorithm,int edge,double aperture,double rotation,
                    double threshold,double boost,int fieldMode,const std::array<double,4>&field,
                    uint64_t&alloc,uint64_t&draws);
private:
 struct Target {gs_texrender_t*value=nullptr;uint32_t width=0,height=0;};
 std::array<Target,6>reduction{};
 std::array<std::array<Target,2>,7>axis{};
};
}
