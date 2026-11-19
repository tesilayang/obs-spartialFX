// Scene-aware preset movement. Called only while compiling an editable preset.
static std::vector<MotionChannel> stageMotion(const StageModel &model, const EvaluatedScene &scene,
                                             size_t slot, const ComposeRequest &request,
                                             const std::string &partId, double direction,
                                             size_t ordinal, size_t count, bool retreat)
{
 auto profile = motion(partId, request.strength, direction, ordinal, count);
 const auto &layer = model.layers[slot];
 const Plane start = scene.authoring[slot].plane;
 const Plane world = scene.world[slot].plane;
 const auto &camera = scene.camera;
 const auto center = project(world.position, camera, model.width, model.height);
 const bool pureFade = partId == "M25" || partId == "M44";
 if (pureFade) return profile;
 const bool primaryDepth = partId == "M02" || partId == "M34";
 const bool stepped = partId == "M34" || partId == "M35";
 auto envelope = [&](double u) {
  if (partId == "M34") return std::floor(std::min(u, 1-u)*8 + .00001)/4.;
  if (partId == "M33") return u < .45 ? std::sin(pi*u/.45) : 0.;
  if (partId == "M35") return u < .12 || u > .88 ? 0. : u < .38 ? .45 : 1.;
  return .5 - .5*std::cos(2*pi*u);
 };
 if (!request.spatialMotion && primaryDepth) {
  // Simulate a push on a plane without invalidating a user's existing Z track.
  profile = {channel("sx", .14*request.strength, envelope, stepped),
             channel("sy", .14*request.strength, envelope, stepped)};
 }
 if (!center.visible || !std::isfinite(layer.perspective)) return request.spatialMotion ? std::vector<MotionChannel>{} : profile;
 const auto distance = center.depth;
 const double depthFraction = std::min(.18, request.strength*(primaryDepth ? .14 : .095));
 // A common world-space dolly produces naturally decreasing parallax with distance.
 // The depth cap also prevents extreme travel for very distant background cards.
 const double desiredDepth = std::min(distance, 1800.)*depthFraction*(retreat && !primaryDepth ? 1. : -1.);
 const double focal = model.height / (2*std::tan(camera.fov*pi/360));
 const double lateral = std::min(distance, 1600.) / std::max(1., focal) * model.height/1080.;
 std::array<double,65> dx{},dy{},dz{};
 std::vector<MotionChannel> result;
 for(auto &ch:profile) {
  if(ch.parameter!="x" && ch.parameter!="y" && ch.parameter!="z") {result.push_back(std::move(ch)); continue;}
  if(ch.parameter=="z") continue;
  auto &axis=ch.parameter=="x" ? dx : dy;
  for(size_t k=0;k<axis.size();++k) axis[k]=ch.amplitude*ch.samples[k]*lateral*1.35;
 }
 for(size_t k=0;k<dz.size();++k) dz[k]=request.spatialMotion ? desiredDepth*envelope(double(k)/64.) : 0.;
 const auto startValue=[&](const char *key) {
  const auto &anim=layer.animation; auto it=anim.find(key); const auto base=baseValue(model,int(slot),key);
  return it==anim.end()?base:it->second.value(request.start,base);
 };
 const double initialRadius=startValue("radius"), yaw=startValue("yaw"), pitch=startValue("pitch");
 const Vec3 anchorVector=rotate({0,0,-initialRadius},{pitch,yaw,0});
 // The depth band is shared symmetrically with neighboring layers. Coplanar
 // copies have no prior front/back order in physical depth and impose no band.
 double nearLimit=10.,farLimit=std::numeric_limits<double>::infinity();
 for(size_t j=0;j<model.layers.size();++j) {
  if(j==slot || !model.layers[j].visible || !model.layers[j].enabled || scene.world[j].opacity<=0) continue;
  const double z=inverseRotate(sub(scene.world[j].plane.position,camera.position),camera.rotation).z;
  if(z<=10) continue;
  if(z<distance-.01) nearLimit=std::max(nearLimit,distance-.47*(distance-z));
  if(z>distance+.01) farLimit=std::min(farLimit,distance+.47*(z-distance));
 }
 const double width=std::max(1.,layer.importWidth-layer.cropLeft-layer.cropRight);
 const double height=std::max(1.,layer.importHeight-layer.cropTop-layer.cropBottom);
 const auto baselineCorners=corners(world,width,height);
 // Estimate the visible subject extent at its import size; transparent window
 // sources can fill the canvas, so do not treat the entire texture as opaque.
 double minX=center.x,maxX=center.x,minY=center.y,maxY=center.y;
 for(auto v:baselineCorners){auto p=project(v,camera,model.width,model.height);if(p.visible){minX=std::min(minX,p.x);maxX=std::max(maxX,p.x);minY=std::min(minY,p.y);maxY=std::max(maxY,p.y);}}
 Layer projection; projection.perspective=layer.perspective;projection.plane=start;
 const auto original=effectivePlane(projection);
 std::array<bool,4> cornerVisible{};
 for(size_t j=0;j<4;++j)cornerVisible[j]=project(baselineCorners[j],camera,model.width,model.height).visible;
 std::array<Vec3,65> offsets{};
 double amplitude=1.;
 for(unsigned attempt=0;attempt<14;++attempt){
  bool safe=true;
  for(size_t k=0;k<offsets.size();++k){
   Vec3 delta=rotate({dx[k]*amplitude,dy[k]*amplitude,dz[k]*amplitude},camera.rotation);
   // Flat mode preserves explicitly authored world depth even for a turned camera.
   if(!request.spatialMotion)delta.z=0;
   projection.plane=start;projection.plane.position=add(start.position,delta);
   for(const auto &ch:result){const double v=ch.amplitude*ch.samples[k];if(ch.parameter=="sx")projection.plane.scaleX*=1+v;else if(ch.parameter=="sy")projection.plane.scaleY*=1+v;else if(ch.parameter=="rz")projection.plane.rotation.z+=v;}
   auto trial=effectivePlane(projection);
   // Keep source-layout corrections already present in the evaluated scene.
   trial.position=add(world.position,sub(trial.position,original.position));
   if(std::abs(original.scaleX)>1e-8)trial.scaleX*=world.scaleX/original.scaleX;
   if(std::abs(original.scaleY)>1e-8)trial.scaleY*=world.scaleY/original.scaleY;
   auto p=project(trial.position,camera,model.width,model.height);
   // An existing off-center crop may move inward; do not push its center farther outside.
   const double mx=std::min<double>(model.width*.06,(maxX-minX)*.25),my=std::min<double>(model.height*.06,(maxY-minY)*.25);
   if(!p.visible||p.depth<nearLimit||p.depth>farLimit||p.x<std::min(center.x,mx)-.01||p.x>std::max(center.x,model.width-mx)+.01||p.y<std::min(center.y,my)-.01||p.y>std::max(center.y,model.height-my)+.01){safe=false;break;}
   const auto points=corners(trial,width,height);
   for(size_t j=0;j<points.size();++j){if(cornerVisible[j]&&!project(points[j],camera,model.width,model.height).visible){safe=false;break;}}
   if(!safe)break;
   if(layer.orbit.enabled){
    auto v=add(anchorVector,delta);const double radius=std::hypot(std::hypot(v.x,v.z),v.y);
    if(radius<.01){safe=false;break;}
    double nextYaw=std::atan2(-v.x,-v.z)*180/pi,nextPitch=std::atan2(v.y,std::hypot(v.x,v.z))*180/pi;
    if(std::cos(pitch*pi/180)<0){nextYaw+=180.;nextPitch=180.-nextPitch;}
    offsets[k]={std::remainder(nextYaw-yaw,360.),std::remainder(nextPitch-pitch,360.),radius-initialRadius};
   } else offsets[k]=delta;
  }
  if(safe)break;
  amplitude*=.58;
  if(attempt==12)amplitude=0.;
  if(attempt==13)offsets={};
 }
 const std::array<const char*,3> keys=layer.orbit.enabled?std::array<const char*,3>{"yaw","pitch","radius"}:std::array<const char*,3>{"x","y","z"};
 for(unsigned axis=0;axis<3;++axis){
  MotionChannel c;c.parameter=keys[axis];c.amplitude=1.;c.stepped=stepped;
  bool moves=false;
  for(auto v:offsets){double value=axis==0?v.x:axis==1?v.y:v.z;c.samples.push_back(value);moves|=std::abs(value)>1e-8;}
  if(moves)result.push_back(std::move(c));
 }
 return result;
}
