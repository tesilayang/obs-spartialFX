#pragma once
#include "fx_catalog.hpp"
#include <algorithm>
#include <cmath>

namespace spatial {
// These layer-local optical composites run after surface processing. They do
// not provide occluders or lights to the world-space lighting solver.
inline bool isLocalOptic(const FxDefinition& d) {
 return d.id=="FX04.0"||d.id=="FX04.1"||d.id=="FX05.5";
}
inline unsigned opticalPadding(const FxStack& stack) {
 double radius=0;
 for(const auto& d:fxCatalog())if(auto* f=activeFx(stack,d)) {
  if(d.id=="FX04.0"&&f->values[3]>0)radius+=f->values[1];
  if(d.id=="FX04.1"&&f->values[4]>0)radius+=f->values[0]+2;
  if(d.id=="FX05.5"&&f->values[3]>0)
   radius+=f->values[1]+f->values[2]+f->values[5];
 }
 // Quantized extents limit reallocations while dragging small slider changes.
 return radius>0?unsigned(std::ceil((std::min(radius,1024.)+2)/32))*32:0;
}
inline int starRayCount(int choice) {
 constexpr int rays[]={2,4,5,6};return rays[std::clamp(choice,0,3)];
}
}
