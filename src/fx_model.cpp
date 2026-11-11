#include "fx_catalog.hpp"
#include <algorithm>
#include <cmath>
namespace spatial {
namespace {
double finite(obs_data_t*d,const char*key,double def,double lo,double hi){if(!obs_data_has_user_value(d,key))return def;double n=obs_data_get_double(d,key);return std::isfinite(n)?std::clamp(n,lo,hi):def;}
Vec3 readColor(obs_data_t*d,const char*key,Vec3 def){auto*v=obs_data_get_obj(d,key);if(v)def={finite(v,"r",def.x,0,1),finite(v,"g",def.y,0,1),finite(v,"b",def.z,0,1)};obs_data_release(v);return def;}
void writeColor(obs_data_t*d,const char*key,Vec3 v){auto*c=obs_data_create();obs_data_set_double(c,"r",v.x);obs_data_set_double(c,"g",v.y);obs_data_set_double(c,"b",v.z);obs_data_set_obj(d,key,c);obs_data_release(c);}
}
FxStack readFxStack(obs_data_t*parent){
 FxStack s;auto*d=obs_data_get_obj(parent,"fx_stack");if(!d)return s;s.attached=obs_data_get_bool(d,"attached");
 auto*groups=obs_data_get_array(d,"group_states");for(size_t i=0;i<std::min(size_t(13),obs_data_array_count(groups));++i){auto*g=obs_data_array_item(groups,i);s.groups[i]=obs_data_get_bool(g,"enabled");obs_data_release(g);}obs_data_array_release(groups);
 auto*effects=obs_data_get_array(d,"effects");for(size_t i=0;i<std::min(fxCatalog().size(),obs_data_array_count(effects));++i){auto*e=obs_data_array_item(effects,i);const auto*def=fxDefinition(obs_data_get_string(e,"id"));if(def&&!findFx(s,def->id)){
  auto f=makeFx(*def);f.enabled=obs_data_get_bool(e,"enabled");f.strength=finite(e,"strength",1,0,1);f.color=readColor(e,"color",f.color);f.color2=readColor(e,"color2",f.color2);f.reference=obs_data_get_string(e,"reference");f.path=obs_data_get_string(e,"path");
  auto*p=obs_data_get_obj(e,"parameters");for(size_t j=0;j<def->parameters.size()&&j<f.values.size();++j){auto&m=def->parameters[j];f.values[j]=finite(p,m.key.c_str(),m.initial,m.minimum,m.maximum);if(!m.choices.empty())f.values[j]=std::round(f.values[j]);}obs_data_release(p);if(!f.path.empty())loadFxAsset(f,def->asset);s.effects.push_back(std::move(f));
 }obs_data_release(e);}obs_data_array_release(effects);obs_data_release(d);
 // Deterministic migration for hand-edited/imported stacks with both AA modes.
 auto*smaa=findFx(s,"FX02.2");auto*fxaa=findFx(s,"FX02.3");
 if(smaa&&fxaa&&smaa->enabled&&smaa->strength>0)fxaa->enabled=false;
 return s;
}
void writeFxStack(obs_data_t*parent,const FxStack&s){
 auto*d=obs_data_create();obs_data_set_bool(d,"attached",s.attached);auto*groups=obs_data_array_create();for(bool on:s.groups){auto*g=obs_data_create();obs_data_set_bool(g,"enabled",on);obs_data_array_push_back(groups,g);obs_data_release(g);}obs_data_set_array(d,"group_states",groups);obs_data_array_release(groups);
 auto*arr=obs_data_array_create();for(auto&f:s.effects){auto*def=fxDefinition(f.id);if(!def)continue;auto*e=obs_data_create();obs_data_set_string(e,"id",f.id.c_str());obs_data_set_bool(e,"enabled",f.enabled);obs_data_set_double(e,"strength",f.strength);writeColor(e,"color",f.color);writeColor(e,"color2",f.color2);obs_data_set_string(e,"reference",f.reference.c_str());obs_data_set_string(e,"path",f.path.c_str());auto*p=obs_data_create();for(size_t j=0;j<def->parameters.size()&&j<f.values.size();++j)obs_data_set_double(p,def->parameters[j].key.c_str(),f.values[j]);obs_data_set_obj(e,"parameters",p);obs_data_release(p);obs_data_array_push_back(arr,e);obs_data_release(e);}obs_data_set_array(d,"effects",arr);obs_data_array_release(arr);obs_data_set_obj(parent,"fx_stack",d);obs_data_release(d);
}
}
