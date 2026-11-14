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
 auto*order=obs_data_get_array(d,"group_order");for(size_t i=0;i<obs_data_array_count(order);++i){auto*g=obs_data_array_item(order,i);const int n=int(obs_data_get_int(g,"group"));if(n>=1&&n<=13&&s.groups[size_t(n-1)]&&std::find(s.groupOrder.begin(),s.groupOrder.end(),n)==s.groupOrder.end())s.groupOrder.push_back(n);obs_data_release(g);}obs_data_array_release(order);
 for(int n=1;n<=13;++n)if(s.groups[size_t(n-1)]&&std::find(s.groupOrder.begin(),s.groupOrder.end(),n)==s.groupOrder.end())s.groupOrder.push_back(n);
 auto*effects=obs_data_get_array(d,"effects");for(size_t i=0;i<std::min(fxCatalog().size(),obs_data_array_count(effects));++i){auto*e=obs_data_array_item(effects,i);const auto*def=fxDefinition(obs_data_get_string(e,"id"));if(def&&!findFx(s,def->id)){
  auto f=makeFx(*def);f.enabled=obs_data_get_bool(e,"enabled");f.strength=finite(e,"strength",1,0,1);if(f.id!="FX05.5")f.color=readColor(e,"color",f.color);f.color2=readColor(e,"color2",f.color2);f.reference=obs_data_get_string(e,"reference");f.path=obs_data_get_string(e,"path");
  auto*p=obs_data_get_obj(e,"parameters");
  if((f.id=="FX04.0"||f.id=="FX04.1")&&!obs_data_has_user_value(e,"optics_revision")){
   // These stored colors were unused in earlier builds.
   f.color={1,1,1};f.color2={1,1,1};
  }
  // Convert the removed five-node curve to endpoint levels plus a midpoint fit.
  if(f.id=="FX03.4"&&!obs_data_has_user_value(p,"input_black")&&!obs_data_has_user_value(p,"gamma")&&obs_data_has_user_value(p,"middle")){
   const double black=finite(p,"black",0,0,1),white=finite(p,"white",1,0,1),middle=finite(p,"middle",.5,0,1);
   obs_data_set_double(p,"output_black",black);obs_data_set_double(p,"output_white",white);
   double gamma=1;if(std::abs(white-black)>1e-6){const double mid=std::clamp((middle-black)/(white-black),.001,.999);gamma=std::clamp(std::log(.5)/std::log(mid),.1,10.);}
   obs_data_set_double(p,"gamma",gamma);
  }
  if(f.id=="FX05.1"&&!obs_data_has_user_value(p,"light_x")&&obs_data_has_user_value(p,"offset")){
   const double old=finite(p,"offset",12,-100,100);obs_data_set_double(p,"light_x",old*.832050294);obs_data_set_double(p,"light_y",old*-.554700196);
  }
  // Old star choices were 4/6/8. Keep 4/6 and map the removed 8 to 6.
  // Presence of tolerance marks the new 2/4/5/6 format. Old unfiltered
  // projects remain unfiltered until the user chooses a target color.
  if(f.id=="FX04.1"&&!obs_data_has_user_value(p,"tolerance")){
   const int old=int(finite(p,"rays",0,0,2));obs_data_set_double(p,"rays",old==0?1:3);
   obs_data_set_double(p,"tolerance",1);
  }
  for(size_t j=0;j<def->parameters.size()&&j<f.values.size();++j){auto&m=def->parameters[j];f.values[j]=finite(p,m.key.c_str(),m.initial,m.minimum,m.maximum);if(!m.choices.empty())f.values[j]=std::round(f.values[j]);}obs_data_release(p);if(!f.path.empty())loadFxAsset(f,def->asset);s.effects.push_back(std::move(f));
 }obs_data_release(e);}obs_data_array_release(effects);obs_data_release(d);
 // Deterministic migration for hand-edited/imported stacks with both AA modes.
 auto*smaa=findFx(s,"FX02.2");auto*fxaa=findFx(s,"FX02.3");
 if(smaa&&fxaa&&smaa->enabled&&smaa->strength>0)fxaa->enabled=false;
 return s;
}
void writeFxStack(obs_data_t*parent,const FxStack&s){
 auto*d=obs_data_create();obs_data_set_bool(d,"attached",s.attached);auto*groups=obs_data_array_create();for(bool on:s.groups){auto*g=obs_data_create();obs_data_set_bool(g,"enabled",on);obs_data_array_push_back(groups,g);obs_data_release(g);}obs_data_set_array(d,"group_states",groups);obs_data_array_release(groups);
 auto*order=obs_data_array_create();std::array<bool,13>seen{};auto append=[&](int n){if(n<1||n>13||!s.groups[size_t(n-1)]||seen[size_t(n-1)])return;seen[size_t(n-1)]=true;auto*g=obs_data_create();obs_data_set_int(g,"group",n);obs_data_array_push_back(order,g);obs_data_release(g);};for(int n:s.groupOrder)append(n);for(int n=1;n<=13;++n)append(n);obs_data_set_array(d,"group_order",order);obs_data_array_release(order);
 auto*arr=obs_data_array_create();for(auto&f:s.effects){auto*def=fxDefinition(f.id);if(!def)continue;auto*e=obs_data_create();obs_data_set_string(e,"id",f.id.c_str());obs_data_set_bool(e,"enabled",f.enabled);if(f.id=="FX04.0"||f.id=="FX04.1"||f.id=="FX05.5")obs_data_set_int(e,"optics_revision",1);obs_data_set_double(e,"strength",f.strength);if(f.id!="FX05.5")writeColor(e,"color",f.color);writeColor(e,"color2",f.color2);obs_data_set_string(e,"reference",f.reference.c_str());obs_data_set_string(e,"path",f.path.c_str());auto*p=obs_data_create();for(size_t j=0;j<def->parameters.size()&&j<f.values.size();++j)obs_data_set_double(p,def->parameters[j].key.c_str(),f.values[j]);obs_data_set_obj(e,"parameters",p);obs_data_release(p);obs_data_array_push_back(arr,e);obs_data_release(e);}obs_data_set_array(d,"effects",arr);obs_data_array_release(arr);obs_data_set_obj(parent,"fx_stack",d);obs_data_release(d);
}
}
