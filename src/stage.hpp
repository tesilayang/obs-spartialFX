#pragma once
#include <obs.h>
#include <cstdint>
namespace spatial {
struct StageStats { uint64_t renders=0,renderNanoseconds=0,textureAllocations=0; uint32_t missing=0;double timeline=0;bool effectReady=false;uint64_t fxAllocations=0,fxPasses=0,fxFailures=0; };
StageStats stageStats(obs_source_t *source);
// UI-thread API: return a detached authoritative model settings copy.
obs_data_t *stageGetSettings(obs_source_t *source);
void stageApplyClock(obs_source_t*,double time,bool playing,bool preview,uint64_t token);
void stageApplySettings(obs_source_t *source,obs_data_t *settings);
}

