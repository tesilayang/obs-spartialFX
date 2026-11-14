#pragma once
#include "model.hpp"
namespace spatial {
struct EvaluatedObject {Plane plane;double opacity=1;};
struct EvaluatedScene {Camera camera;std::vector<EvaluatedObject> authoring,world;std::vector<Vec3> lights;std::vector<presets::EvaluatedGeometry> geometries;double outputScaleX=1,outputScaleY=1,outputOpacity=1;};
struct SourceSize {double width=0,height=0;};
Plane effectivePlane(const Layer&);
EvaluatedScene evaluateSceneAt(const StageModel&,TimeUs,const std::vector<SourceSize>& layouts={},bool animation=true,double fxSeconds=-1);
// New lights use the current evaluated composition; existing lights are never relocated.
Vec3 newLightPosition(const StageModel&,TimeUs,const std::vector<SourceSize>& layouts={},bool animation=true,double fxSeconds=-1);
}
