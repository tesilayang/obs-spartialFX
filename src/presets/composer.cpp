#include "composer.hpp"
#include "scene_evaluator.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>
#include <algorithm>
#include <cmath>
#include <functional>
#include <set>

namespace spatial::presets {
const std::vector<StyleDefinition>&styleCatalog(){static const std::vector<StyleDefinition>v={
 {"gentle","温柔舒缓","缓慢呼吸与柔和圆形，保留主体空间",{.68,.55,.85},{.94,.74,.68}},
 {"geometry","简洁几何","克制的滑移、边框与斜带",{.28,.42,.92},{.18,.77,.84}},
 {"pop","活泼波普","弹性入场、星星与网点",{.98,.72,.12},{.96,.24,.48}},
 {"paper","纸片拼贴","轻摇卡片、纸条和角贴",{.83,.52,.32},{.94,.84,.58}},
 {"tech","科技界面","细线、角标、轨道与阶梯推进",{.12,.78,.85},{.38,.46,.98}},
 {"rhythm","节奏快剪","短促推近与斜带，保留稳定展示间隔",{.96,.28,.2},{.98,.77,.16}},
 {"dream","朦胧梦幻","椭圆漂浮、柔波与色团",{.63,.42,.94},{.34,.73,.94}},
 {"glitch","故障实验","阶梯运动和几何图块，无默认强闪光",{.42,.98,.35},{.9,.24,.76}}
 };return v;}
const std::vector<PartDefinition>&motionCatalog(){static const std::vector<PartDefinition>v={
 {"M02","缓推缓拉","沿深度缓慢推近再回到原构图"},{"M04","横向滑轨","水平滑移，循环首尾保持原位"},
 {"M08","曲线滑行","横向与纵向按不同相位组成平滑弧线"},{"M17","呼吸推拉","小幅同步缩放，像呼吸一样起伏"},
 {"M18","轻浮漂移","水平和垂直不同频率的小幅漂浮"},{"M20","摆锤轻摇","围绕素材中心轻微旋转"},
 {"M21","椭圆环游","沿闭合椭圆运动，保持素材朝向"},{"M25","淡入停留","淡入后稳定展示，末尾淡出"},
 {"M26","滑入刹停","从一侧滑到原位，稳定展示后退场"},{"M27","缩放入场","由略小尺寸放大到原尺寸后稳定展示"},
 {"M28","弹簧到位","指数衰减振荡回到原位，不使用普通正弦冒充"},{"M29","挤压回弹","横纵缩放反向变化，衰减回到原比例"},
 {"M31","支点甩入","偏移和旋转同时衰减，模拟卡片甩入"},{"M33","重拍推近","短促推近、缓慢恢复，保留安静区间"},
 {"M34","阶梯推近","四级离散推近后依级恢复"},{"M35","快速换构图","离散换位与稳定停留交替"},
 {"M39","依次登场","按勾选顺序错时滑入并淡入"},{"M44","双主体交班","两组所选素材交替淡入淡出，需至少两个素材"}
 };return v;}
const std::vector<PartDefinition>&maskCatalog(){static const std::vector<PartDefinition>v={
 {"T01","直线擦除","从一侧推进的硬边露出窗口"},{"T02","柔边擦除","同向推进的宽柔边窗口"},
 {"T03","斜带擦除","沿对角方向推进"},{"T08","中心对开","从中心向两侧打开"},
 {"T09","圆形虹膜","等比例圆形窗口扩大"},{"T10","椭圆窗口","跟随画布比例的椭圆扩大"},
 {"T11","方窗展开","矩形从中心向四周打开"},{"T12","菱形开合","菱形距离场扩展"},
 {"T13","多边形虹膜","六边形窗口向外展开"},{"T14","星形揭幕","五角星窗口扩大"},
 {"T16","时钟擦除","扇形角度绕中心打开"},{"T17","百叶窗","等宽竖条同步展开"},
 {"T18","错时百叶","竖条按列依次展开"},{"T19","倾斜条栅","斜向条栅同步展开"},
 {"T24","逐行铺开","按水平行依次露出"},{"T25","棋盘翻换","两组棋盘方格错时扩大"},
 {"T26","随机方格溶解","固定种子为每格分配露出时刻"},{"T27","中心图块扩散","方格从画面中心向外依次打开"},
 {"T28","对角图块波","方格沿对角线错时打开"},{"T33","波浪擦除","带有平滑起伏的擦除前沿"},
 {"T35","水滴汇聚","每格圆点以固定错时扩大汇合"},{"T57","网点扩张","规则圆点同步扩大直至覆盖全部画面"}
 };return v;}
const std::vector<PartDefinition>&decorationCatalog(){static const std::vector<PartDefinition>v={
 {"D01","呼吸圆环","厚圆环轻微呼吸，与实心圆组合时保留主体空间"},{"D03","角标归位","四角短线轻微滑动归位"},
 {"D04","描线框","四条细边组成干净的外框"},{"D05","斜带穿行","宽幅实心斜带从主体边缘掠过"},
 {"D06","交叉双带","两条宽幅色带以不同角度反向移动"},{"D07","边缘色面板","清晰的色面板、卡片或错落图块轻轻滑动"},
 {"D09","网点漂移","局部网点缓慢漂移，保留中央空间"},{"D10","棋盘流动","局部低透明度棋盘格轻移"},
 {"D13","虚线路径","短划线沿线方向缓动"},{"D14","箭头流","有限数量的三角箭头依次滑动"},
 {"D15","轨道小点","圆环与少量小圆点组成轨道"},{"D17","放射光束","中心留白的短放射线轻微旋转"},
 {"D19","星星跳动","画面边缘少量星星缩放摇摆"},{"D20","十字闪点","少量十字缓慢呼吸，无高频闪光"},
 {"D21","纸条底签","素材下缘的扁平纸条"},{"D23","胶带角贴","卡片角上的半透明斜条"},
 {"D25","呼吸色团","解析波形构成的圆润色团变形"},{"D26","柔波色带","低透明度波浪带缓慢流过"},
 {"D30","稀疏光点","少量柔边小光点缓慢漂浮"},{"D31","渐变光带","低透明度渐变带从边缘掠过"},
 {"D32","实心圆面","清晰的实心圆或椭圆色面，配合主体与辅助图形"}
 };return v;}
namespace {
std::string fresh(){return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();}
struct Random {uint32_t x;uint32_t next(){x+=0x9e3779b9u;uint32_t z=x;z=(z^(z>>16))*0x85ebca6bu;z=(z^(z>>13))*0xc2b2ae35u;return z^(z>>16);}double unit(){return double(next())/4294967296.;}};
const PartDefinition*part(const std::vector<PartDefinition>&list,const std::string&id){for(const auto&p:list)if(id==p.id)return &p;return nullptr;}
double smooth(double x){x=std::clamp(x,0.,1.);return x*x*(3-2*x);}
double edge(double u){return smooth(std::min(u,1-u)/.12);}
double entry(double u){return 1-smooth(u/.24);}
double exit(double u){return smooth((u-.78)/.22);}
bool finite(Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool multi(const std::string&p){return p=="sx"||p=="sy"||p=="opacity";}
int object(const StageModel&m,const std::string&id){if(id=="camera")return -1;for(size_t i=0;i<m.layers.size();++i)if(m.layers[i].instanceId==id)return int(i);for(size_t i=0;i<m.geometries.size();++i)if(m.geometries[i].instanceId==id)return -3-int(i);return INT_MIN;}
bool overlap(TimeUs a,TimeUs b,TimeUs c,TimeUs d){return a<d&&c<b;}
MotionChannel channel(const char*p,double a,const std::function<double(double)>&f,bool stepped=false){MotionChannel c;c.parameter=p;c.amplitude=a;c.stepped=stepped;for(int i=0;i<=64;++i)c.samples.push_back(f(i/64.));return c;}
QJsonObject ref(const std::string&id){return {{"instance_id",QString::fromStdString(id)}};}
void splice(Curve&c,TimeUs start,TimeUs end,double base,const std::vector<AnimKey>&generated,bool startIsGuard){
 const auto existing=std::find_if(c.keys().begin(),c.keys().end(),[&](const auto&k){return k.time==start;});
 const bool preserveGuard=startIsGuard&&existing!=c.keys().end()&&existing->value==generated.front().value;
 const AnimKey originalGuard=preserveGuard?*existing:AnimKey{};
 c.split(start,base);c.split(end,base);AnimKey first=*std::find_if(c.keys().begin(),c.keys().end(),[&](const auto&k){return k.time==start;});AnimKey last=*std::find_if(c.keys().begin(),c.keys().end(),[&](const auto&k){return k.time==end;});
 auto old=c.keys();for(const auto&k:old)if(k.time>start&&k.time<end)c.erase(k.time);
 // A guard outside the generated interval belongs to the existing timeline.
 // An unchanged value must also retain its interpolation and both Bezier handles.
 if(preserveGuard)first=originalGuard;
 else{first.value=generated.front().value;first.interpolation=generated.front().interpolation;first.outAbsolute=false;first.outX=first.outY=1./3;}
 last.value=generated.back().value;last.inAbsolute=false;last.inX=last.inY=2./3;
 c.put(first);for(size_t i=1;i+1<generated.size();++i)c.put(generated[i]);c.put(last);c.enabled=true;
}
std::vector<MotionChannel>motion(const std::string&id,double strength,double direction,size_t index,size_t count){
 const double a=strength,d=direction;
 auto sin=[](double u){return std::sin(2*pi*u)*edge(u);};auto pulse=[](double u){return .5-.5*std::cos(2*pi*u);};
 auto spring=[](double u){return u<.62?-std::exp(-11*u)*std::cos(21*u)*(1-smooth((u-.45)/.17)):0.;};
 if(id=="M02")return {channel("z",-110*a,pulse)};
 if(id=="M04")return {channel("x",75*a*d,sin)};
 if(id=="M08")return {channel("x",85*a*d,sin),channel("y",32*a,[](double u){return std::sin(4*pi*u)*edge(u);})};
 if(id=="M17")return {channel("sx",.055*a,pulse),channel("sy",.055*a,pulse)};
 if(id=="M18")return {channel("x",32*a*d,sin),channel("y",22*a,[](double u){return (std::cos(4*pi*u)-1)*edge(u);})};
 if(id=="M20")return {channel("rz",4*a*d,sin)};
 if(id=="M21")return {channel("x",60*a*d,sin),channel("y",25*a,[](double u){return (std::cos(2*pi*u)-1)*edge(u);})};
 if(id=="M25")return {channel("opacity",-1,[](double u){return std::max(entry(u),exit(u));})};
 if(id=="M26")return {channel("x",180*a*d,[](double u){return -entry(u)+exit(u);})};
 if(id=="M27")return {channel("sx",-.22*a,[](double u){return std::max(entry(u),exit(u));}),channel("sy",-.22*a,[](double u){return std::max(entry(u),exit(u));})};
 if(id=="M28")return {channel("x",135*a*d,spring)};
 if(id=="M29")return {channel("sx",.2*a,spring),channel("sy",-.16*a,spring)};
 if(id=="M31")return {channel("x",120*a*d,spring),channel("rz",22*a*d,spring)};
 if(id=="M33")return {channel("sx",.1*a,[](double u){return u<.42?std::sin(pi*std::min(1.,u/.42))*std::exp(-u*4):0.;}),channel("sy",.1*a,[](double u){return u<.42?std::sin(pi*std::min(1.,u/.42))*std::exp(-u*4):0.;})};
 if(id=="M34")return {channel("z",-18*a,[](double u){return std::floor(std::min(u,1-u)*8+.00001);},true)};
 if(id=="M35")return {channel("x",60*a*d,[](double u){if(u<.12||u>.88)return 0.;return u<.38?-1.:u<.65?1.:0.;},true)};
 if(id=="M39"){double delay=std::min(.35,double(index)*.06);auto enter=[delay](double u){return 1-smooth((u-delay)/.2);};return {channel("x",-120*a*d,enter),channel("opacity",-1,enter)};}
 if(id=="M44"){bool second=index%2!=0;return {channel("opacity",-1,[second,count](double u){(void)count;double on=smooth((u-.2)/.12)*(1-smooth((u-.7)/.12));return second?1-on:on;})};}
 return {};
}
}
bool compileComposition(StageModel&m,const ComposeRequest&r,std::string&error,ComposeFailure*failure){
 error.clear();if(failure)*failure=ComposeFailure::None;auto fail=[&](const std::string&s,ComposeFailure kind=ComposeFailure::Other){error=s;if(failure)*failure=kind;return false;};
 auto style=std::find_if(styleCatalog().begin(),styleCatalog().end(),[&](const auto&s){return r.styleId==s.id;});if(style==styleCatalog().end())return fail("找不到风格卡片");
 const std::string decorationStyle=r.decorationStyleId.empty()?r.styleId:r.decorationStyleId;auto decorStyle=std::find_if(styleCatalog().begin(),styleCatalog().end(),[&](const auto&s){return decorationStyle==s.id;});if(decorStyle==styleCatalog().end())return fail("找不到装饰风格");
 if(r.selectedIds.empty())return fail("请先勾选要动起来的素材");if(r.selectedIds.size()>32)return fail("最多选择32个素材");
 if(r.start<0||r.duration<10000||r.cycles<1||r.cycles>(INT64_MAX-r.start)/r.duration)return fail("时长无效或超出微秒时间范围");
 if(!std::isfinite(r.strength)||r.strength<0||r.strength>2||r.decorationAmount<0||r.decorationAmount>2||!finite(r.colorA)||!finite(r.colorB)||!finite(r.decorationPlane.position)||!finite(r.decorationPlane.rotation)||!std::isfinite(r.decorationPlane.scaleX)||!std::isfinite(r.decorationPlane.scaleY)||!finite(r.localMaskPlane.position)||!finite(r.localMaskPlane.rotation)||!std::isfinite(r.localMaskPlane.scaleX)||!std::isfinite(r.localMaskPlane.scaleY))return fail("组合参数无效");
 for(auto color:{r.colorA,r.colorB})if(color.x<0||color.y<0||color.z<0||color.x>1||color.y>1||color.z>1)return fail("颜色超出范围");
 std::set<std::string>unique,all;for(const auto&l:m.layers)if(l.instanceId.empty()||!all.insert(l.instanceId).second)return fail("舞台有无效或重复身份");for(const auto&g:m.geometries)if(g.instanceId.empty()||!all.insert(g.instanceId).second)return fail("舞台有无效或重复身份");
 for(const auto&id:r.selectedIds){const int o=object(m,id);if(o<0||!unique.insert(id).second)return fail("素材选择失效或重复");if(objectLocked(m,o))return fail("所选素材已锁定："+m.layers[size_t(o)].label);}
 if(r.camera&&m.cameraLocked)return fail("镜头已锁定");
 auto effective=[&](uint32_t overrideSeed,uint32_t salt){if(overrideSeed)return overrideSeed;Random stream{r.seed^salt};auto value=stream.next();return value?value:1u;};
 const uint32_t motionSeed=effective(r.motionSeed,0x4d4f544eu),maskSeed=effective(r.maskSeed,0x4d41534bu),decorationSeed=effective(r.decorationSeed,0x4445434fu);
 Random motionRng{motionSeed},maskRng{maskSeed},decorRng{decorationSeed};const size_t styleIndex=size_t(style-styleCatalog().begin()),decorStyleIndex=size_t(decorStyle-styleCatalog().begin());
 static const std::vector<std::vector<const char*>>motions={{"M02","M17","M18","M20"},{"M04","M08","M26","M27"},{"M28","M29","M33","M39"},{"M20","M31","M26"},{"M04","M08","M35"},{"M33","M34","M35","M39"},{"M17","M18","M21"},{"M34","M35","M31"}};
 static const std::vector<std::vector<const char*>>masks={{"T02","T09","T33"},{"T03","T08","T17"},{"T14","T25","T57"},{"T11","T12","T18"},{"T13","T24","T28"},{"T03","T17","T19"},{"T02","T10","T35"},{"T26","T28","T25"}};
 static const std::vector<std::vector<const char*>>decors={{"D25","D01","D32"},{"D07","D05","D32"},{"D32","D19","D09"},{"D21","D07","D23"},{"D15","D13","D07"},{"D05","D14","D07"},{"D25","D32","D26","D30"},{"D07","D10","D13"}};
 auto choose=[&](const std::string&id,const std::vector<std::vector<const char*>>&pool,Random&rng){const auto slot=rng.next()%pool[styleIndex].size();return id.empty()?std::string(pool[styleIndex][slot]):id;};
 const std::string motionId=choose(r.motionId,motions,motionRng),maskId=choose(r.maskId,masks,maskRng);
 // Consume one choice draw even for resolved/manual IDs so replay keeps layout draws aligned.
 const auto decorChoice=decorRng.next();std::vector<std::string>decorIds;
 if(r.decorationAmount){
  if(!r.resolvedDecorationIds.empty())decorIds=r.resolvedDecorationIds;
  else if(!r.decorationId.empty()){if(r.decorationId!="none")decorIds.push_back(r.decorationId);}
  else{
   for(const auto*id:decors[decorStyleIndex])decorIds.emplace_back(id);
   if(decorStyleIndex==5){if(decorChoice%2)decorIds[0]="D06";}
   else if(decorStyleIndex!=4&&decorStyleIndex!=7&&decorChoice%2)std::swap(decorIds[0],decorIds[1]);
  }
 }
 if(decorIds.size()>4)return fail("装饰组合最多包含4种图形");std::set<std::string>decorUnique;
 for(const auto&id:decorIds)if(!part(decorationCatalog(),id)||!decorUnique.insert(id).second)return fail("找不到所选装饰或装饰重复");
 const std::string decorId=decorIds.empty()?"none":decorIds.front();
 if(motionId!="none"&&!part(motionCatalog(),motionId))return fail("找不到所选运动");if(maskId!="none"&&!part(maskCatalog(),maskId))return fail("找不到所选遮罩");if(motionId=="M44"&&r.selectedIds.size()<2)return fail("双主体交班需要至少两个素材");
 if((decorId!="none"||(maskId!="none"&&!r.localMask))&&!r.anchorId.empty()&&object(m,r.anchorId)<0)return fail("图形锚点不存在");
 if(motionId=="none"&&maskId=="none"&&decorId=="none"&&!r.camera)return fail("请选择至少一种运动、遮罩或装饰");
 if(m.presetInstances.size()>=256)return fail("预设数量已达上限");StageModel out=m;
 PresetInstance preset;preset.instanceId=fresh();preset.definitionId="composition";preset.version=1;preset.seed=r.seed;preset.label=std::string(style->name)+" · 组合";preset.start=r.start;preset.end=r.start+r.duration*r.cycles;
 QJsonArray selected;for(const auto&id:r.selectedIds)selected.append(ref(id));QJsonObject recipe{{"definition_id","composition"},{"version",1},{"style",QString::fromStdString(r.styleId)},{"seed",double(r.seed)},{"motion",QString::fromStdString(motionId)},{"mask",QString::fromStdString(maskId)},{"decoration",QString::fromStdString(decorId)},{"selected",selected},{"start_us",QString::number(r.start)},{"duration_us",QString::number(r.duration)},{"cycles",QString::number(r.cycles)},{"loop",r.loop},{"strength",r.strength},{"decoration_amount",r.decorationAmount},{"camera",r.camera},{"color_a",QJsonObject{{"r",r.colorA.x},{"g",r.colorA.y},{"b",r.colorA.z}}},{"color_b",QJsonObject{{"r",r.colorB.x},{"g",r.colorB.y},{"b",r.colorB.z}}},{"local_mask",r.localMask},{"screen_fixed",r.screenFixed},{"anchor",ref(r.anchorId)},{"plane",QJsonObject{{"x",r.decorationPlane.position.x},{"y",r.decorationPlane.position.y},{"z",r.decorationPlane.position.z},{"rx",r.decorationPlane.rotation.x},{"ry",r.decorationPlane.rotation.y},{"rz",r.decorationPlane.rotation.z},{"sx",r.decorationPlane.scaleX},{"sy",r.decorationPlane.scaleY}}}};
 recipe["local_mask_plane"]=QJsonObject{{"x",r.localMaskPlane.position.x},{"y",r.localMaskPlane.position.y},{"z",r.localMaskPlane.position.z},{"rx",r.localMaskPlane.rotation.x},{"ry",r.localMaskPlane.rotation.y},{"rz",r.localMaskPlane.rotation.z},{"sx",r.localMaskPlane.scaleX},{"sy",r.localMaskPlane.scaleY}};
 QJsonArray resolvedDecor;for(const auto&id:decorIds)resolvedDecor.append(QJsonObject{{"id",QString::fromStdString(id)}});recipe["decoration_ids"]=resolvedDecor;
 recipe["decoration_style"]=QString::fromStdString(decorationStyle);
 const double direction=motionRng.next()%2?1.:-1.;recipe["direction"]=direction;recipe["motion_seed"]=double(motionSeed);recipe["mask_seed"]=double(maskSeed);recipe["decoration_seed"]=double(decorationSeed);auto*recipeData=obs_data_create_from_json(QJsonDocument(recipe).toJson(QJsonDocument::Compact).constData());preset.definitionSnapshot=obs_data_get_json(recipeData);obs_data_release(recipeData);out.presetInstances.push_back(preset);
 auto add=[&](const std::string&id,std::vector<MotionChannel>channels){
  int o=object(out,id);if(o==INT_MIN)return fail("运动绑定失效");if(objectLocked(out,o))return fail("对象已锁定");auto&a=objectAnimation(out,o);
  const bool orbit=o==-1?out.cameraOrbit.enabled:o>=0?out.layers[size_t(o)].orbit.enabled:false;
  for(auto&ch:channels){
   if(r.loop&&ch.samples.size()>1){
    const auto original=ch.samples;const size_t last=original.size()-1;
    const double delta=original.back()-original.front();
    if(std::abs(delta)>1e-10){
     // Smoothly reparameterize entry responses and remove displacement.
     // The stored samples close with identical one-sided zero tangents.
     for(size_t k=0;k<=last;++k){const double u=smooth(double(k)/double(last)),x=u*double(last);const size_t lo=std::min(size_t(x),last-1);const double f=x-double(lo);ch.samples[k]=original[lo]*(1-f)+original[lo+1]*f-delta*u;}
    }
    if(!ch.stepped){ch.samples[1]=ch.samples[0];ch.samples[last-1]=ch.samples[last];}
   }
   if(orbit){if(ch.parameter=="x"){ch.parameter="yaw";ch.amplitude*=.06;}if(ch.parameter=="y"){ch.parameter="pitch";ch.amplitude*=.06;}if(ch.parameter=="z")ch.parameter="radius";}
   auto it=a.find(ch.parameter);if(it!=a.end()){if(it->second.locked)return fail("参数轨已锁定："+ch.parameter);if(it->second.enabled&&!it->second.keys().empty()&&it->second.keys().front().time<=preset.end&&it->second.keys().back().time>=r.start&&!r.replace)return fail("参数区间已有关键帧："+ch.parameter,ComposeFailure::IntervalConflict);}
   for(const auto&b:out.motionBlocks)if(b.enabled&&b.targetId==id&&overlap(r.start,preset.end,b.start,b.end))for(const auto&c:b.channels)if(c.parameter==ch.parameter)return fail("参数区间已有循环动作："+ch.parameter,ComposeFailure::IntervalConflict);
  }
  if(r.loop){MotionBlock b;b.instanceId=fresh();b.ownerId=preset.instanceId;b.targetId=id;b.label=preset.label;b.start=r.start;b.end=preset.end;b.period=r.duration;b.timeOrigin=r.start;b.camera=o==-1;b.channels=std::move(channels);out.motionBlocks.push_back(std::move(b));return true;}
  for(const auto&ch:channels){GeneratedTrack track;track.targetId=id;track.parameter=ch.parameter;track.existed=a.find(ch.parameter)!=a.end();auto&c=a[ch.parameter];track.before=c;const double base=baseValue(out,o,ch.parameter),begin=c.value(r.start,base);std::vector<AnimKey>keys;
   // Finite cycles have bounded key storage; long repeated work uses loop blocks.
   if(r.cycles>64)return fail("有限动画最多64次；更多重复请选择循环模式");
   for(int64_t cycle=0;cycle<r.cycles;++cycle)for(size_t i=0;i<ch.samples.size();++i){if(cycle&&i==0)continue;const auto t=r.start+r.duration*cycle+TimeUs(r.duration/TimeUs(ch.samples.size()-1)*TimeUs(i)+(r.duration%TimeUs(ch.samples.size()-1))*TimeUs(i)/TimeUs(ch.samples.size()-1));const double value=multi(ch.parameter)?begin*(1+ch.amplitude*ch.samples[i]):begin+ch.amplitude*ch.samples[i];keys.push_back({t,value,ch.stepped?Interpolation::Hold:Interpolation::Linear});}
   const TimeUs lo=r.start>0?r.start-1:r.start,hi=preset.end<INT64_MAX?preset.end+1:preset.end;if(lo<r.start)keys.insert(keys.begin(),{lo,c.value(lo,base),Interpolation::Hold});if(hi>preset.end){keys.back().interpolation=Interpolation::Hold;keys.push_back({hi,c.value(hi,base),Interpolation::Linear});}splice(c,lo,hi,base,keys,lo<r.start);track.after=c;if(o>=-1)out.presetInstances.back().generatedTracks.push_back(std::move(track));
  }return true;
 };
 if(motionId!="none")for(size_t i=0;i<r.selectedIds.size();++i)if(!add(r.selectedIds[i],motion(motionId,r.strength,direction,i,r.selectedIds.size())))return false;
 if(r.camera&&!add("camera",motion("M04",r.strength*.18,direction,0,1)))return false;
 const double w=out.width,h=out.height;
 auto geometry=[&](const std::string&label){Geometry g;g.instanceId=fresh();g.ownerId=preset.instanceId;g.label=label;g.anchorId=r.anchorId;g.plane=r.decorationPlane;g.layoutPivot=r.decorationPlane.position;g.screenFixed=r.screenFixed;g.color=r.colorA;g.referenceWidth=w;g.referenceHeight=h;g.width=w;g.height=h;g.spacing=h*.1;g.thickness=std::max(2.,h*.006);g.softness=1;return g;};
 if(maskId!="none")for(const auto&id:r.selectedIds){Geometry g=geometry(std::string(part(maskCatalog(),maskId)->name));g.mode=Mode::Mask;g.receivers={id};g.localMask=r.localMask;g.referenceWidth=g.referenceHeight=0;g.width=w*1.05;g.height=h*1.05;g.reveal=int(part(maskCatalog(),maskId)-maskCatalog().data())+1;g.revealStart=r.start;g.revealPeriod=r.duration;g.revealLoop=r.loop||r.cycles>1;g.detail=double(maskRng.next()%65536);g.softness=1.;if(g.localMask){g.plane=r.localMaskPlane;g.layoutPivot=r.localMaskPlane.position;g.anchorId.clear();g.screenFixed=false;const auto&l=out.layers[size_t(object(out,id))];if(l.importWidth>0&&l.importHeight>0){g.width=std::max(1.,l.importWidth-l.cropLeft-l.cropRight);g.height=std::max(1.,l.importHeight-l.cropTop-l.cropBottom);}}out.geometries.push_back(g);}
 const bool rich=r.decorationAmount==2,mixed=decorIds.size()>1;const double densityScale=rich?1.16:1.;
 for(size_t motif=0;motif<decorIds.size();++motif){
  const auto&decoration=decorIds[motif];int copies=rich?4:2;
  if(decoration=="D01"||decoration=="D09"||decoration=="D10"||decoration=="D17"||decoration=="D21"||decoration=="D25"||decoration=="D26"||decoration=="D31")copies=rich?2:1;
  if(decoration=="D06")copies=2;if(decoration=="D03")copies=rich?16:8;if(decoration=="D04")copies=rich?8:4;
  if(decoration=="D05")copies=rich?3:2;if(decoration=="D07"||decoration=="D13"||decoration=="D32")copies=rich?3:2;
  if(decoration=="D15")copies=rich?7:4;if(decoration=="D30")copies=rich?6:3;
  for(int i=0;i<copies;++i){
   Geometry g=geometry(std::string(part(decorationCatalog(),decoration)->name)+" "+std::to_string(i+1));g.mode=Mode::Decoration;g.depthOrder=int(motif)*20+i;
   g.color=(size_t(i)+motif)%2?r.colorB:r.colorA;g.width=g.height=h*.1;g.opacity=.88;
   const double side=i%2?1.:-1.,jitter=(decorRng.unit()-.5),variation=.94+decorRng.unit()*.12;
   g.plane.position.x+=side*w*(.31+jitter*.035);g.plane.position.y+=(decorRng.unit()-.5)*h*.62;
   std::vector<MotionChannel>channels={channel("y",10*r.strength,[](double u){return std::sin(2*pi*u)*edge(u);})};
   if(decoration=="D01"){
    g.shape=Shape::Ring;g.width=g.height=h*(mixed?.55:.78)*densityScale;g.thickness=h*(rich?.045:.035);g.opacity=.82;
    if(mixed){g.plane.position.x=r.decorationPlane.position.x-side*w*.29;g.plane.position.y=r.decorationPlane.position.y+side*h*.18;}
    else{g.plane.position=r.decorationPlane.position;if(i)g.plane.position.x+=w*.13;}
    channels={channel("sx",.05*r.strength,[](double u){return .5-.5*std::cos(2*pi*u);}),channel("sy",.05*r.strength,[](double u){return .5-.5*std::cos(2*pi*u);})};
   }else if(decoration=="D03"||decoration=="D04"){
    const bool corners=decoration=="D03";const int frame=i/(corners?8:4),segment=i%(corners?8:4),corner=segment/2;const bool horizontal=corners?segment%2==0:segment<2;
    const double sx=corners?(corner%2?1.:-1.):segment==2?-1.:1.,sy=corners?(corner/2?1.:-1.):segment==0?-1.:1.;
    const double fw=w*(.62+frame*.08),fh=h*(.78+frame*.08),arm=h*.1;g.thickness=h*(rich?.015:.012);g.width=horizontal?(corners?arm:fw):g.thickness;g.height=horizontal?g.thickness:(corners?arm:fh);
    g.plane.position=r.decorationPlane.position;g.plane.position.x+=horizontal&&!corners?0:sx*(fw-g.width)*.5;g.plane.position.y+=!horizontal&&!corners?0:sy*(fh-g.height)*.5;
   }else if(decoration=="D05"||decoration=="D06"){
    const bool crossed=decoration=="D06";g.shape=Shape::Parallelogram;g.width=w*(crossed?.76:.65)*densityScale;g.height=h*(crossed?.11:.10)*densityScale;
    g.plane.rotation.z+=crossed?(i?-29:29):22;g.plane.position.x=r.decorationPlane.position.x+(crossed?0:side*w*.12);g.plane.position.y=r.decorationPlane.position.y+side*h*(.30+.045*(i/2));
    const double sense=crossed?side:1.;channels={channel("x",(crossed?35:65)*r.strength*sense,[](double u){return std::sin(2*pi*u)*edge(u);})};
   }else if(decoration=="D07"){
    g.width=w*.14*densityScale;g.height=h*.63*densityScale;g.plane.position.x=r.decorationPlane.position.x+side*w*(.37+jitter*.02);g.plane.position.y=r.decorationPlane.position.y+((i/2)*2-1)*h*.04;
    if(mixed&&decorationStyle=="paper"){g.width=w*.18*densityScale;g.height=h*.36*densityScale;g.plane.rotation.z+=side*(7+jitter*4);g.plane.position.x=r.decorationPlane.position.x+side*w*.31;g.plane.position.y=r.decorationPlane.position.y-side*h*.13;}
    else if(mixed&&decorationStyle=="tech"){g.width=w*.19*densityScale;g.height=h*.32*densityScale;g.opacity=.68;g.plane.position.y=r.decorationPlane.position.y+side*h*.19;}
    else if(mixed&&(decorationStyle=="rhythm"||decorationStyle=="glitch")){g.width=w*(decorationStyle=="glitch"?.26:.17)*densityScale;g.height=h*.20*densityScale;g.plane.position.x=r.decorationPlane.position.x+side*w*.30;g.plane.position.y=r.decorationPlane.position.y+side*h*(.22-.055*(i/2));}
   }else if(decoration=="D09"||decoration=="D10"){
    g.shape=decoration=="D09"?Shape::DotGrid:Shape::Checker;g.width=w*.23*densityScale;g.height=h*.26*densityScale;g.spacing=h*(decoration=="D09"?.044:.055);g.thickness=h*.012;g.opacity=decoration=="D09"?.42:.28;
    g.plane.position.x=r.decorationPlane.position.x-side*w*.32;g.plane.position.y=r.decorationPlane.position.y+side*h*.27;channels={channel("x",10*r.strength,[](double u){return std::sin(2*pi*u)*edge(u);})};
   }else if(decoration=="D13"){
    g.shape=Shape::Dashed;g.width=w*(mixed?.30:.58)*densityScale;g.height=h*(decorationStyle=="glitch"?.026:.018)*densityScale;g.spacing=h*.065;g.thickness=h*.04;g.opacity=.8;
    g.plane.position.x=r.decorationPlane.position.x+side*w*.24;g.plane.position.y=r.decorationPlane.position.y+side*h*(.34-.055*(i/2));channels={channel("x",18*r.strength,[](double u){return std::sin(2*pi*u)*edge(u);})};
   }else if(decoration=="D14"){
    g.shape=Shape::Polygon;g.sides=3;g.width=g.height=h*.13*densityScale;g.plane.rotation.z+=30;g.plane.position.y=r.decorationPlane.position.y+h*.32;g.plane.position.x=r.decorationPlane.position.x+(i-double(copies-1)*.5)*h*.19;
    channels={channel("x",28*r.strength,[](double u){return std::sin(2*pi*u)*edge(u);})};
   }else if(decoration=="D15"){
    const double radius=h*.27*densityScale,angle=(i-1)*2*pi/double(copies-1);g.shape=i==0?Shape::Ring:Shape::Ellipse;g.plane.position=r.decorationPlane.position;g.plane.position.x+=mixed?w*.23:0;
    if(i==0){g.width=g.height=radius*2;g.thickness=h*.022;g.opacity=.72;channels.clear();}
    else{g.width=g.height=h*.052*densityScale;g.plane.position.x+=std::cos(angle)*radius;g.plane.position.y+=std::sin(angle)*radius;channels={channel("x",radius,[angle](double u){return std::cos(angle+2*pi*smooth(u))-std::cos(angle);}),channel("y",radius,[angle](double u){return std::sin(angle+2*pi*smooth(u))-std::sin(angle);})};}
   }else if(decoration=="D17"){
    g.shape=Shape::Radial;g.width=g.height=h*.92*densityScale;g.plane.position=r.decorationPlane.position;g.spacing=16;g.opacity=.32;channels={channel("rz",8*r.strength,[](double u){return std::sin(2*pi*u)*edge(u);})};
   }else if(decoration=="D19"||decoration=="D20"){
    g.shape=decoration=="D19"?Shape::Star:Shape::Cross;g.width=g.height=h*(decoration=="D19"?.24:.095)*densityScale;g.sides=5;g.thickness=g.width*.2;g.plane.rotation.z+=jitter*40;
    g.plane.position.y=r.decorationPlane.position.y-side*h*(.23-.045*(i/2));channels={channel("sx",.12*r.strength,[](double u){return .5-.5*std::cos(2*pi*u);}),channel("sy",.12*r.strength,[](double u){return .5-.5*std::cos(2*pi*u);}),channel("rz",10*r.strength,[](double u){return std::sin(2*pi*u)*edge(u);})};
   }else if(decoration=="D21"){
    g.shape=Shape::Parallelogram;g.width=w*.43*densityScale;g.height=h*.14*densityScale;g.plane.position=r.decorationPlane.position;g.plane.position.x+=i? w*.17:-w*.06;g.plane.position.y+=h*(i?-.30:.30);g.plane.rotation.z+=i?5:-4;
   }else if(decoration=="D23"){
    g.width=w*.14*densityScale;g.height=h*.067*densityScale;g.opacity=.76;g.plane.position.x=r.decorationPlane.position.x+side*w*.29;g.plane.position.y=r.decorationPlane.position.y+(mixed&&decorationStyle=="paper"?(i<2?-side*.30:side*.05):(i<2?-.30:.30))*h;g.plane.rotation.z+=side*23;
   }else if(decoration=="D25"){
    g.shape=Shape::Blob;g.width=h*.66*densityScale;g.height=h*.53*densityScale;g.opacity=.62;g.plane.position.x=r.decorationPlane.position.x+side*w*.31;g.plane.position.y=r.decorationPlane.position.y-side*h*.22;g.detail=decorRng.unit()*2*pi;
    channels={channel("rz",7*r.strength,[](double u){return std::sin(2*pi*u)*edge(u);}),channel("sx",.08*r.strength,[](double u){return .5-.5*std::cos(2*pi*u);})};
   }else if(decoration=="D26"){
    g.shape=Shape::WaveBand;g.width=w*.76*densityScale;g.height=h*.23*densityScale;g.opacity=.48;g.shear=.5;g.plane.position=r.decorationPlane.position;g.plane.position.y+=side*h*.33;g.plane.rotation.z+=side*4;
   }else if(decoration=="D30"){
    g.shape=Shape::Ellipse;g.width=g.height=h*(.014+decorRng.unit()*.014)*densityScale;g.softness=g.width*.7;g.opacity=.64;
   }else if(decoration=="D31"){
    g.shape=Shape::Gradient;g.width=w*.23*densityScale;g.height=h*.72*densityScale;g.plane.rotation.z+=22;g.opacity=.4;g.plane.position.x=r.decorationPlane.position.x+side*w*.37;channels={channel("x",w*.08*r.strength,[](double u){return std::sin(2*pi*u)*edge(u);})};
   }else if(decoration=="D32"){
    g.shape=Shape::Ellipse;g.width=g.height=h*(i?.20:.32)*densityScale;
    if(mixed&&decorationStyle=="dream"){g.width=h*.43*densityScale;g.height=h*.28*densityScale;g.opacity=.66;g.plane.rotation.z+=side*18;}
    g.plane.position.y=r.decorationPlane.position.y+side*h*(.25-.055*(i/2));
   }
   // A small seeded size variation makes rerolls useful without creating unbounded layouts.
   // Preserve exact analytic frame joins and orbit radii rather than jittering each segment.
   if(decoration!="D03"&&decoration!="D04"&&decoration!="D15"){g.width*=variation;g.height*=variation;}
   out.geometries.push_back(g);if(!channels.empty()&&!add(g.instanceId,channels))return false;
  }
 }
 if(out.geometries.size()>128||out.motionBlocks.size()>512)return fail("组合超过128个图形或512个循环动作预算，请减少装饰或素材数量");m=std::move(out);return true;
}
}
