#pragma once
#include "core.hpp"
#include "animation.hpp"
#include "fx_catalog.hpp"
#include "presets/types.hpp"
#include <obs.h>
#include <cstdint>
#include <string>
namespace spatial {
inline constexpr const char *stageId="obs_spatial_fx_stage";
struct Layer {
    FxStack fx;
    std::string sourceUuid,sceneUuid,label,instanceId;
    int instanceNumber=1;
    int64_t itemId=0;
    Plane plane{},defaultPlane{};
    Animation animation;Orbit orbit;bool locked=false;double defaultOpacity=1;
    SourceTransform importTransform{};double importWidth=0,importHeight=0;bool followSourceSize=false;
    double opacity=1,perspective=1,focusBlur=1;
    double boundsWidth=0,boundsHeight=0;
    int boundsType=0,depthOrder=0,cropLeft=0,cropRight=0,cropTop=0,cropBottom=0;
    bool visible=true,originalVisible=true,enabled=true,receiveLight=true,castShadow=true,receiveShadow=true,rimLight=true;
    bool hasDefault=false;
};
struct Light {
    bool enabled=false,shadows=true,visible=false;
    int type=0;
    std::string label="点光源 1",bindUuid,receiverUuid,bindSceneUuid,receiverSceneUuid,bindInstanceId,receiverInstanceId;int64_t bindItemId=0,receiverItemId=0;
    Vec3 position{-500,-300,-700},color{1,.9,.8},shadowColor{};
    double intensity=1.5,range=2500,ambient=.6,shadowOpacity=.35,receiverZ=500;
    double size=60,falloff=2,softness=15,rim=1,rimWidth=8,rimSoftness=.65;
};
struct Fx {bool enabled=false;double exposure=0,saturation=1,vignette=0;};
struct StageModel {
    uint32_t width=1920,height=1080;
    std::string mediaUuid;bool mediaLinked=false;double mediaOffset=0;
    Camera camera{};
    Animation cameraAnimation,compositeAnimation;Orbit cameraOrbit;bool cameraLocked=false,compositeLocked=false;
    double outputScaleX=1,outputScaleY=1,outputOpacity=1;
    Vec3 target{};
    bool lookAtTarget=false,preview=true;
    double animationSpeed=1,animationDuration=6,focusDistance=1500,aperture=0;
    bool autoKey=false,keyPositionsLocked=false;
    Light light{};
    std::vector<Light> extraLights;
    Fx fx{};
    std::vector<Layer> layers;
    std::vector<presets::Geometry> geometries;
    std::vector<presets::MotionBlock> motionBlocks;
    std::vector<presets::PresetInstance> presetInstances;
    double timelinePosition=0;bool timelinePreview=false;
    bool playing=false,loop=false;
    uint64_t playToken=0;
};
bool lightMatchesLayer(const Light& light,const Layer& layer,bool receiver=false);
bool sameSourceItem(const Layer&,const Layer&);
bool sameLayerInstance(const Layer&,const Layer&);
std::string layerDisplayName(const Layer&);
StageModel readModel(obs_data_t *settings);
void writeModel(const StageModel &model,obs_data_t *settings);
obs_data_t *modelData(const StageModel &model);
Animation &objectAnimation(StageModel&,int object);
const Animation &objectAnimation(const StageModel&,int object);
bool objectLocked(const StageModel&,int object);
double baseValue(const StageModel&,int object,const std::string& parameter);
void setBaseValue(StageModel&,int object,const std::string& parameter,double value);

TimeUs contentEnd(const StageModel&);
inline double playbackEnd(const StageModel&m){return toSeconds(contentEnd(m));}
}
