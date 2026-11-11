#pragma once
#include "scene_evaluator.hpp"
#include <algorithm>
namespace spatial {
struct StageWidescreen {bool enabled=false,transparent=false;double amount=1,visibleFraction=1;};
inline StageWidescreen stageWidescreen(const StageModel&m,const EvaluatedScene*evaluated=nullptr){
 const auto*def=fxDefinition("FX13.1");if(!def)return {};
 for(size_t i=0;i<m.layers.size();++i){const auto&layer=m.layers[i];
  const double opacity=evaluated&&i<evaluated->authoring.size()?evaluated->authoring[i].opacity:layer.opacity;
  if(!layer.enabled||!layer.visible||opacity<=0)continue;
  if(const auto*f=activeFx(layer.fx,*def))return {true,f->values[0]>.5,f->strength,std::clamp(double(m.width)*9.0/(21.0*std::max(1u,m.height)),0.0,1.0)};
 }
 return {};
}
}
