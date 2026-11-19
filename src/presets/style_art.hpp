#pragma once
#include "presets/composer.hpp"
#include "scene_evaluator.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace spatial::presets::style_art {
// Art boards use canvas-height units. Their parts share a registration point,
// so changing the corner or the palette never breaks the drawing apart.
struct Ink {
 const char* label;
 Shape shape;
 double x,y,w,h,angle;
 int pigment; // palette A, B, paper tint, or ink shade
 double alpha,stroke=.005,spacing=.024,softness=.0015;
};
inline std::vector<Ink> board(int style, bool echo, int variant){
 using S=Shape;
 if(echo){
  switch(style){
  case 0:return {{"花粉",S::Ellipse,0,0,.034,.034,0,1,.77},{"小叶",S::Ellipse,.044,-.025,.048,.019,-32,0,.72}};
  case 1:return {{"书挡刻线",S::Rectangle,0,0,.108,.009,0,0,.94},{"方印",S::Rectangle,-.066,0,.022,.022,0,1,.94}};
  case 2:return {{"小印花",S::Star,0,0,.078,.078,14,1,.95},{"圆点",S::Ellipse,.063,-.027,.023,.023,0,2,.96}};
  case 3:return {{"纸屑",S::Parallelogram,0,0,.096,.035,13,2,.92},{"压纹",S::Dashed,0,0,.078,.005,13,3,.65,.008,.016}};
  case 4:return {{"状态灯",S::Ellipse,0,0,.020,.020,0,1,1},{"读数",S::Dashed,.074,0,.094,.012,0,0,.94,.019,.027}};
  case 5:return {{"尾迹",S::Parallelogram,0,0,.19,.022,-12,0,.92},{"短尾迹",S::Parallelogram,-.042,.023,.105,.012,-12,1,.92}};
  case 6:return {{"星点",S::Cross,0,0,.055,.055,12,2,.94,.009},{"伴星",S::Ellipse,.037,-.031,.013,.013,0,1,.86}};
  default:return {{"信号尾码",S::Dashed,0,0,.135,.018,0,1,.95,.017,.029},{"偏移线",S::Rectangle,-.028,.022,.094,.005,0,0,.96}};
  }
 }
 // Second and third editions keep the same visual vocabulary, with a new
 // silhouette or arrangement. Rerolling should change the art, not just its size.
 if(variant==1){switch(style){
 case 0:return {{"晨光 · 展叶",S::Ellipse,-.050,.018,.172,.064,25,0,.80},{"晨光 · 小叶",S::Ellipse,.033,-.025,.135,.056,-27,1,.85},{"叶茎",S::Rectangle,.003,.021,.175,.005,-15,2,.84},{"晨露",S::Ellipse,.080,-.060,.025,.025,0,2,.94}};
 case 1:return {{"拱门 · 圆顶",S::Ellipse,-.006,-.031,.155,.155,0,0,.97},{"拱门 · 基座",S::Rectangle,-.006,.025,.155,.099,0,0,.97},{"拱门 · 阳面",S::Rectangle,.033,.025,.078,.099,0,1,.99},{"拱门 · 轴线",S::Rectangle,.087,.073,.099,.008,0,2,1}};
 case 2:return {{"漫画 · 网点",S::DotGrid,-.023,.030,.229,.129,0,0,.92,.008,.026},{"漫画 · 气泡",S::Ellipse,0,-.004,.195,.107,-10,2,1},{"气泡角",S::Polygon,-.056,.036,.042,.042,35,2,.99},{"感叹号",S::Rectangle,0,-.016,.018,.042,8,1,1},{"点",S::Ellipse,-.006,.019,.021,.021,0,1,1}};
 case 3:return {{"拼贴 · 便签底",S::Rectangle,-.021,.014,.190,.132,-9,0,.88},{"拼贴 · 便签面",S::Rectangle,0,0,.184,.136,6,2,.99},{"索引条",S::Rectangle,.017,-.059,.105,.037,-6,1,.96},{"印刷横线",S::Rectangle,-.011,.012,.112,.006,6,3,.64},{"签名",S::Dashed,-.022,.031,.081,.006,6,0,.87,.014,.02}};
 case 4:return {{"示波器底座",S::Rectangle,0,0,.251,.113,0,3,.92},{"网格",S::Grid,-.001,.010,.22,.066,0,0,.49,.002,.021},{"信号轨迹",S::WaveBand,0,.009,.21,.057,0,1,.97},{"状态点",S::Ellipse,-.096,-.033,.013,.013,0,0,1},{"刻度",S::Dashed,.059,-.033,.098,.006,0,2,.91,.013,.021}};
 case 5:return {{"双彩带 · 远面",S::Parallelogram,-.037,.031,.216,.051,-19,1,.98},{"双彩带 · 反折",S::Parallelogram,.064,.007,.075,.045,27,3,.99},{"双彩带 · 近面",S::Parallelogram,.012,-.019,.219,.056,-17,0,1},{"亮缎",S::Parallelogram,.030,-.028,.157,.009,-17,2,.93}};
 case 6:return {{"行星 · 雾光",S::Ellipse,-.021,.003,.204,.168,0,0,.50,.005,.024,.062},{"行星 · 星体",S::Ellipse,-.021,.003,.115,.115,0,1,.83},{"行星 · 星环",S::Ring,-.021,.003,.221,.068,-26,2,.98,.006},{"伴星",S::Cross,.095,-.059,.035,.035,0,2,.91,.006}};
 default:return {{"信号 · 抽样窗",S::Rectangle,-.015,0,.192,.117,0,3,.97},{"分色条",S::Stripes,-.018,-.022,.176,.046,0,0,.98,.016,.032},{"扫描截线",S::Rectangle,.025,.006,.243,.013,0,1,.95},{"校验位",S::Checker,-.054,.034,.101,.022,0,2,.95,.004,.011}};
 }}
 if(variant==2){switch(style){
 case 0:return {{"花簇 · 侧瓣",S::Ellipse,-.052,0,.172,.099,-31,1,.73},{"花簇 · 顶瓣",S::Ellipse,.021,-.040,.119,.086,-62,0,.66},{"花簇 · 下瓣",S::Ellipse,.019,.017,.146,.087,12,0,.78},{"花蕊",S::Ellipse,-.002,.032,.036,.031,0,2,.97}};
 case 1:return {{"折纸路标",S::Polygon,-.051,.003,.124,.138,0,0,.97},{"转折面",S::Parallelogram,.024,-.003,.112,.083,-13,1,.98},{"路标短横",S::Rectangle,.030,.063,.171,.009,0,3,.98},{"路标方点",S::Rectangle,-.068,.063,.017,.017,0,2,.95}};
 case 2:return {{"波普 · 日芒",S::Radial,-.017,-.002,.166,.166,0,1,.98},{"唱片",S::Ellipse,-.017,-.002,.111,.111,0,0,1},{"唱片中心",S::Ring,-.017,-.002,.051,.051,0,2,1,.014},{"跳动标记",S::Star,.086,.039,.063,.063,20,1,.98}};
 case 3:return {{"拼贴 · 相纸",S::Rectangle,0,.003,.172,.151,-8,2,.99},{"相纸 · 着色区",S::Rectangle,-.001,-.008,.143,.104,-8,0,.90},{"剪影日轮",S::Ellipse,-.028,-.020,.062,.062,0,1,.98},{"剪影山丘",S::Polygon,.017,.006,.082,.076,-9,3,.78},{"角贴",S::Parallelogram,.072,-.059,.089,.026,28,1,.90}};
 case 4:return {{"标尺 · 支架",S::Rectangle,-.021,.029,.220,.041,0,3,.90},{"刻度尺",S::Dashed,-.021,.018,.205,.026,0,0,.95,.005,.015},{"游标",S::Polygon,.025,-.012,.037,.038,90,1,.99},{"标定点",S::Ring,-.095,-.027,.046,.046,0,2,.94,.006}};
 case 5:return {{"箭羽 · 投影",S::Parallelogram,-.009,.018,.211,.056,-11,3,.94},{"箭羽 · 上绸",S::Parallelogram,-.006,-.012,.203,.041,-11,0,.98},{"箭羽 · 金线",S::Parallelogram,.005,.022,.216,.018,-11,1,1},{"箭羽 · 尾饰",S::Polygon,.110,-.032,.055,.057,-12,2,.96}};
 case 6:return {{"流星 · 柔光",S::Ellipse,0,.006,.234,.107,-19,0,.55,.005,.024,.049},{"流星 · 星尾",S::Gradient,-.030,.016,.180,.035,-22,1,.86},{"流星 · 主星",S::Cross,.046,-.015,.065,.065,-9,2,.98,.009},{"流星 · 细星",S::Cross,-.071,-.036,.034,.034,11,1,.88,.006}};
 default:return {{"错码 · 渐层",S::Gradient,-.012,.004,.226,.081,0,0,.88},{"错码 · 条码",S::Dashed,-.025,-.006,.200,.041,0,2,.99,.009,.019},{"错码 · 扫描头",S::Rectangle,.076,0,.022,.091,0,1,.94},{"错码 · 余影",S::Rectangle,-.048,.045,.139,.008,0,0,.95}};
 }}
 switch(style){
 case 0:return {{"晨光 · 远瓣",S::Ellipse,-.050,.007,.223,.114,-29,0,.58,.005,.024,.016},
  {"晨光 · 近瓣",S::Ellipse,.044,.025,.167,.091,variant?31.:18.,1,.79,.005,.024,.007},
  {"花脉",S::Ellipse,-.047,.006,.134,.017,-29,2,.79},{"花粉",S::Ellipse,.109,-.042,.032,.032,0,2,.93}};
 case 1:return {{"几何 · 底座",S::Rectangle,-.021,.034,.238,.073,0,0,.94},{"几何 · 日轮",S::Ellipse,.041,-.041,.136,.136,0,1,.94},
  {"书挡横梁",S::Rectangle,-.031,.042,.257,.013,0,3,.95},{"标尺",S::Dashed,-.059,.070,.138,.009,0,2,.95,.009,.024}};
 case 2:return {{"波普 · 网点底印",S::DotGrid,-.039,.022,.226,.145,-8,0,.86,.007,.024},
  {"错版阴影",S::Star,.025,.006,.149,.149,-14,3,.93},{"波普星章",S::Star,.017,-.005,.149,.149,-14,1,1},
  {"贴纸亮点",S::Ellipse,.020,-.014,.027,.027,0,2,.99}};
 case 3:return {{"拼贴 · 票底",S::Rectangle,.003,.018,.248,.109,-8,3,.73},
  {"拼贴 · 纸票",S::Rectangle,0,.005,.248,.110,-8,2,.98},{"侧签",S::Rectangle,-.089,.018,.045,.108,-8,0,.94},
  {"票面印迹",S::Dashed,.015,.002,.102,.009,-8,3,.76,.016,.024},{"撕线",S::Dashed,.074,-.004,.091,.004,82,3,.68,.007,.014},
  {"压角胶带",S::Parallelogram,-.062,-.049,.105,.036,variant?11.:16.,1,.92}};
 case 4:return {{"仪表 · 表盘",S::Ellipse,-.033,-.008,.170,.170,0,3,.80},
  {"仪表 · 刻度",S::Radial,-.033,-.008,.160,.160,0,0,.98},{"内轨",S::Ring,-.033,-.008,.109,.109,0,0,.62,.004},
  {"表针",S::Rectangle,-.017,-.026,.055,.007,-45,2,.96},{"仪表轴",S::Ellipse,-.033,-.008,.017,.017,0,1,1},
  {"引线",S::Rectangle,.078,.051,.100,.005,0,0,.97},{"数字格",S::Dashed,.085,.032,.082,.011,0,1,.98,.014,.021}};
 case 5:return {{"彩带 · 折尾",S::Parallelogram,-.098,.044,.100,.049,15,3,.98},
  {"彩带 · 折面",S::Polygon,-.059,.022,.056,.058,26,1,.99},{"彩带 · 主绸",S::Parallelogram,.027,-.004,.240,.059,-14,0,.98},
  {"彩带 · 亮边",S::Parallelogram,.030,-.021,.224,.009,-14,2,.88},{"速度线",S::Parallelogram,.068,.029,.152,.013,-14,1,.97}};
 case 6:return {{"极光 · 雾晕",S::Ellipse,-.017,.006,.251,.167,-26,0,.47,.005,.024,.062},
  {"月轮",S::Ring,-.030,-.008,.151,.151,0,2,.88,.007},{"极光带",S::WaveBand,.018,.019,.234,.068,-17,1,.81,.005,.024,.006},
  {"星核",S::Cross,.073,-.043,.047,.047,10,2,.99,.008},{"星尘",S::Ellipse,-.105,.028,.017,.017,0,1,.92}};
 default:return {{"信号 · 错版底",S::Rectangle,-.012,.006,.228,.068,0,1,.91},
  {"信号 · 主条",S::Rectangle,.011,-.011,.228,.055,0,3,.98},{"局部棋格",S::Checker,-.039,-.012,.108,.046,0,0,.92,.005,.012},
  {"信号峰值",S::Rectangle,.066,-.011,.059,.012,0,2,.98},{"错位扫描",S::Rectangle,-.060,.054,.143,.012,0,0,.95}};
 }
}
// A free edge can carry a longer, quieter second rhythm on a rich board.
inline std::vector<Ink> edgeBoard(int style){using S=Shape;switch(style){
case 0:return {{"花枝 · 茎",S::Rectangle,-.005,0,.37,.006,-6,0,.73},{"花枝 · 叶一",S::Ellipse,-.075,-.011,.093,.029,15,1,.82},{"花枝 · 叶二",S::Ellipse,.055,-.017,.079,.028,-25,0,.78}};
case 1:return {{"延伸基线",S::Rectangle,0,0,.39,.007,0,3,.86},{"基准点",S::Rectangle,-.165,0,.027,.027,0,1,.97},{"几何带",S::Stripes,.085,-.013,.159,.025,0,0,.82,.006,.019}};
case 2:return {{"印花边",S::DotGrid,0,0,.352,.070,0,0,.69,.005,.024},{"弹跳星",S::Star,-.092,-.015,.065,.065,14,1,.97},{"印花圆",S::Ellipse,.102,.016,.042,.042,0,2,.89}};
case 3:return {{"装订线",S::Dashed,0,0,.375,.006,-3,2,.91,.016,.03},{"纸页签",S::Parallelogram,-.095,-.015,.098,.033,7,1,.86},{"印墨点",S::Ellipse,.144,-.008,.017,.017,0,0,.94}};
case 4:return {{"仪表 · 信号总线",S::Rectangle,0,0,.432,.005,0,0,.80},{"信道脉冲",S::WaveBand,-.011,-.003,.124,.045,0,1,.97},{"数据包",S::Dashed,.144,-.014,.096,.010,0,2,.78,.010,.018}};
case 5:return {{"流线 · 绸光",S::Parallelogram,-.025,.005,.383,.025,-4,0,.78},{"流线 · 拍点",S::Dashed,.076,-.020,.236,.012,-4,1,.96,.027,.046},{"流线 · 高光",S::Parallelogram,-.110,-.015,.153,.009,-4,2,.89}};
case 6:return {{"环绕 · 星轨",S::Ring,0,0,.355,.067,-10,1,.69,.003},{"环绕 · 伴星",S::Ellipse,.115,-.038,.030,.030,0,2,.97},{"环绕 · 星尘",S::Cross,-.098,.047,.039,.039,-7,0,.89,.006}};
default:return {{"采样轨",S::Dashed,-.007,-.010,.405,.011,0,0,.88,.019,.031},{"失步块",S::Rectangle,.071,.012,.093,.021,0,1,.90},{"对时轨",S::Rectangle,-.077,.021,.191,.005,0,2,.88}};}}
struct Piece {
 std::string label; Shape shape; Plane plane; Vec3 color;
 double width,height,thickness,spacing,softness,shear=.18,sides=3,opacity; int order=0;
 std::vector<MotionChannel> motion;
};
struct Rect {double left=1e9,top=1e9,right=-1e9,bottom=-1e9;};
inline double area(Rect a){return std::max(0.,a.right-a.left)*std::max(0.,a.bottom-a.top);}
inline Rect intersection(Rect a,Rect b){return {std::max(a.left,b.left),std::max(a.top,b.top),std::min(a.right,b.right),std::min(a.bottom,b.bottom)};}
inline void include(Rect&box,double x,double y){box.left=std::min(box.left,x);box.right=std::max(box.right,x);box.top=std::min(box.top,y);box.bottom=std::max(box.bottom,y);}
inline Rect projectedBox(const Plane&plane,double w,double h,const Camera&camera,double cw,double ch,bool screen){
 Rect result;for(const auto&p:corners(plane,w,h)){if(screen){include(result,.5+p.x/cw,.5+p.y/ch);continue;}const auto q=project(p,camera,cw,ch);if(!q.visible)return {};include(result,q.x/cw,q.y/ch);}return result;
}
inline std::vector<Rect> protectedAreas(const StageModel&m,const EvaluatedScene&e,int manual){
 std::vector<Rect> areas;const Rect canvas{0,0,1,1};
 // OBS may report a full canvas for a transparent capture. Ignore uniform
 // full-bleed coverage, keep a central subject reserve and allow manual placement protection.
 areas.push_back({.32,.19,.68,.80});
 const Rect protectedZones[]={{},{.20,.12,.80,.88},{0,0,.51,1},{.49,0,1,1},{0,.51,1,1},{0,0,1,.49}};
 if(manual>0&&manual<6)areas.push_back(protectedZones[manual]);
 for(size_t i=0;i<m.layers.size();++i){const auto&l=m.layers[i];if(!l.enabled||!l.visible||e.world[i].opacity<.03||l.importWidth<=0||l.importHeight<=0)continue;
  double w=std::max(1.,l.importWidth-l.cropLeft-l.cropRight),h=std::max(1.,l.importHeight-l.cropTop-l.cropBottom);
  if(l.followSourceSize){const auto lay=sourceLayout(l.importWidth,l.importHeight,l.importTransform);w=lay.width;h=lay.height;}
  auto b=intersection(projectedBox(e.world[i].plane,w,h,e.camera,m.width,m.height,false),canvas);
  if(area(b)<=0||area(b)>.78)continue;
  areas.push_back({b.left-.018,b.top-.022,b.right+.018,b.bottom+.022});
 }return areas;
}
inline double eased(double u){u=std::clamp(u,0.,1.);return u*u*(3.-2.*u);}
template<typename F> MotionChannel curve(const char*name,double amplitude,F fn){MotionChannel c;c.parameter=name;c.amplitude=amplitude;for(int k=0;k<=24;++k)c.samples.push_back(fn(double(k)/24));return c;}
inline Vec3 mixInk(Vec3 a,Vec3 b,double t){return add(multiply(a,1-t),multiply(b,t));}
inline std::vector<Piece> compose(const StageModel&m,const ComposeRequest&r,int style,uint32_t seed,int&groupCount){
 const auto scene=evaluateSceneAt(m,r.start);const auto occupied=protectedAreas(m,scene,r.decorationProtection);const double cw=m.width,ch=m.height;
 if(cw<1||ch<1)return {};
 const Plane*parent=nullptr;for(size_t i=0;i<m.layers.size();++i)if(m.layers[i].instanceId==r.anchorId&&!r.screenFixed)parent=&scene.world[i].plane;
 auto worldPlane=[&](Plane p){if(parent){p.position=bindPosition(p.position,*parent);p.rotation=add(parent->rotation,p.rotation);p.scaleX*=parent->scaleX;p.scaleY*=parent->scaleY;}return p;};
 const auto center=worldPlane(r.decorationPlane);const double depth=project(center.position,scene.camera,cw,ch).depth;
 const double unit=r.screenFixed?ch:depth*2*std::tan(scene.camera.fov*pi/360);
 if(unit<=0||!std::isfinite(unit))return {};
 const auto variant=int(seed%3);const auto mainCorner=int((seed>>6)%4);
 const double jitter=(double((seed>>10)%101)/100.-.5)*.036;
 const double scale=(.86+double((seed>>20)%101)/100.*.20)*(r.decorationAmount==2?1.12:1.);
 std::vector<Piece> result;std::vector<Rect> drawn;int primaryCorner=-1;
 for(int group=0;group<(r.decorationAmount==2?3:2);++group){
  const auto inks=group==2?edgeBoard(style):board(style,group!=0,variant);const auto preferred=group?(primaryCorner^3):mainCorner;
  double bestCost=1e10;int chosen=-1;std::vector<Piece> best;Rect bestBox;
  for(int k=0;k<4;++k){const auto corner=(preferred+k)%4;if(group==1&&corner==primaryCorner)continue;
   const double sx=(corner&1)?1.:-1.,sy=(corner&2)?1.:-1.;
   const double y=group==2?.444:.30+jitter+((style==5||style==3)?.025:0);
   const double x=group==2?.145:.50-(.202+jitter)*ch/cw;
   for(double fit:{1.,.74}){
    std::vector<Piece> pieces;Rect box;
    for(size_t n=0;n<inks.size();++n){const auto&ink=inks[n];Piece p{};p.label=ink.label;p.shape=ink.shape;p.plane=r.decorationPlane;
     const double size=unit*scale*fit*(group==1?(r.decorationAmount==2?1.38:.86):1.);
     // Register to the current camera plane. Keep world depth and explicit
     // anchor/custom offsets as authored instead of moving shapes to z=0.
     p.plane.position.x+=sx*(x*unit*cw/ch-ink.x*size);p.plane.position.y+=sy*(y*unit-ink.y*size);
     p.plane.rotation.z+=ink.angle*sx*sy;
     p.width=ink.w*size;p.height=ink.h*size;p.thickness=ink.stroke*size;p.spacing=ink.spacing*size;p.softness=std::max(.65,ink.softness*size);
     p.opacity=ink.alpha;p.order=int(result.size()+n);
     const auto paper=mixInk(r.colorB,{1,.98,.92},.72),dark=multiply(mixInk(r.colorA,r.colorB,.25),.29);
     p.color=ink.pigment==0?r.colorA:ink.pigment==1?r.colorB:ink.pigment==2?paper:dark;
     const auto projected=projectedBox(worldPlane(p.plane),p.width*(p.shape==Shape::Parallelogram?1.2:1.),p.height,scene.camera,cw,ch,r.screenFixed);if(area(projected)>0){include(box,projected.left,projected.top);include(box,projected.right,projected.bottom);}
     pieces.push_back(p);
    }
    if(area(box)<=0)continue;const double visible=area(intersection(box,{0,0,1,1}))/area(box);if(visible<.83)continue;
    // Extend the swept bounds by the maximum travel used while opaque.
    Rect swept{box.left-.025*ch/cw,box.top-.020,box.right+.025*ch/cw,box.bottom+.020};double obstruction=0;
    for(const auto&obstacle:occupied)obstruction=std::min(1.,obstruction+area(intersection(swept,obstacle))/area(swept));
    for(const auto&obstacle:drawn)obstruction=std::min(1.,obstruction+area(intersection(swept,obstacle))/area(swept));
    const double cost=obstruction+(1-visible)*.8+(1-fit)*.16+k*.003;
    if(cost<bestCost){bestCost=cost;chosen=corner;best=std::move(pieces);bestBox=swept;}
   }
  }
  if(chosen<0||bestCost>(group?.13:.25)){if(!group)break;continue;}
  ++groupCount;if(!group)primaryCorner=chosen;drawn.push_back(bestBox);
  const double sx=(chosen&1)?1.:-1.,sy=(chosen&2)?1.:-1.;
  for(size_t n=0;n<best.size();++n){auto&p=best[n];const double lag=group*.055+.012*n;
   const double in=style==0||style==6?.30:style==2||style==5||style==7?.13:.21;
   const double out=style==5||style==7?.14:.21;
   const auto envelope=[=](double t){return eased((t-lag)/in)*eased((1-t-lag*.6)/out);};
   p.motion.push_back(curve("opacity",1.,[=](double t){return envelope(t)-1.;}));
   const double travel=(style==5?.13:style==3?.036:style==7?.047:.024)*unit*r.strength;
   if(style==2){const auto pulse=[=](double t){return std::sin(13.*std::max(0.,t-lag))*std::exp(-10.*std::max(0.,t-lag))*envelope(t);};p.motion.push_back(curve("sx",.50*r.strength,pulse));p.motion.push_back(curve("sy",.50*r.strength,pulse));}
   else if(style==4&&p.shape==Shape::Radial)p.motion.push_back(curve("rz",22*r.strength,[=](double t){return std::sin(2*pi*t)*envelope(t);}));
   else if(style==7)p.motion.push_back(curve("x",sx*travel,[=](double t){return (t<lag+.09?.70:t<lag+.15?-.22:0.)*envelope(t);}));
   else if(style==0||style==6)p.motion.push_back(curve("y",sy*travel,[=](double t){return (1.-envelope(t))+.45*std::sin(2*pi*t)*envelope(t);}));
   else p.motion.push_back(curve("x",sx*travel,[=](double t){return 1-eased((t-lag)/in)-eased((t-.79-lag*.4)/.15);}));
   result.push_back(std::move(p));
  }

 }
 return result;
}
}
