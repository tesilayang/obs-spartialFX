#include "animation.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
using namespace spatial;
void check(bool b,const char*s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
int main(){
 Curve c; c.enabled=true; c.put({0,0});c.put({toTime(100000),100});
 check(std::abs(c.value(toTime(90000),0)-90)<1e-6,"long time linear");
 for(int n=1;n<1000;++n)c.put({toTime(n),double(n)});
 check(c.keys().size()==1001,"no 256 key truncation");
 Curve b;b.enabled=true;AnimKey a{0,0};a.interpolation=Interpolation::Bezier;a.outX=.2;a.outY=.7;
 AnimKey z{toTime(2),10};z.inX=.8;z.inY=.9;b.put(a);b.put(z);
 const double before=b.value(toTime(.71),0);b.split(toTime(.9),0);
 check(std::abs(before-b.value(toTime(.71),0))<1e-6,"Bezier split preserves path");
 check(b.value(0,-1)==0&&b.value(toTime(2),-1)==10,"Bezier endpoints");
 Curve overshoot;overshoot.enabled=true;AnimKey os{0,0};os.interpolation=Interpolation::Bezier;os.outY=-1.0/3;AnimKey oe{toTime(2),10};oe.inY=0;overshoot.put(os);overshoot.put(oe);double dip=overshoot.value(toTime(.5),0);overshoot.split(toTime(1),0);check(std::abs(dip-overshoot.value(toTime(.5),0))<1e-6,"zero-delta overshoot split exact");
 Curve legacy;legacy.enabled=true;AnimKey lk{0,-20};lk.interpolation=Interpolation::LegacyCubic;legacy.put(lk);legacy.put({2000001,300});Curve beforeLegacy=legacy;for(TimeUs t:{123456LL,1000001LL,1754321LL})legacy.split(t,0);for(TimeUs t=0;t<=2000001;t+=137)check(std::abs(legacy.value(t,0)-beforeLegacy.value(t,0))<1e-7,"piecewise legacy cubic split exact with odd microseconds");
 for(auto mode:{Interpolation::Linear,Interpolation::EaseIn,Interpolation::EaseOut,Interpolation::EaseBoth,Interpolation::Hold}){Curve track;track.enabled=true;AnimKey a{0,30};a.interpolation=mode;a.outAbsolute=true;a.outY=800;AnimKey b{toTime(2),100};b.inAbsolute=true;b.inY=-500;track.put(a);track.put(b);auto before=track;track.split(toTime(.7),0);for(double t:{.1,.4,.9,1.6,1.9})check(std::abs(track.value(toTime(t),0)-before.value(toTime(t),0))<1e-7,"all eased subdivisions ignore stale absolute handles");}
 Curve turns;turns.enabled=true;turns.put({0,0});turns.put({toTime(2),720});check(turns.value(toTime(1),0)==360,"full turns preserved");
 for(auto mode:{Interpolation::Linear,Interpolation::EaseIn,Interpolation::EaseOut,Interpolation::EaseBoth}){
  Curve angular;angular.enabled=angular.shortest=true;AnimKey a{0,350},b{toTime(2),10};a.interpolation=mode;angular.put(a);angular.put(b);auto before=angular;materializeHandles(a,b,true);a.interpolation=Interpolation::Bezier;angular.put(a);angular.put(b);
  for(double t:{.1,.4,.9,1.6,1.9})check(std::abs(angular.value(toTime(t),0)-before.value(toTime(t),0))<1e-7,"shortest rotation materialization preserves interpolation");
 }
 Curve copy=turns;copy.put({toTime(1),20});check(turns.keys().size()==2,"immutable copy isolates edits");
 check(toTime(90000.123456)==90000123456LL,"microseconds exact");
 std::cout<<"animation checks passed\n";
}
