#pragma once
#include "presets/types.hpp"
#include "model.hpp"
namespace spatial::presets {
const std::vector<Definition>& catalog();
bool applyPreset(StageModel&,const ApplyRequest&,std::string& error);
bool removePreset(StageModel&,const std::string& instanceId,std::string& error);
bool bakePreset(StageModel&,const std::string& instanceId,TimeUs start,TimeUs end,TimeUs step,std::string& error);
double motionOffset(const StageModel&,const std::string& target,const std::string& parameter,TimeUs);
std::vector<EvaluatedGeometry> evaluateGeometry(const StageModel&,TimeUs,const std::vector<Plane>& world,bool motion=true);
void readPresets(StageModel&,obs_data_t*);
void writePresets(const StageModel&,obs_data_t*);
}
