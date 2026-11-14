#pragma once
#include "presets/presets.hpp"

namespace spatial::presets {
struct PartDefinition {
 const char *id,*name,*description;
};
struct StyleDefinition {
 const char *id,*name,*description;
 Vec3 colorA,colorB;
};
const std::vector<StyleDefinition>& styleCatalog();
const std::vector<PartDefinition>& motionCatalog();
const std::vector<PartDefinition>& maskCatalog();
const std::vector<PartDefinition>& decorationCatalog();
struct ComposeRequest {
 std::string styleId="gentle";
 std::vector<std::string> selectedIds;
 uint32_t seed=1;
 uint32_t motionSeed=0,maskSeed=0,decorationSeed=0;
 // Empty part IDs mean style-based random selection. "none" disables a part.
 std::string motionId,maskId="none",decorationId;
 // Nonempty replays a complete resolved mixture; decorationId keeps the primary ID.
 std::vector<std::string> resolvedDecorationIds;
 // A frozen decoration recipe keeps its style context when the motion style changes.
 std::string decorationStyleId;
 TimeUs start=0,duration=6000000;
 int64_t cycles=1;
 double strength=.6;
 int decorationAmount=1; // 0=none, 1=sparse, 2=rich.
 bool loop=false,replace=false,camera=false;
 Vec3 colorA{.38,.28,.95},colorB{.1,.65,1};
 Plane decorationPlane{};
 Plane localMaskPlane{};
 std::string anchorId;
 bool localMask=true,screenFixed=false;
};
// Atomic: failure leaves model unchanged. Resolved part IDs and seed persist.
enum class ComposeFailure { None, IntervalConflict, Other };
bool compileComposition(StageModel&,const ComposeRequest&,std::string&error,ComposeFailure*failure=nullptr);
}
