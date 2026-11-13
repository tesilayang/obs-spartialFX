#include "project_obs.hpp"
#include <QJsonArray>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QSet>
#include <QRegularExpression>
#include <QDirIterator>
#include <functional>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace spatial::projects {
namespace {
QString escape(QString s){return s.replace("~","~0").replace("/","~1");}
bool devicePath(QString path){path.replace('\\','/');return path.startsWith("//./",Qt::CaseInsensitive)||path.startsWith("//?/",Qt::CaseInsensitive)||path.startsWith("/??/",Qt::CaseInsensitive)||path.startsWith("/Device/",Qt::CaseInsensitive)||path.startsWith("/DosDevices/",Qt::CaseInsensitive);}
bool directoryField(const QString &key){static const QSet<QString> fields={"directory","directory_path","folder","folder_path","image_dir"};return fields.contains(key);}
bool reparsePoint(const QString &path){
 if(QFileInfo(path).isSymLink())return true;
#ifdef Q_OS_WIN
 const DWORD attributes=GetFileAttributesW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()));return attributes!=INVALID_FILE_ATTRIBUTES&&(attributes&FILE_ATTRIBUTE_REPARSE_POINT);
#else
 return false;
#endif
}
bool reparseAncestor(QString path){path=QFileInfo(path).absoluteFilePath();for(;;){if(reparsePoint(path))return true;const auto parent=QFileInfo(path).absolutePath();if(parent==path)return false;path=parent;}}
bool likelyFile(const QString&key,const QString&s){
 if(s.isEmpty()||s.contains("://")||s.startsWith("data:"))return false;
 static const QSet<QString> fields={"file","file2","local_file","path","image_path","mask_path","lut_path","filename","font_file","playlist_file","sound_file"};
 return fields.contains(key)||directoryField(key)||QRegularExpression("^[a-zA-Z]:[\\\\/]").match(s).hasMatch()||s.startsWith("\\\\");
}
bool replace(QJsonValue &value,const QStringList&parts,int n,const QString&path){
 if(n==parts.size()){if(!value.isString())return false;value=path;return true;}
 QString key=parts[n];for(int i=0;i<key.size();++i)if(key[i]=='~'){if(i+1>=key.size()||(key[i+1]!='0'&&key[i+1]!='1'))return false;++i;}key.replace("~1","/").replace("~0","~");
 if(value.isArray()){auto array=value.toArray();bool valid=false;int index=key.toInt(&valid);if(!valid||index<0||index>=array.size()||QString::number(index)!=key)return false;QJsonValue child=array[index];if(!replace(child,parts,n+1,path))return false;array[index]=child;value=array;return true;}
 if(value.isObject()){auto object=value.toObject();if(!object.contains(key))return false;QJsonValue child=object[key];if(!replace(child,parts,n+1,path))return false;object[key]=child;value=object;return true;}
 return false;
}
}
bool validateDocument(const QJsonObject&doc,QString&error){
 error.clear();if(doc["format"]!="obs-spatial-fx-project"||doc["version"].toInt()!=1){error=QString::fromUtf8("工程格式或版本不支持。");return false;}
 const auto sources=doc["sources"].toArray();const QString root=doc["root_uuid"].toString();
 if(doc["scene_name"].toString().trimmed().isEmpty()||sources.isEmpty()||sources.size()>10000){error=QString::fromUtf8("工程场景名称或来源数量无效。");return false;}
 QSet<QString> ids;bool found=false;
 for(auto v:sources){auto s=v.toObject();const QString uuid=s["uuid"].toString();if(uuid.isEmpty()||ids.contains(uuid)||s["id"].toString().isEmpty()||!s["settings"].isObject()){error=QString::fromUtf8("来源标识重复或配置无效。");return false;}ids.insert(uuid);if(uuid==root)found=s["id"]=="scene";}
 if(!found){error=QString::fromUtf8("工程根场景不存在。");return false;}
 return true;
}
void discoverAssets(QJsonObject&doc,std::atomic_bool *cancellation){
 const auto canceled=[cancellation]{return cancellation&&cancellation->load(std::memory_order_relaxed);};if(canceled())return;
 QJsonArray assets,directories,requirements;QMap<QString,int> indexes,folderIndexes,directoryPlacements;QSet<QString> warnings;
 const QString warningPrefix=QString::fromUtf8("素材依赖：");for(const auto &requirement:doc["requirements"].toArray())if(!requirement.toString().startsWith(warningPrefix))requirements.append(requirement);
 auto warn=[&](const QString &message){if(warnings.contains(message))return;warnings.insert(message);requirements.append(warningPrefix+message);};
 auto normalized=[](const QString &path){return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).normalized(QString::NormalizationForm_C).toCaseFolded();};
 auto add=[&](const QString&raw,const QString&relative,const QString&pointer){
  if(canceled())return;
  QFileInfo file(raw);const QString path=QDir::cleanPath(file.absoluteFilePath());
  // A single original can require several package placements. An HTML sibling
  // and a separate OBS image must not steal each other's destination.
  const QString placement=normalized(path)+QChar('\n')+relative.toCaseFolded();int index;
  if(indexes.contains(placement))index=indexes[placement];else{index=assets.size();indexes[placement]=index;const QString id=QString("asset-%1").arg(index+1,5,10,QChar('0'));assets.append(QJsonObject{{"id",id},{"original_path",path},{"relative_path",relative.isEmpty()?"assets/"+id+"/"+file.fileName():relative},{"size",file.exists()?file.size():-1},{"sha256",""},{"pointers",QJsonArray{}}});}
  if(!pointer.isEmpty()){auto asset=assets[index].toObject();auto pointers=asset["pointers"].toArray();if(!pointers.contains(pointer))pointers.append(pointer);asset["pointers"]=pointers;assets[index]=asset;}
  if(!file.isFile()||!file.isReadable())warn(QString::fromUtf8("文件缺失或不可读：")+path);else if(reparseAncestor(path))warn(QString::fromUtf8("文件路径包含符号链接或重解析点，无法安全打包：")+path);
 };
 auto addDirectory=[&](const QString &original,const QString &relative,const QString &pointer){int index;const auto placement=relative.toCaseFolded();if(directoryPlacements.contains(placement))index=directoryPlacements[placement];else{index=directories.size();directoryPlacements[placement]=index;directories.append(QJsonObject{{"id",QString("directory-%1").arg(index+1,5,10,QChar('0'))},{"original_path",QFileInfo(original).absoluteFilePath()},{"relative_path",relative},{"pointers",QJsonArray{}}});}if(!pointer.isEmpty()){auto entry=directories[index].toObject();auto pointers=entry["pointers"].toArray();if(!pointers.contains(pointer))pointers.append(pointer);entry["pointers"]=pointers;directories[index]=entry;}return index;};
 auto addFolder=[&](const QString&path,const QString&pointer){
  if(canceled())return QString();
  const auto norm=normalized(path);QString relative;
  if(folderIndexes.contains(norm))relative=directories[folderIndexes[norm]].toObject()["relative_path"].toString();else{
   relative=QString("assets/folder-%1").arg(folderIndexes.size()+1,5,10,QChar('0'));folderIndexes[norm]=addDirectory(path,relative,{});const QFileInfo info(path);
   if(!info.isDir()||!info.isReadable())warn(QString::fromUtf8("文件夹缺失或不可读：")+info.absoluteFilePath());
   else if(reparseAncestor(path))warn(QString::fromUtf8("文件夹包含符号链接或重解析点，未扫描：")+info.absoluteFilePath());
   else{QDir root(path);QDirIterator iterator(path,QDir::Files|QDir::Dirs|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);while(!canceled()&&iterator.hasNext()){iterator.next();if(canceled())break;const auto child=iterator.fileInfo();if(reparseAncestor(child.absoluteFilePath())){warn(QString::fromUtf8("已跳过符号链接或重解析点依赖：")+child.absoluteFilePath());continue;}const auto destination=relative+"/"+root.relativeFilePath(child.absoluteFilePath());if(child.isDir())addDirectory(child.absoluteFilePath(),destination,{});else if(child.isFile())add(child.absoluteFilePath(),destination,{});}}
  }
  addDirectory(path,relative,pointer);return relative;
 };
 std::function<void(const QJsonValue&,const QString&,const QString&)> walk;
 walk=[&](const QJsonValue&v,const QString&pointer,const QString&key){
  if(canceled())return;
  if(v.isObject()){auto o=v.toObject();for(auto it=o.begin();it!=o.end()&&!canceled();++it)walk(it.value(),pointer+"/"+escape(it.key()),it.key());}
  else if(v.isArray()){auto a=v.toArray();for(int i=0;i<a.size()&&!canceled();++i)walk(a[i],pointer+"/"+QString::number(i),key);}
  else if(v.isString()){
   const QString raw=v.toString();if(devicePath(raw)){warn(QString::fromUtf8("设备命名空间路径不作为本地素材复制：")+raw);return;}if(!likelyFile(key,raw))return;
   QFileInfo info(raw);if(info.isDir()||directoryField(key)){addFolder(raw,pointer);return;}
   if(info.isFile()&&(info.suffix().compare("html",Qt::CaseInsensitive)==0||info.suffix().compare("htm",Qt::CaseInsensitive)==0)){const auto relative=addFolder(info.absolutePath(),{});add(raw,relative+"/"+info.fileName(),pointer);return;}
   add(raw,{},pointer);
  }
 };
 walk(doc["sources"],"/sources","");if(canceled())return;doc["assets"]=assets;doc["directories"]=directories;doc["requirements"]=requirements;
}
QStringList missingAssets(const QJsonObject&doc){QStringList missing;for(auto v:doc["assets"].toArray()){auto o=v.toObject();QFileInfo f(o["original_path"].toString());if(!f.isFile()||!f.isReadable()||reparseAncestor(f.absoluteFilePath()))missing<<o["original_path"].toString();}for(auto v:doc["directories"].toArray()){auto o=v.toObject();QFileInfo f(o["original_path"].toString());if(!f.isDir()||!f.isReadable()||reparseAncestor(f.absoluteFilePath()))missing<<o["original_path"].toString();}missing.removeDuplicates();return missing;}
bool relinkAsset(QJsonObject&doc,const QString&id,const QString&path,QString&error){
 QFileInfo replacement(path);auto assets=doc["assets"].toArray(),directories=doc["directories"].toArray();QString original;bool directory=false;
 for(const auto &value:assets)if(value.toObject()["id"]==id){original=value.toObject()["original_path"].toString();break;}
 if(original.isEmpty())for(const auto &value:directories)if(value.toObject()["id"]==id){original=value.toObject()["original_path"].toString();directory=true;break;}
 if(original.isEmpty()){error=QString::fromUtf8("未找到素材记录。");return false;}
 if((directory?!replacement.isDir():!replacement.isFile())||!replacement.isReadable()||reparseAncestor(replacement.absoluteFilePath())){error=QString::fromUtf8("替代路径不存在、不可读或包含重解析点。");return false;}
 const auto old=QDir::cleanPath(QFileInfo(original).absoluteFilePath()),newPath=QDir::cleanPath(replacement.absoluteFilePath());auto updated=doc;
 auto rewrite=[&](QJsonArray &entries,bool isDirectory){for(int i=0;i<entries.size();++i){auto entry=entries[i].toObject();const auto current=QDir::cleanPath(QFileInfo(entry["original_path"].toString()).absoluteFilePath());QString target;if(current.compare(old,Qt::CaseInsensitive)==0)target=newPath;else if(directory&&current.startsWith(old+"/",Qt::CaseInsensitive))target=newPath+current.mid(old.size());else continue;if(!entry["pointers"].isArray()){error=QString::fromUtf8("素材引用路径列表无效。");return false;}for(const auto &value:entry["pointers"].toArray()){const auto pointer=value.toString();QJsonValue candidate(updated);if(!value.isString()||!pointer.startsWith("/sources/")||!replace(candidate,pointer.mid(1).split('/'),0,target)){error=QString::fromUtf8("素材引用路径不存在或格式无效。");return false;}updated=candidate.toObject();}entry["original_path"]=target;if(!isDirectory){entry["size"]=QFileInfo(target).isFile()?QFileInfo(target).size():-1;entry["sha256"]="";}entries[i]=entry;}return true;};
 if(!rewrite(assets,false)||!rewrite(directories,true))return false;updated["assets"]=assets;updated["directories"]=directories;QJsonArray requirements;for(const auto &value:updated["requirements"].toArray()){const auto message=value.toString();if(message.startsWith(QString::fromUtf8("素材依赖："))&&message.contains(old,Qt::CaseInsensitive))continue;requirements.append(value);}updated["requirements"]=requirements;doc=updated;error.clear();return true;
}
}
