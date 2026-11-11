#pragma once
#include "model.hpp"
#include <algorithm>
namespace spatial {
// References sample raw input, so one pass suffices and reference cycles cannot recurse.
inline std::vector<bool> fxCaptureLayers(const std::vector<Layer>&layers){
 std::vector<bool> required(layers.size(),false);
 for(size_t i=0;i<layers.size();++i){
  const auto&layer=layers[i];
  if(!layer.visible||!layer.enabled)continue;
  required[i]=true;
  for(const auto&definition:fxCatalog())if(const auto*effect=activeFx(layer.fx,definition)){
   if(effect->reference.empty())continue;
   for(size_t j=0;j<layers.size();++j)if(layers[j].instanceId==effect->reference){required[j]=true;break;}
  }
 }
 return required;
}
}
