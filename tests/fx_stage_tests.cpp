#include "fx_stage.hpp"
#include <cstdlib>
#include <iostream>
using namespace spatial;
static void check(bool value,const char*message){if(!value){std::cerr<<"FAIL "<<message<<'\n';std::exit(1);}std::cout<<"PASS "<<message<<'\n';}
int main(){
 StageModel m;check(!stageWidescreen(m).enabled,"empty stage has no mask");
 Layer l;l.instanceId="one";l.fx.attached=true;l.fx.groups[12]=true;
 auto f=makeFx(*fxDefinition("FX13.1"));f.enabled=true;l.fx.effects.push_back(f);m.layers.push_back(l);
 auto s=stageWidescreen(m);check(s.enabled&&!s.transparent&&s.amount==1,"active wide stage defaults to black bars");
 check(std::abs(s.visibleFraction-16.0/9*9/21)<1e-9,"1920x1080 visible region is exactly21by9");
 m.layers[0].fx.effects[0].values[0]=1;s=stageWidescreen(m);check(s.transparent,"crop mode requests transparent outside");
 m.layers[0].visible=false;check(!stageWidescreen(m).enabled,"hidden layer cannot activate stage mask");m.layers[0].visible=true;
 m.layers.push_back(m.layers[0]);m.layers[1].fx.effects[0].values[0]=0;check(stageWidescreen(m).transparent,"first active timeline layer controls stage conflict");
 m.layers[0].fx.groups[12]=false;check(!stageWidescreen(m).transparent,"disabled parent yields next active stage configuration");
 m.width=3840;m.height=1080;check(stageWidescreen(m).visibleFraction==1,"already wider canvas needs no negative bars");
 m.layers.resize(1);m.layers[0].fx.groups[12]=true;m.layers[0].opacity=0;
 auto evaluated=evaluateSceneAt(m,0);evaluated.authoring[0].opacity=1;check(stageWidescreen(m,&evaluated).enabled,"evaluated opacity activates animated visible layer");
 evaluated.authoring[0].opacity=0;m.layers[0].opacity=1;check(!stageWidescreen(m,&evaluated).enabled,"evaluated opacity disables animated invisible layer");
 std::cout<<"stage mask policy passed\n";
}
