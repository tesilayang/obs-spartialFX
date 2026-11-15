#include "fx_renderer.hpp"
#include <cstdlib>
#include <iostream>
using namespace spatial;
void check(bool value,const char*label){if(!value){std::cerr<<"FAIL "<<label<<'\n';std::exit(1);}std::cout<<"PASS "<<label<<'\n';}
int main(){
 FxPlaybackClock c;auto a=c.update(1,0,5,false,false,1,0,0);
 auto b=c.update(16666668,.016666667,5,false,false,1,0,0);check(b.elapsed>.016&&b.epoch==a.epoch&&b.advance,"wall baseline advances without reset");
 a=c.update(33333335,.033333334,2,true,false,1,0,2);check(a.elapsed==2&&a.epoch>b.epoch,"preview selects timeline and resets once");
 b=c.update(50000002,.05,2,true,false,1,0,2);check(b.epoch==a.epoch&&!b.advance,"paused preview does not clear or advance each frame");
 a=c.update(66666669,.066,3,true,false,1,0,3);check(a.elapsed==3&&a.epoch>b.epoch,"paused seek clears history");
 b=c.update(83333336,.083,3,true,true,1,1,3);check(b.epoch>a.epoch,"play restart token clears history");
 a=c.update(100000003,.1,3.016666667,true,true,1,1,3);check(a.epoch==b.epoch&&a.advance,"continuous playback retains history");
 b=c.update(116666670,.116,3.033333334,true,true,1,1,3);check(b.epoch==a.epoch,"next ordinary playback frame retains history");
 a=c.update(133333337,.133,.01,true,true,1,1,3);check(a.epoch>b.epoch,"loop or backwards seek invalidates history");
 b=c.update(150000004,.15,6,true,true,1,1,3);check(b.epoch>a.epoch,"forward jump invalidates history");
 a=c.update(166666671,.166,6.016666667,true,true,1,1,3);check(a.epoch==b.epoch,"history stays allocated through recovery frame");
 b=c.update(166666671,.166,100,true,true,1,1,100);check(b.elapsed==a.elapsed&&b.epoch==a.epoch,"repeated draw in same video frame shares clock decision");
 a=c.update(183333338,.183,6.05,true,true,2,1,3);check(a.epoch==b.epoch,"continuous speed change is not a seek");
 b=c.update(200000005,.2,6.05,false,false,1,1,3);check(b.elapsed==.2&&b.epoch>a.epoch,"return to wall domain invalidates once");
}
