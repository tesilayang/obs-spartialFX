#include "fx_catalog.hpp"
#include <QImage>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <map>
#include <utility>
namespace spatial {
void loadFxAsset(FxInstance&f,FxAsset kind){
 f.pixels.reset();f.assetError.clear();if(f.path.empty())return;
 // Repeated immutable snapshots share decoded pixels. The UI owns this cache.
 static thread_local std::map<std::pair<QString,FxAsset>,std::weak_ptr<const FxPixels>>cache;
 QFileInfo info(QString::fromStdString(f.path));const auto key=std::make_pair(info.absoluteFilePath()+":"+QString::number(info.size())+":"+QString::number(info.lastModified().toMSecsSinceEpoch()),kind);
 if(auto p=cache[key].lock()){f.pixels=std::move(p);return;}
 auto pixels=std::make_shared<FxPixels>();
 if(kind==FxAsset::Lut&&info.suffix().compare("cube",Qt::CaseInsensitive)==0){
  QFile file(info.absoluteFilePath());if(!file.open(QIODevice::ReadOnly|QIODevice::Text)){f.assetError="无法读取 LUT 文件";return;}
  QTextStream stream(&file);int n=0;std::vector<Vec3>values;bool failed=false;
  while(!stream.atEnd()){
   const auto line=stream.readLine().split('#').first().trimmed();if(line.isEmpty()||line.startsWith("TITLE"))continue;auto fields=line.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);
   if(fields[0]=="LUT_3D_SIZE"){bool ok=false;n=fields.value(1).toInt(&ok);if(fields.size()!=2||!ok||n<2||n>64){failed=true;break;}}
   else if(fields[0]=="DOMAIN_MIN"||fields[0]=="DOMAIN_MAX"){if(fields.size()!=4){failed=true;break;}bool a,b,c;Vec3 v{fields[1].toDouble(&a),fields[2].toDouble(&b),fields[3].toDouble(&c)};if(!a||!b||!c||!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)){failed=true;break;}auto&domain=fields[0]=="DOMAIN_MIN"?pixels->minimum:pixels->maximum;domain=v;}
   else if(fields[0]=="LUT_1D_SIZE"){failed=true;break;}
   else {if(fields.size()!=3||values.size()>size_t(64*64*64)){failed=true;break;}bool a,b,c;Vec3 v{fields[0].toDouble(&a),fields[1].toDouble(&b),fields[2].toDouble(&c)};if(!a||!b||!c||!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)){failed=true;break;}values.push_back(v);}
  }
  if(failed||n<2||values.size()!=size_t(n*n*n)||pixels->maximum.x<=pixels->minimum.x||pixels->maximum.y<=pixels->minimum.y||pixels->maximum.z<=pixels->minimum.z){f.assetError="LUT 无效：需要 2–64 阶、数据完整的 3D .cube";return;}
  pixels->width=uint32_t(n*n);pixels->height=uint32_t(n);pixels->lutSize=n;pixels->pixels.resize(size_t(n*n*n*4));
  for(int b=0;b<n;++b)for(int g=0;g<n;++g)for(int r=0;r<n;++r){const auto v=values[size_t(r+g*n+b*n*n)];auto offset=size_t((b*n+r+g*n*n)*4);pixels->pixels[offset]=uint8_t(std::lround(std::clamp(v.x,0.0,1.0)*255));pixels->pixels[offset+1]=uint8_t(std::lround(std::clamp(v.y,0.0,1.0)*255));pixels->pixels[offset+2]=uint8_t(std::lround(std::clamp(v.z,0.0,1.0)*255));pixels->pixels[offset+3]=255;}
 }else{
  QImage image(info.absoluteFilePath());if(image.isNull()){f.assetError="图像文件无法读取";return;}
  if(kind==FxAsset::Lut){int n=image.height();if(n<2||n>64||image.width()!=n*n){f.assetError="PNG LUT 必须是 N×N² 横条布局，N 为 2–64";return;}pixels->lutSize=n;}
  else if(image.width()>2048||image.height()>2048)image=image.scaled(2048,2048,Qt::KeepAspectRatio,Qt::SmoothTransformation);
  image=image.convertToFormat(QImage::Format_RGBA8888_Premultiplied);pixels->width=uint32_t(image.width());pixels->height=uint32_t(image.height());pixels->pixels.resize(size_t(image.width()*image.height()*4));for(int y=0;y<image.height();++y)std::copy_n(image.constScanLine(y),image.width()*4,pixels->pixels.data()+size_t(y*image.width()*4));
 }
 f.pixels=pixels;cache[key]=pixels;if(cache.size()>256){for(auto it=cache.begin();it!=cache.end();)if(it->second.expired())it=cache.erase(it);else++it;}
}
}
