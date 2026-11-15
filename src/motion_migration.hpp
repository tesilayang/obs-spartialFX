#pragma once
#include "model.hpp"
namespace spatial {
// Keeps the original settings in the project before replacing legacy motion.
void migrateLoadedMotion(StageModel&,obs_data_t*);
void writeMotionMigration(const StageModel&,obs_data_t*);
}
