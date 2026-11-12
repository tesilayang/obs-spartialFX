#include "fx_catalog.hpp"
#include <algorithm>
namespace spatial {
const std::array<FxGroup,13>&fxGroups(){
 static const std::array<FxGroup,13>g={{{"画面模糊","虚化、磨砂、散景、方向模糊、运动模糊和拖尾。"},{"画质修正","锐化、降噪、空间抗锯齿和轻量 RCAS 清晰化。"},{"颜色与调色","调节明暗、白平衡、颜色分区、曲线和 LUT。"},{"光晕与镜头光效","辉光、星芒、色散、暗角、光束与镜头反光。"},{"受光与阴影","环境受光、柔光、高光材质和投影。始终使用随素材移动的独立附加光源，与灯光页主灯和舞台灯光开关分开。"},{"轮廓描边","透明轮廓的内外描边、内发光与画面边缘线稿。"},{"天气设置","雨窗、流云、水波、热浪、火焰、余烬与烟花。"},{"故障与复古屏幕","数字故障、电视磁带、CRT、扫描、颗粒和科幻屏幕。"},{"绘画与像素风格","漫画、卡通、素描、油画、掌机像素与字符画。"},{"形变与纹理","透视、弯曲、鱼眼、折射、置换与重复排列。"},{"立体与自动运动","立体厚度、抖动、呼吸和闪烁。运动使用与时间轴、灯光相同的空间坐标。"},{"裁切、遮罩与合成","形状裁切、素材遮罩、透明处理、纹理覆盖和混合。"},{"舞台与风格预设","由基础处理组合的 LED 舞台、宽银幕、柔光与眩晕风格。"}}};return g;
}
const std::vector<FxDefinition>&fxCatalog(){
 static const auto catalog=[](){std::vector<FxDefinition>result;auto add=[&](FxDefinition d){result.push_back(std::move(d));};
#include "fx_catalog_a.inc"
#include "fx_catalog_b.inc"
#include "fx_catalog_c.inc"
 return result;}();return catalog;
}
const FxDefinition*fxDefinition(const std::string&id){for(auto&d:fxCatalog())if(d.id==id)return &d;return nullptr;}
FxInstance makeFx(const FxDefinition&d){FxInstance f;f.id=d.id;const auto count=std::min(d.parameters.size(),f.values.size());for(size_t i=0;i<count;++i)f.values[i]=d.parameters[i].initial;return f;}
FxInstance*findFx(FxStack&s,const std::string&id){for(auto&f:s.effects)if(f.id==id)return &f;return nullptr;}
const FxInstance*findFx(const FxStack&s,const std::string&id){for(auto&f:s.effects)if(f.id==id)return &f;return nullptr;}
const FxInstance*activeFx(const FxStack&s,const FxDefinition&d){if(!s.attached||d.group<1||d.group>13||!s.groups[size_t(d.group-1)])return nullptr;auto*f=findFx(s,d.id);if(d.id=="FX02.3"){auto*other=findFx(s,"FX02.2");if(other&&other->enabled&&other->strength>0)return nullptr;}return f&&f->enabled&&f->strength>0?f:nullptr;}
bool hasActiveFx(const FxStack&s){for(auto&d:fxCatalog())if(activeFx(s,d))return true;return false;}
}
