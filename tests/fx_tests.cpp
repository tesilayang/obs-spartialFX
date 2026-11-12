#include "model.hpp"
#include "fx_shader.hpp"
#include <obs.h>
#include <iostream>
#include <cstdlib>
#include <set>
#include <cmath>
#include <fstream>
static void require(bool ok,const char*label){if(!ok){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}std::cout<<"PASS "<<label<<'\n';}
static size_t parameterIndex(const spatial::FxDefinition&def,const std::string&key){for(size_t i=0;i<def.parameters.size();++i)if(def.parameters[i].key==key)return i;require(false,"six-color named parameter exists");return 0;}
static void sixColorPersistence(){
 const auto*def=spatial::fxDefinition("FX03.1");require(def!=nullptr,"six-color effect registered");auto instance=spatial::makeFx(*def);require(instance.values.size()==32,"FX instance stores 32 packed shader values");require(def->parameters.size()==22,"six-color effect exposes eighteen independent controls and four shared controls");
 for(size_t i=0;i<def->parameters.size();++i)require(instance.values[i]==def->parameters[i].initial,"all six-color controls including extended slots receive catalog defaults");
 const std::array<std::string,6>names={"red","yellow","green","cyan","blue","magenta"};const std::array<double,6>hues={-50,-30,-10,10,30,50},saturations={.4,.6,.8,1.2,1.4,1.6},densities={-.6,-.4,-.2,.2,.4,.6};
 for(size_t i=0;i<names.size();++i){instance.values[parameterIndex(*def,names[i]+"_hue")]=hues[i];instance.values[parameterIndex(*def,names[i]+"_saturation")]=saturations[i];instance.values[parameterIndex(*def,names[i]+"_density")]=densities[i];}
 instance.values[parameterIndex(*def,"width")]=42;instance.values[parameterIndex(*def,"remap")]=.6;instance.values[parameterIndex(*def,"range")]=4;instance.values[parameterIndex(*def,"mode")]=1;instance.enabled=true;
 spatial::FxStack original;original.attached=true;original.groups[2]=true;original.effects.push_back(instance);auto*parent=obs_data_create();spatial::writeFxStack(parent,original);
 auto*saved=obs_data_get_obj(parent,"fx_stack");auto*effects=obs_data_get_array(saved,"effects");auto*effect=obs_data_array_item(effects,0);auto*parameters=obs_data_get_obj(effect,"parameters");
 for(size_t i=0;i<names.size();++i){const auto hue=names[i]+"_hue",sat=names[i]+"_saturation",density=names[i]+"_density";require(obs_data_has_user_value(parameters,hue.c_str())&&obs_data_get_double(parameters,hue.c_str())==hues[i]&&obs_data_get_double(parameters,sat.c_str())==saturations[i]&&obs_data_get_double(parameters,density.c_str())==densities[i],"six independent color triplets are simultaneously serialized by stable keys");}
 const auto restored=spatial::readFxStack(parent);require(restored.effects.size()==1,"six-color stack roundtrips once");for(size_t i=0;i<def->parameters.size();++i)require(restored.effects[0].values[i]==instance.values[i],"every six-color control survives save and read including slots beyond sixteen");
 obs_data_set_double(parameters,"magenta_density",400);obs_data_set_double(parameters,"width",-20);obs_data_set_double(parameters,"range",2.6);obs_data_set_obj(effect,"parameters",parameters);obs_data_array_erase(effects,0);obs_data_array_push_back(effects,effect);obs_data_set_array(saved,"effects",effects);obs_data_set_obj(parent,"fx_stack",saved);const auto clamped=spatial::readFxStack(parent);require(clamped.effects[0].values[parameterIndex(*def,"magenta_density")]==1&&clamped.effects[0].values[parameterIndex(*def,"width")]==5&&clamped.effects[0].values[parameterIndex(*def,"range")]==3,"extended-slot numeric ranges and combo indices are sanitized on read");
 obs_data_release(parameters);obs_data_release(effect);obs_data_array_release(effects);obs_data_release(saved);obs_data_release(parent);
}
static void localLightControlPersistence(){
 for(const char*id:{"FX05.2","FX05.3"}){
  const auto*def=spatial::fxDefinition(id);require(def&&def->parameters.size()==8,"candle and spotlight expose independent center and size controls");
  require(def->parameters[4].key=="scale"&&def->parameters[5].key=="center_x"&&def->parameters[6].key=="center_y"&&def->parameters[7].key=="size","light controls append without shifting legacy shader slots");
  auto*legacy=obs_data_create();auto*stack=obs_data_create();auto*items=obs_data_array_create();auto*item=obs_data_create();auto*values=obs_data_create();
  obs_data_set_string(item,"id",id);obs_data_set_double(values,"scale",1.7);obs_data_set_obj(item,"parameters",values);obs_data_array_push_back(items,item);obs_data_set_array(stack,"effects",items);obs_data_set_obj(legacy,"fx_stack",stack);
  auto migrated=spatial::readFxStack(legacy);require(migrated.effects.size()==1&&migrated.effects[0].values[4]==1.7&&migrated.effects[0].values[5]==0&&migrated.effects[0].values[6]==0&&migrated.effects[0].values[7]==1,"legacy light settings retain surface scale and original projected lamp center");
  auto&light=migrated.effects[0];light.values[5]=.23;light.values[6]=.79;light.values[7]=.45;spatial::writeFxStack(legacy,migrated);const auto restored=spatial::readFxStack(legacy);
  require(restored.effects.size()==1&&restored.effects[0].values[5]==.23&&restored.effects[0].values[6]==.79&&restored.effects[0].values[7]==.45,"independent light center and size survive scene serialization");
  obs_data_set_double(values,"center_x",-2);obs_data_set_double(values,"center_y",4);obs_data_set_double(values,"size",0);obs_data_set_obj(item,"parameters",values);obs_data_array_erase(items,0);obs_data_array_push_back(items,item);obs_data_set_array(stack,"effects",items);obs_data_set_obj(legacy,"fx_stack",stack);const auto clamped=spatial::readFxStack(legacy);
  require(clamped.effects[0].values[5]==-1&&clamped.effects[0].values[6]==1&&clamped.effects[0].values[7]==.01,"light center offsets clamp to signed UV range and size stays positive");
  obs_data_release(values);obs_data_release(item);obs_data_array_release(items);obs_data_release(stack);obs_data_release(legacy);
 }
}
static void quality341(){
 spatial::FxStack stack;stack.attached=true;stack.groups[1]=true;
 for(const char*id:{"FX02.2","FX02.3","FX02.4"}){
  const auto*d=spatial::fxDefinition(id);require(d&&d->group==2,"v341 quality effects registered under quality correction");
  auto f=spatial::makeFx(*d);require(!f.enabled&&!spatial::activeFx(stack,*d),"new quality effects default off");
  f.enabled=true;for(size_t j=0;j<d->parameters.size();++j)f.values[j]=d->parameters[j].maximum;stack.effects.push_back(f);
 }
 auto*data=obs_data_create();spatial::writeFxStack(data,stack);const auto restored=spatial::readFxStack(data);obs_data_release(data);
 require(restored.effects.size()==3,"all new quality settings roundtrip");
 for(const auto&f:restored.effects){const auto*d=spatial::fxDefinition(f.id);for(size_t j=0;j<d->parameters.size();++j)require(f.values[j]==d->parameters[j].maximum,"new quality parameters retain stable names and precise values");}
 require(!spatial::fxDefinition("FX02.5"),"deferred Native AA is not advertised");
 require(spatial::activeFx(restored,*spatial::fxDefinition("FX02.2"))&&!spatial::activeFx(restored,*spatial::fxDefinition("FX02.3")),"imported conflicting AA resolves to SMAA without stacking");
 spatial::FxStack complete;for(const auto&d:spatial::fxCatalog())complete.effects.push_back(spatial::makeFx(d));
 data=obs_data_create();spatial::writeFxStack(data,complete);auto all=spatial::readFxStack(data);obs_data_release(data);
 require(all.effects.size()==spatial::fxCatalog().size()&&spatial::findFx(all,"FX13.3"),"extended catalog persistence never truncates effects at old92 limit");
 auto*invalid=obs_data_create_from_json(R"({"fx_stack":{"effects":[{"id":"FX02.2","parameters":{"quality":20,"threshold":-9}},{"id":"FX02.3","parameters":{"threshold":90,"subpixel":-5}},{"id":"FX02.4","parameters":{"amount":40,"denoise":-8}}]}})");
 auto clamped=spatial::readFxStack(invalid);obs_data_release(invalid);
 require(clamped.effects.size()==3&&clamped.effects[0].values[0]==1&&clamped.effects[0].values[1]==.02&&clamped.effects[1].values[0]==.5&&clamped.effects[1].values[1]==0&&clamped.effects[2].values[0]==1&&clamped.effects[2].values[1]==0,"new quality controls sanitize imported invalid numeric values");
}
static void patch342ShaderIsolation(){
 const std::string children="float4 fxApply(float2 uv) { float4 c = src(uv); if (effect_id == 0) { return environment_image.Sample(s,uv); } if (effect_id == 1) { if (c.a > 0) { c.rgb *= 2; } return soft_alpha(uv); } if (effect_id == 2) { return c; } return c; }";
 for(int selected=0;selected<3;++selected){std::string common="uniform int effect_id;",body=children;require(spatial::specializeFxShader(common,body,selected,true),"material shader child specialization succeeds");require(body.find("effect_id")==std::string::npos&&common.empty(),"runtime selector eliminated before OBS parsing");require((body.find("environment_image")!=std::string::npos)==(selected==0)&& (body.find("soft_alpha")!=std::string::npos)==(selected==1),"only selected child contributes texture dependencies");require((body.find("c.rgb *= 2")!=std::string::npos)==(selected==1),"nested child braces retained without sibling code");}
 std::string common="uniform int effect_id;",broken="if (effect_id == 0) { return c;";require(!spatial::specializeFxShader(common,broken,0,true),"malformed isolated child rejected");
}
static void patch342Persistence(){
 auto*input=obs_data_create_from_json(R"({"fx_stack":{"attached":true,"group_states":[{},{},{"enabled":true},{"enabled":true},{"enabled":true}],"group_order":[{"group":5},{"group":3},{"group":4}],"effects":[{"id":"FX03.4","parameters":{"input_black":0.12,"input_white":0.85,"output_black":0.05,"output_white":0.95,"gamma":1.35}},{"id":"FX05.1","parameters":{"light_x":-75,"light_y":125,"light_z":300}}]}})");
 auto restored=spatial::readFxStack(input);auto*saved=obs_data_create();spatial::writeFxStack(saved,restored);auto*stack=obs_data_get_obj(saved,"fx_stack");
 auto*order=obs_data_get_array(stack,"group_order");require(obs_data_array_count(order)==3,"enabled group activation order roundtrips");obs_data_array_release(order);
 const auto*levels=spatial::findFx(restored,"FX03.4");const auto*def=spatial::fxDefinition("FX03.4");require(def->parameters.size()==6,"levels exposes channel plus five actual levels controls");
 for(const char*key:{"input_black","input_white","output_black","output_white","gamma"})parameterIndex(*def,key);
 require(levels->values[parameterIndex(*def,"gamma")]==1.35,"levels gamma retains exact value");
 const auto*back=spatial::findFx(restored,"FX05.1");def=spatial::fxDefinition("FX05.1");require(back->values[parameterIndex(*def,"light_x")]==-75&&back->values[parameterIndex(*def,"light_y")]==125&&back->values[parameterIndex(*def,"light_z")]==300,"backlight independent XYZ roundtrips");
 obs_data_release(stack);obs_data_release(saved);obs_data_release(input);
 input=obs_data_create_from_json(R"({"fx_stack":{"effects":[{"id":"FX03.4","parameters":{"channel":2,"black":0.1,"shadow":0.3,"middle":0.6,"highlight":0.8,"white":0.9}},{"id":"FX05.1","parameters":{"radius":91.14,"offset":12,"gain":1.5,"scale":2}}]}})");
 restored=spatial::readFxStack(input);levels=spatial::findFx(restored,"FX03.4");def=spatial::fxDefinition("FX03.4");
 require(levels->values[parameterIndex(*def,"output_black")]==.1&&levels->values[parameterIndex(*def,"output_white")]==.9&&levels->values[parameterIndex(*def,"channel")]==2,"old curve endpoints and channel migrate to levels");
 back=spatial::findFx(restored,"FX05.1");def=spatial::fxDefinition("FX05.1");require(back->values[parameterIndex(*def,"radius")]==91.14&&back->values[parameterIndex(*def,"gain")]==1.5&&back->values[parameterIndex(*def,"scale")]==2,"old backlight shared parameters retained");obs_data_release(input);
}
int main(){patch342ShaderIsolation();patch342Persistence();quality341();
 for(const char*id:{"FX05.1","FX05.2","FX05.3"}){
  const auto*def=spatial::fxDefinition(id);require(def&&def->color,"backlight candle and spotlight expose their own color");
  spatial::FxStack stack;auto light=spatial::makeFx(*def);light.color={.12,.34,.56};stack.effects.push_back(light);
  auto*data=obs_data_create();spatial::writeFxStack(data,stack);const auto restored=spatial::readFxStack(data);obs_data_release(data);
  require(restored.effects.size()==1&&restored.effects[0].color.x==.12&&restored.effects[0].color.y==.34&&restored.effects[0].color.z==.56,"local effect color survives scene serialization");
 }
 auto*legacy=obs_data_create_from_json(R"({"layers":[{"uuid":"legacy-source","instance_id":""},{"uuid":"legacy-source","instance_id":""},{"uuid":"another-source","instance_id":"explicit"},{"uuid":"another-source","instance_id":"explicit"}]})");auto migrated=spatial::readModel(legacy);obs_data_release(legacy);std::set<std::string>migratedIds;for(auto&l:migrated.layers)require(!l.instanceId.empty()&&migratedIds.insert(l.instanceId).second,"legacy and duplicated instance IDs become unique");auto*legacySaved=spatial::modelData(migrated);auto legacyRound=spatial::readModel(legacySaved);obs_data_release(legacySaved);for(size_t i=0;i<migrated.layers.size();++i)require(migrated.layers[i].instanceId==legacyRound.layers[i].instanceId,"migrated instance IDs remain stable after save/load");
 sixColorPersistence();
 localLightControlPersistence();
 std::set<std::string>ids;std::array<int,13>counts{};
 require(spatial::fxCatalog().size()==95,"all 95 confirmed suboptions registered");
 for(const auto&def:spatial::fxCatalog()){
  require(ids.insert(def.id).second,"effect identity is unique");require(def.group>=1&&def.group<=13,"effect group is valid");++counts[size_t(def.group-1)];require(def.parameters.size()<=32,"shader parameter packing fits");
  for(auto&p:def.parameters)require(std::isfinite(p.initial)&&p.initial>=p.minimum&&p.initial<=p.maximum&&!p.help.empty(),"parameter defaults and hover explanation valid");
 }
 for(int n:counts)require(n>0,"every top-level group populated");
 auto*catalog=obs_data_array_create();
 for(auto&d:spatial::fxCatalog()){
  auto*r=obs_data_create();obs_data_set_string(r,"id",d.id.c_str());obs_data_set_string(r,"name",d.name.c_str());obs_data_set_int(r,"group",d.group);obs_data_set_bool(r,"history",d.history);obs_data_set_bool(r,"geometry",d.geometry);obs_data_set_bool(r,"stage",d.stage);obs_data_set_bool(r,"color",d.color);obs_data_set_bool(r,"color2",d.color2);obs_data_set_int(r,"asset",int(d.asset));obs_data_set_bool(r,"source",d.asset==spatial::FxAsset::Source);obs_data_set_bool(r,"reference",d.asset==spatial::FxAsset::Source||d.asset==spatial::FxAsset::Image);
  auto*p=obs_data_create();auto*limits=obs_data_create();auto*modes=obs_data_array_create();
  for(auto&meta:d.parameters){
   obs_data_set_double(p,meta.key.c_str(),meta.initial);auto*limit=obs_data_create();obs_data_set_double(limit,"min",meta.minimum);obs_data_set_double(limit,"max",meta.maximum);obs_data_set_double(limit,"initial",meta.initial);obs_data_set_obj(limits,meta.key.c_str(),limit);obs_data_release(limit);
   if(!meta.choices.empty()){auto*mode=obs_data_create();obs_data_set_string(mode,"key",meta.key.c_str());obs_data_set_int(mode,"count",int64_t(meta.choices.size()));obs_data_array_push_back(modes,mode);obs_data_release(mode);}
  }
  obs_data_set_obj(r,"parameters",p);obs_data_set_obj(r,"parameter_limits",limits);obs_data_set_array(r,"modes",modes);obs_data_release(p);obs_data_release(limits);obs_data_array_release(modes);obs_data_array_push_back(catalog,r);obs_data_release(r);
 }
 auto*root=obs_data_create();obs_data_set_array(root,"effects",catalog);std::ofstream("fx-runtime-catalog.json")<<obs_data_get_json(root);obs_data_array_release(catalog);obs_data_release(root);
 spatial::Layer fresh;require(!spatial::hasActiveFx(fresh.fx)&&!fresh.fx.attached,"new scene/layer starts empty with no FX");
 auto*def=spatial::fxDefinition("FX01.0");fresh.fx.effects.push_back(spatial::makeFx(*def));fresh.fx.effects[0].enabled=true;require(!spatial::hasActiveFx(fresh.fx),"child alone cannot activate unattached stack");fresh.fx.attached=true;require(!spatial::hasActiveFx(fresh.fx),"parent group gates enabled child");fresh.fx.groups[0]=true;require(spatial::hasActiveFx(fresh.fx),"attached group+child activates effect");auto copy=fresh;copy.fx.effects[0].strength=0;require(!spatial::hasActiveFx(copy.fx)&&spatial::hasActiveFx(fresh.fx),"same-source duplicate retains independent FX configuration");
 auto*d=obs_data_create_from_json(R"({"schema_version":5,"layers":[{"uuid":"source","instance_id":"one","fx_stack":{"attached":true,"group_states":[{"enabled":true}],"effects":[{"id":"FX01.0","enabled":true,"strength":0.75,"values":[12,0]}]}}]})");
 auto m=spatial::readModel(d);auto*out=spatial::modelData(m);auto*layers=obs_data_get_array(out,"layers");auto*layer=obs_data_array_item(layers,0);auto*stack=obs_data_get_obj(layer,"fx_stack");
 require(stack&&obs_data_get_bool(stack,"attached"),"per-instance FX attachment survives model serialization");
 auto*effects=obs_data_get_array(stack,"effects");require(effects&&obs_data_array_count(effects)==1,"enabled child effect survives save/load");
 auto*effect=obs_data_array_item(effects,0);require(obs_data_get_double(effect,"strength")==.75,"effect strength survives save/load");
 obs_data_release(effect);obs_data_array_release(effects);obs_data_release(stack);obs_data_release(layer);obs_data_array_release(layers);obs_data_release(out);obs_data_release(d);
 auto*bad=obs_data_create_from_json(R"({"layers":[{"uuid":"a","fx_stack":{"attached":true,"effects":[{"id":"FX01.0","strength":200,"parameters":{"radius":-400}},{"id":"FX01.0"},{"id":"unknown"}]}}]})");auto clean=spatial::readModel(bad);require(clean.layers[0].fx.effects.size()==1,"duplicate and unknown effects rejected");require(clean.layers[0].fx.effects[0].strength==1&&clean.layers[0].fx.effects[0].values[0]>=0,"malformed strength and parameter values clamped");obs_data_release(bad);
 std::cout<<"FX model tests complete\n";return 0;
}
