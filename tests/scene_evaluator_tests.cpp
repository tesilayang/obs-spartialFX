#include "scene_evaluator.hpp"
#include "fx_dependencies.hpp"
#include "shadow_blur.hpp"
#include <chrono>
#include <cstdlib>
#include <cmath>
#include <iostream>
#include <limits>
using namespace spatial;
void check(bool b,const char*s){if(!b){std::cerr<<"FAIL "<<s<<'\n';std::exit(1);}}
int main(){
 Plane shadowReceiver;shadowReceiver.position.z=500;
 auto penumbra=shadowBlurRadius(shadowReceiver,{0,0,0},{0,0,-500},100,2000,1000);
 check(std::abs(penumbra[0]-25.6)<1e-9&&std::abs(penumbra[1]-51.2)<1e-9,"penumbra scales from world into rectangular receiver UV");
 check(shadowBlurRadius(shadowReceiver,{0,0,500},{0,0,-500},100,2000,1000)[0]==0,"contact shadow remains sharp");
 shadowReceiver.scaleX=-2;check(std::abs(shadowBlurRadius(shadowReceiver,{0,0,0},{0,0,-500},100,2000,1000)[0]-12.8)<1e-9,"mirrored receiver uses absolute physical extent");
 check(shadowBlurRadius(shadowReceiver,{0,0,0},{0,0,0},100,2000,1000)[0]==0,"degenerate emitter plane has finite blur");
 StageModel lightPlacement;Layer wall;wall.importWidth=1920;wall.importHeight=1080;wall.plane.position.z=500;lightPlacement.layers={wall};
 auto newLight=newLightPosition(lightPlacement,0);check(std::abs(newLight.x)<1e-9&&std::abs(newLight.y)<1e-9&&newLight.z<500&&newLight.z>-1490,"new light centered ahead of imported layer");
 lightPlacement.layers[0].plane.position={200,-100,0};newLight=newLightPosition(lightPlacement,0);auto lightPixel=project(newLight,lightPlacement.camera,1920,1080),objectPixel=project({200,-100,0},lightPlacement.camera,1920,1080);
 check(std::abs(lightPixel.x-objectPixel.x)<1e-8&&std::abs(lightPixel.y-objectPixel.y)<1e-8,"new light follows projected composition center");
 lightPlacement.camera.position={50,100,-900};lightPlacement.camera.rotation={12,23,7};lightPlacement.layers[0].plane.position=add(lightPlacement.camera.position,rotate({0,0,800},lightPlacement.camera.rotation));lightPlacement.layers[0].plane.rotation=lightPlacement.camera.rotation;
 newLight=newLightPosition(lightPlacement,0);auto local=inverseRotate(sub(newLight,lightPlacement.camera.position),lightPlacement.camera.rotation);
 check(std::abs(local.x)<1e-7&&std::abs(local.y)<1e-7&&local.z<800&&local.z>10,"new light uses moved rotated camera axes");
 lightPlacement=StageModel{};lightPlacement.layers={wall};auto&near=lightPlacement.layers.emplace_back(wall);near.plane.position.z=-1000;near.visible=false;
 check(newLightPosition(lightPlacement,0).z>0,"hidden foreground excluded from new light depth");near.visible=true;near.opacity=0;check(newLightPosition(lightPlacement,0).z>0,"transparent foreground excluded from new light depth");near.opacity=1;near.enabled=false;check(newLightPosition(lightPlacement,0).z>0,"disabled foreground excluded from new light depth");near.enabled=true;
 check(newLightPosition(lightPlacement,0).z<-1000,"nearest duplicate instance determines new light depth");
 near.animation["z"].enabled=true;near.animation["z"].put({0,-1000});near.animation["z"].put({toTime(2),-500});check(newLightPosition(lightPlacement,toTime(2)).z>-1000&&newLightPosition(lightPlacement,toTime(2)).z<-500,"new light uses current animated depth");
 near.animation.clear();near.perspective=.5;check(newLightPosition(lightPlacement,0).z>-1000&&newLightPosition(lightPlacement,0).z<-500,"new light uses effective perspective depth");
 lightPlacement.layers={wall};lightPlacement.layers[0].plane.rotation.y=35;lightPlacement.layers[0].plane.scaleX=-1;newLight=newLightPosition(lightPlacement,0);
 for(const auto&corner:corners(lightPlacement.layers[0].plane,1920,1080))check(newLight.z<corner.z,"new light precedes tilted mirrored corners");
 lightPlacement.layers[0].cropLeft=700;lightPlacement.layers[0].cropRight=700;check(newLightPosition(lightPlacement,0).z>newLight.z,"cropped bounds drive nearest visible depth");
 lightPlacement.layers[0].cropLeft=0;lightPlacement.layers[0].cropRight=0;auto&breathingFx=lightPlacement.layers[0].fx;breathingFx.attached=true;breathingFx.groups[10]=true;auto breathingEffect=makeFx(*fxDefinition("FX11.3"));breathingEffect.enabled=true;breathingEffect.values[0]=.5;breathingEffect.values[1]=1;breathingEffect.values[2]=0;breathingEffect.values[3]=0;breathingFx.effects.push_back(breathingEffect);
 check(newLightPosition(lightPlacement,0,{},false,.25).z<newLightPosition(lightPlacement,0,{},false,0).z,"new light follows live FX clock while timeline paused");
 lightPlacement.layers[0].fx={};lightPlacement.layers[0].followSourceSize=true;lightPlacement.layers[0].importTransform.boundsType=3;lightPlacement.layers[0].importTransform.boundsWidth=800;lightPlacement.layers[0].importTransform.boundsHeight=500;lightPlacement.layers[0].importTransform.cropToBounds=true;
 for(const SourceSize size:std::vector<SourceSize>{{640,360},{1920,1080},{1080,1920}}){
  const auto e=evaluateSceneAt(lightPlacement,0,{size});const auto crop=sourceLayout(size.width,size.height,lightPlacement.layers[0].importTransform);newLight=newLightPosition(lightPlacement,0,{size});
  for(const auto&corner:corners(e.world[0].plane,crop.width,crop.height))check(newLight.z<corner.z,"source growth bounds and crop keep new light in front");
 }
 lightPlacement.layers[0].followSourceSize=false;lightPlacement.layers[0].boundsType=3;lightPlacement.layers[0].boundsWidth=500;lightPlacement.layers[0].boundsHeight=300;
 const std::vector<SourceSize> boundedSizes{{1920,1080}};const auto bounded=evaluateSceneAt(lightPlacement,0,boundedSizes);newLight=newLightPosition(lightPlacement,0,boundedSizes);
 for(const auto&corner:corners(bounded.world[0].plane,1920,1080))check(newLight.z<corner.z,"non-follow bounds share renderer world plane");
 lightPlacement.layers.clear();newLight=newLightPosition(lightPlacement,0);check(newLight.x==0&&newLight.y==0&&newLight.z==-1000,"empty scene light has visible camera-relative fallback");
 wall.plane.position.z=-1495;lightPlacement.layers={wall};check(newLightPosition(lightPlacement,0).z==-1000,"near clipped layer excluded safely");
 wall.plane.position.z=std::numeric_limits<double>::quiet_NaN();lightPlacement.layers={wall};newLight=newLightPosition(lightPlacement,0);check(std::isfinite(newLight.x)&&std::isfinite(newLight.y)&&std::isfinite(newLight.z),"invalid plane cannot create invalid light");
 StageModel dependencies;Layer consumer,hidden;consumer.instanceId="consumer";hidden.instanceId="hidden";hidden.visible=false;hidden.enabled=false;hidden.opacity=0;dependencies.layers={consumer,hidden};
 auto needed=fxCaptureLayers(dependencies.layers);check(needed[0]&&!needed[1],"unused hidden input needs no FX capture");
 auto&stack=dependencies.layers[0].fx;stack.attached=true;stack.groups[11]=true;auto mask=makeFx(*fxDefinition("FX12.2"));mask.enabled=true;mask.reference="hidden";stack.effects.push_back(mask);
 needed=fxCaptureLayers(dependencies.layers);check(needed[0]&&needed[1],"active FX captures hidden disabled zero-opacity reference");stack.groups[11]=false;needed=fxCaptureLayers(dependencies.layers);check(!needed[1],"disabled category releases auxiliary activation requirement");
 dependencies.light.shadowColor={.12,.34,.56};auto*lightData=modelData(dependencies);auto lightRound=readModel(lightData);obs_data_release(lightData);check(lightRound.light.shadowColor.x==.12&&lightRound.light.shadowColor.y==.34&&lightRound.light.shadowColor.z==.56,"independent shadow color persists");
 StageModel breathing;Layer breathe;breathe.instanceId="breathe";breathe.fx.attached=true;breathe.fx.groups[10]=true;auto pulse=makeFx(*fxDefinition("FX11.3"));pulse.enabled=true;pulse.values[0]=.2;pulse.values[1]=1;pulse.values[2]=0;pulse.values[3]=0;breathe.fx.effects.push_back(pulse);breathing.layers.push_back(breathe);breathing.light.bindUuid=breathe.sourceUuid;breathing.light.bindInstanceId="breathe";breathing.light.position={10,0,0};const auto pulseAt=evaluateSceneAt(breathing,0,{},false,.25);check(std::abs(pulseAt.authoring[0].plane.scaleX-1.2)<1e-9,"FX geometry uses shared evaluated plane");
 StageModel twins;Layer twin;twin.sourceUuid="shared";twin.sceneUuid="scene";twin.itemId=4;twin.instanceId="first";twins.layers.push_back(twin);twin.instanceId="second";twin.instanceNumber=2;twins.layers.push_back(twin);twins.layers[1].animation["x"].enabled=true;twins.layers[1].animation["x"].put({0,200});twins.layers[1].animation["x"].put({toTime(2),600});twins.light.bindUuid="shared";twins.light.bindSceneUuid="scene";twins.light.bindItemId=4;twins.light.bindInstanceId="second";twins.light.position={10,0,0};auto twinsAt=evaluateSceneAt(twins,toTime(1));check(twinsAt.authoring[0].plane.position.x==0&&twinsAt.authoring[1].plane.position.x==400,"same item instances independently evaluate motion");check(std::abs(twinsAt.lights[0].x-410)<1e-9,"bound light follows chosen duplicate rather than first instance");
 StageModel m;Layer l;l.sourceUuid="duplicate";l.sceneUuid="scene";l.itemId=12;l.plane.position={100,200,300};l.plane.rotation={10,20,30};l.plane.scaleX=1.2;l.perspective=.5;m.layers.push_back(l);l.itemId=13;l.plane.position.x=800;m.layers.push_back(l);
 m.light.bindUuid="duplicate";m.light.bindSceneUuid="scene";m.light.bindItemId=12;m.light.position={10,20,30};
 auto&c=m.layers[0].animation["x"];c.enabled=true;c.put({0,100});c.put({toTime(2),500});auto e=evaluateSceneAt(m,toTime(1));
 check(e.authoring[0].plane.position.x==300&&m.layers[0].plane.position.x==100,"evaluated position never overwrites base");
 const double factor=(1500+300*.5)/1800;check(std::abs(e.world[0].plane.position.x-300*factor)<1e-9,"reference perspective compensation unchanged");auto expected=bindPosition(m.light.position,e.world[0].plane);check(std::abs(expected.x-e.lights[0].x)<1e-9,"light exact binding uses evaluated world pose");
 m.layers[0].orbit.enabled=true;m.layers[0].orbit.radius=200;m.layers[0].animation["yaw"].enabled=true;m.layers[0].animation["yaw"].put({0,0});m.layers[0].animation["yaw"].put({toTime(2),360});e=evaluateSceneAt(m,toTime(1));check(std::abs(e.authoring[0].plane.position.z-200)<1e-7,"orbit preserves full turn and changes center");
 Curve dense;dense.enabled=true;for(int i=0;i<100000;++i)dense.put({toTime(i),double(i)});m.cameraAnimation["x"]=dense;auto*d=modelData(m);auto round=readModel(d);obs_data_release(d);check(round.cameraAnimation["x"].keys().size()==100000,"100000 keys persist without truncation");check(contentEnd(round)==toTime(99999),"derived end exceeds24h");
 auto start=std::chrono::steady_clock::now();double sum=0;for(int i=0;i<100000;++i)sum+=dense.value(toTime(i*.875),0);auto ns=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();check(sum>0,"dense lookup executed");std::cout<<"100000 binary evaluations: "<<ns<<"us\n";
 auto*current=obs_data_create_from_json(R"({"schema_version":4,"legacy_playback_end":258.72,"use_legacy_playback_range":true,"animation":[{"parameter":"x","enabled":true,"keys":[{"time_us":"0","value":0,"mode":0},{"time_us":"6000000","value":120,"mode":0}]}]})");
 auto currentModel=readModel(current);obs_data_release(current);check(playbackEnd(currentModel)==6,"content endpoint alone controls playback, obsolete range ignored");check(evaluateSceneAt(currentModel,toTime(3)).camera.position.x==60,"existing v3.2 schema4 track retains evaluated motion");
 auto*saved=modelData(currentModel);auto restored=readModel(saved);obs_data_release(saved);check(restored.cameraAnimation["x"].keys().size()==2&&playbackEnd(restored)==6,"current tracks retain exact keys after revised save");
 m.outputOpacity=.5;m.fx.enabled=false;e=evaluateSceneAt(m,0);check(e.outputOpacity==.5&&!m.fx.enabled,"composite animation independent of FX");std::cout<<"scene evaluator checks passed\n";
}
