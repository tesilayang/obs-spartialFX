#pragma once
#include "core.hpp"
#include <obs.h>
#include <array>
#include <memory>
#include <string>
#include <vector>
namespace spatial {
struct FxParameter {std::string key,label,help;double initial,minimum,maximum;std::vector<std::string> choices;};
enum class FxAsset {None,Source,Image,Lut};
struct FxDefinition {
 std::string id,name,help;int group=1,shader=0;std::vector<FxParameter> parameters;
 FxAsset asset=FxAsset::None;bool color=false,color2=false,history=false,geometry=false,stage=false;
};
struct FxPixels {uint32_t width=0,height=0;std::vector<uint8_t> pixels;int lutSize=0;Vec3 minimum{},maximum{1,1,1};};
struct FxInstance {
 std::string id;bool enabled=false;double strength=1;std::array<double,32> values{};
 Vec3 color{1,.85,.6},color2{.1,.2,.4};std::string reference,path,assetError;
 std::shared_ptr<const FxPixels> pixels;
};
struct FxStack {bool attached=false;std::array<bool,13> groups{};std::vector<FxInstance> effects;};
struct FxGroup {const char*name;const char*help;};
const std::array<FxGroup,13>&fxGroups();
const std::vector<FxDefinition>&fxCatalog();
const FxDefinition*fxDefinition(const std::string&);
FxInstance makeFx(const FxDefinition&);
FxInstance*findFx(FxStack&,const std::string&);
const FxInstance*findFx(const FxStack&,const std::string&);
const FxInstance*activeFx(const FxStack&,const FxDefinition&);
bool hasActiveFx(const FxStack&);
FxStack readFxStack(obs_data_t*);
void writeFxStack(obs_data_t*,const FxStack&);
void loadFxAsset(FxInstance&,FxAsset);
}
