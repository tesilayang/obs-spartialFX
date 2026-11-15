#pragma once
#include "model.hpp"
#include <optional>
namespace spatial {
enum class TimelineKind {Camera,Composite,Layer,Geometry,Light,Fx};
struct TimelineRef {TimelineKind kind=TimelineKind::Camera;std::string id,owner;};
struct TimelineObject {int handle;TimelineRef ref;std::string label;bool enabled=true;};
struct TimelineParameter {std::string id,label;double minimum,maximum,sliderMin,sliderMax;bool discrete=false,integer=false;};
inline constexpr int lightHandleBase=-1000000,fxHandleBase=-2000000;
inline constexpr int invalidTimelineObject=INT32_MIN;
inline int remapTimelineLayer(int handle,int from,int to){
 const bool fx=handle<=fxHandleBase&&handle!=invalidTimelineObject;
 const int layer=fx?int((int64_t(fxHandleBase)-handle)/256):handle;
 if(layer<0)return handle;
 const int mapped=layer==from?to:from<to&&layer>from&&layer<=to?layer-1:from>to&&layer>=to&&layer<from?layer+1:layer;
 return fx?fxHandleBase-mapped*256-int((int64_t(fxHandleBase)-handle)%256):mapped;
}
inline bool geometryObject(int h){return h<=-3&&h>lightHandleBase;}
inline bool lightObject(int h){return h<=lightHandleBase&&h>fxHandleBase;}
inline bool fxObject(int h){return h<=fxHandleBase;}
std::vector<TimelineObject> timelineObjects(const StageModel&);
std::vector<TimelineParameter> objectParameters(const StageModel&,int);
std::optional<TimelineParameter> objectParameter(const StageModel&,int,const std::string&);
std::optional<int> resolveTimelineObject(const StageModel&,const TimelineRef&);
bool validObject(const StageModel&,int);
Light* timelineLight(StageModel&,int);
FxInstance* timelineFx(StageModel&,int);
const Light* timelineLight(const StageModel&,int);
const FxInstance* timelineFx(const StageModel&,int);
void setObjectLocked(StageModel&,int,bool);
void sanitizeObjectAnimations(StageModel&);
double evaluatedValue(const StageModel&,int,const std::string&,TimeUs,bool=true);
Light evaluateLight(const StageModel&,int,TimeUs,bool=true);
FxStack evaluateLayerFx(const StageModel&,size_t,TimeUs,bool=true);
bool writeAnimatedValue(StageModel&,int,const std::string&,double,std::string&);
struct AnimationPaste {Animation tracks;std::vector<std::string> skipped;bool conflict=false;TimeUs first=0,last=0;};
AnimationPaste prepareAnimationPaste(const StageModel&,int,const Animation&,TimeUs);
bool pasteObjectAnimation(StageModel&,int,const AnimationPaste&);
bool duplicateTimelineLayer(StageModel&,const Layer&,std::string&);
}
