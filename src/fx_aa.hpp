#pragma once
#include <graphics/graphics.h>
#include <array>
#include <cstdint>
namespace spatial {
// Both objects are owned and destroyed exclusively inside the graphics context.
// LUTs are shared by all active layers; intermediate images are per layer.
struct SmaaTables {
 gs_texture_t*area=nullptr,*search=nullptr;
 ~SmaaTables();
 bool prepare(uint64_t&alloc);
};
class FxAA {
public:
 ~FxAA();
 gs_texture_t*render(gs_effect_t*,SmaaTables&,gs_texture_t*,uint32_t,uint32_t,
                    double threshold,int quality,double strength,uint64_t&alloc,uint64_t&draws);
private:
 struct Target {gs_texrender_t*value=nullptr;uint32_t width=0,height=0;};
 std::array<Target,3>targets{};
};
}
