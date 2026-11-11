#pragma once
#include "core.hpp"
#include <algorithm>
#include <cmath>

namespace spatial {
struct FxLightingState {
    Vec3 position{},color{1,.9,.8};
    double intensity=1.5,range=2500;
};

// Material FX have a local rig when there is no enabled scene lamp. This does
// not enable StageModel::light or the separate scene illumination pass.
inline FxLightingState resolveFxLighting(bool enabled,FxLightingState configured,
                                         Vec3 position,Vec3 right,Vec3 down,Vec3 normal)
{
    if(enabled)return configured;
    const auto magnitude=[](Vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);};
    const double extent=std::max({magnitude(right),magnitude(down),1.0});
    const double normalLength=magnitude(normal);
    if(normalLength>1e-8){normal.x/=normalLength;normal.y/=normalLength;normal.z/=normalLength;}
    else normal={0,0,-1};
    FxLightingState local;
    local.position={position.x+right.x*.18-down.x*.12+normal.x*extent*.65,
                    position.y+right.y*.18-down.y*.12+normal.y*extent*.65,
                    position.z+right.z*.18-down.z*.12+normal.z*extent*.65};
    local.intensity=2;
    local.range=extent*4;
    return local;
}
}
