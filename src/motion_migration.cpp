#include "motion_migration.hpp"
#include "presets/composer.hpp"
#include <set>
namespace spatial {
namespace {
bool rawMotionValid(obs_data_t*d,std::string&error){
 auto*bs=obs_data_get_array(d,"presets_motion_blocks");bool valid=true;std::set<std::string>ids;
 if(obs_data_array_count(bs)>512){error="旧运动块超过 512 个，未执行转换；原始数据已保留。";valid=false;}
 for(size_t i=0;valid&&i<obs_data_array_count(bs);++i){auto*b=obs_data_array_item(bs,i);std::string id=obs_data_get_string(b,"instance_id");if(id.empty()||!ids.insert(id).second)valid=false;
  auto*cs=obs_data_get_array(b,"channels");if(obs_data_array_count(cs)>32||obs_data_array_count(cs)==0)valid=false;
  for(size_t j=0;valid&&j<obs_data_array_count(cs);++j){auto*c=obs_data_array_item(cs,j);if(!parameter(obs_data_get_string(c,"parameter")))valid=false;for(const auto*k:{"amplitude","phase"})if(!std::isfinite(obs_data_get_double(c,k)))valid=false;
   auto*samples=obs_data_get_array(c,"samples");if(obs_data_array_count(samples)>257)valid=false;for(size_t k=0;valid&&k<obs_data_array_count(samples);++k){auto*v=obs_data_array_item(samples,k);auto n=obs_data_get_double(v,"v");if(!std::isfinite(n)||n< -100||n>100)valid=false;obs_data_release(v);}obs_data_array_release(samples);obs_data_release(c);
  }obs_data_array_release(cs);obs_data_release(b);
 }
 obs_data_array_release(bs);if(!valid&&error.empty())error="旧运动数据含不支持或异常参数，未执行转换；原始数据已保留。";return valid;
}
}
void migrateLoadedMotion(StageModel&m,obs_data_t*d){
 m.legacyMotionBackup=obs_data_get_string(d,"legacy_motion_backup");m.motionMigrationError=obs_data_get_string(d,"motion_migration_error");m.motionMigrationFailed=obs_data_get_bool(d,"motion_migration_failed");
 auto*blocks=obs_data_get_array(d,"presets_motion_blocks");const bool any=obs_data_array_count(blocks)>0;obs_data_array_release(blocks);if(!any)return;
 if(m.legacyMotionBackup.empty()){auto*backup=obs_data_create();obs_data_apply(backup,d);obs_data_erase(backup,"legacy_motion_backup");obs_data_erase(backup,"motion_migration_error");obs_data_erase(backup,"motion_migration_failed");m.legacyMotionBackup=obs_data_get_json(backup);obs_data_release(backup);}
 if(m.motionMigrationFailed)return;
 std::string error;if(!rawMotionValid(d,error)){m.motionMigrationError=std::move(error);m.motionMigrationFailed=true;return;}
 presets::MotionMigrationReport report;if(!presets::migrateLegacyMotionBlocks(m,error,&report)){m.motionMigrationError=std::move(error);m.motionMigrationFailed=true;return;}m.motionMigrationError.clear();m.motionMigrationFailed=false;
}
void writeMotionMigration(const StageModel&m,obs_data_t*d){
 if(!m.legacyMotionBackup.empty())obs_data_set_string(d,"legacy_motion_backup",m.legacyMotionBackup.c_str());obs_data_set_string(d,"motion_migration_error",m.motionMigrationError.c_str());obs_data_set_bool(d,"motion_migration_failed",m.motionMigrationFailed);
 if(m.motionMigrationFailed&&!m.legacyMotionBackup.empty()){auto*original=obs_data_create_from_json(m.legacyMotionBackup.c_str());auto*blocks=obs_data_get_array(original,"presets_motion_blocks");obs_data_set_array(d,"presets_motion_blocks",blocks);obs_data_array_release(blocks);obs_data_release(original);}
}
}
