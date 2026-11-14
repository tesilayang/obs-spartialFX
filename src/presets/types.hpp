#pragma once
#include "core.hpp"
#include "animation.hpp"
#include <string>
#include <vector>
namespace spatial::presets {
enum class Shape { Rectangle, Parallelogram, Ellipse, Ring, Stripes, Grid, Checker, Polygon,
 Star,Blob,WaveBand,DotGrid,Dashed,Radial,Cross,Gradient };
enum class Mode { Decoration, Mask, Both };
struct Geometry {
 std::string instanceId,ownerId,label,anchorId;
 Shape shape=Shape::Rectangle;Mode mode=Mode::Decoration;
 Plane plane{};Animation animation;
 double width=900,height=500,thickness=10,softness=1,spacing=80,sides=6,shear=.35,opacity=1;
 double referenceWidth=0,referenceHeight=0;
 Vec3 layoutPivot{};
 Vec3 color{.38,.28,.95};int depthOrder=0;
 bool enabled=true,visible=true,locked=false,localMask=false,screenFixed=false,invert=false;
 std::vector<std::string> receivers;
 // v3.6.1: analytic reveal family, evaluated using an absolute timeline clock.
 int reveal=0;double progress=1,detail=0;
 TimeUs revealStart=0,revealPeriod=0;bool revealLoop=false;
};
struct MotionChannel {std::string parameter;double amplitude=0,phase=0;bool pulse=false;
 std::vector<double> samples;bool stepped=false;
};
struct MotionBlock {
 std::string instanceId,ownerId,targetId,label;
 TimeUs start=0,end=6000000,period=6000000;
 TimeUs timeOrigin=-1;
 bool enabled=true,camera=false;
 std::vector<MotionChannel> channels;
};
struct GeneratedTrack {std::string targetId,parameter;Curve before,after;bool existed=false;};
struct PresetInstance {
 std::string instanceId,definitionId,definitionSnapshot,label;int version=1;
 uint32_t seed=1;TimeUs start=0,end=6000000;
 std::vector<GeneratedTrack> generatedTracks;
};
struct EvaluatedGeometry {
 Geometry geometry;Plane plane{};double opacity=1;bool active=true;
};
struct Definition {const char*id;const char*name;const char*description;std::vector<std::string> variants;bool requiresBackground=false,requiresSecond=false;};
struct ApplyRequest {
 std::string templateId="blocks",subjectId,secondId,anchorId;
 std::vector<std::string> backgroundIds,foregroundIds,maskTargets;
 int variant=0;Mode mode=Mode::Decoration;
 TimeUs start=0,period=6000000;int64_t cycles=1;
 double strength=1;Vec3 colorA{.38,.28,.95},colorB{.1,.65,1};
 Plane decorationPlane{};
 bool camera=false,loop=false,motion=true,decoration=true,replace=false,localMask=false,screenFixed=false;
};
}
