#include "project/project_obs.hpp"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QSet>
#include <QProcess>
#include <atomic>
#include <iostream>
using namespace spatial::projects;
static void require(bool ok,const char*s){if(!ok){std::cerr<<"FAIL "<<s<<'\n';std::exit(1);}std::cout<<"PASS "<<s<<'\n';}
int main(int argc,char**argv){QCoreApplication app(argc,argv);QString error;
 QJsonObject doc{{"format","obs-spatial-fx-project"},{"version",1},{"scene_name",QString::fromUtf8("场景 B")},{"root_uuid","root"},{"sources",QJsonArray{QJsonObject{{"uuid","root"},{"name",QString::fromUtf8("场景 B")},{"id","scene"},{"settings",QJsonObject{}}}}}};
 require(validateDocument(doc,error),"valid scene document accepted");auto invalid=doc;invalid["root_uuid"]="absent";require(!validateDocument(invalid,error),"missing root rejected before mutation");
 invalid=doc;invalid["version"]=900;require(!validateDocument(invalid,error),"future schema rejected");
 QTemporaryDir tmp;QString path=tmp.path()+QString::fromUtf8("/原始.png");QFile f(path);require(f.open(QIODevice::WriteOnly),"fixture open");f.write("original-bytes");f.close();
 auto sources=doc["sources"].toArray();sources.append(QJsonObject{{"uuid","image"},{"id","image_source"},{"name","image"},{"settings",QJsonObject{{"file",path},{"file2",path},{"local_file",tmp.path()+"/missing.mp4"}}}});doc["sources"]=sources;
 discoverAssets(doc);auto assets=doc["assets"].toArray();require(assets.size()==2,"same local file deduplicated and missing retained");require(assets.first().toObject()["pointers"].toArray().size()==2,"all uses recorded for relink");require(missingAssets(doc).size()==1,"missing local material reported");
 QString id;for(auto a:assets)if(a.toObject()["original_path"].toString().endsWith("missing.mp4"))id=a.toObject()["id"].toString();
 require(relinkAsset(doc,id,path,error),"manual relink succeeds");require(missingAssets(doc).isEmpty(),"relink clears missing asset");require(doc["sources"].toArray()[1].toObject()["settings"].toObject()["local_file"]==path,"relink rewrites actual OBS setting");
 invalid=doc;auto bad=invalid["sources"].toArray();bad.append(bad.first());invalid["sources"]=bad;require(!validateDocument(invalid,error),"duplicate UUID rejected");
 auto withSettings=[&](const QJsonArray &settings){auto copy=doc;QJsonArray entries;int n=0;for(const auto &setting:settings)entries.append(QJsonObject{{"uuid",QString("source-%1").arg(n++)},{"id","image_source"},{"settings",setting.toObject()}});copy["sources"]=entries;return copy;};
 auto device=withSettings(QJsonArray{QJsonObject{{"file",QStringLiteral("\\\\.\\pipe\\spatial-device")},{"path",QStringLiteral("\\\\?\\GLOBALROOT\\Device\\HarddiskVolume1\\asset.png")}}});discoverAssets(device);require(device["assets"].toArray().isEmpty(),"Windows device namespace paths excluded from local assets");require(!device["requirements"].toArray().isEmpty(),"excluded device paths explicitly reported");
 auto unc=withSettings(QJsonArray{QJsonObject{{"file",QStringLiteral("\\\\localhost\\spatial-missing-test-share\\asset.png")}}});discoverAssets(unc);require(unc["assets"].toArray().size()==1,"ordinary UNC path retained as local asset");
 const QString web=tmp.path()+"/web",nested=web+"/nested";require(QDir().mkpath(nested),"browser fixture directories created");
 auto write=[&](const QString &name){QFile file(name);require(file.open(QIODevice::WriteOnly),"browser fixture file opened");file.write("fixture");};
 const QString image=web+"/shared.png",html=web+"/index.html",nestedImage=nested+"/inner.png";write(image);write(html);write(nestedImage);
 auto browser=withSettings(QJsonArray{QJsonObject{{"file",image}},QJsonObject{{"local_file",html}},QJsonObject{{"folder",nested}},QJsonObject{{"folder",web}}});discoverAssets(browser);
 QMap<QString,QString> placements;QSet<QString> sharedPlacements,innerPlacements;QString browserHtml;
 for(const auto &value:browser["assets"].toArray()){const auto asset=value.toObject();const auto original=asset["original_path"].toString(),relative=asset["relative_path"].toString();if(original==image)sharedPlacements.insert(relative);if(original==nestedImage)innerPlacements.insert(relative);for(const auto &pointer:asset["pointers"].toArray()){placements[pointer.toString()]=relative;if(pointer=="/sources/1/settings/local_file")browserHtml=relative;}}
 require(sharedPlacements.size()==2,"direct asset and browser folder each retain required shared image placement");require(!browserHtml.isEmpty()&&sharedPlacements.contains(QFileInfo(browserHtml).path()+"/shared.png"),"browser entry retains sibling image beside HTML");require(innerPlacements.size()==2,"overlapping folders retain inner file in every required placement");
 auto missingDirectory=withSettings(QJsonArray{QJsonObject{{"folder",tmp.path()+"/missing-folder"}}});discoverAssets(missingDirectory);require(missingDirectory["directories"].toArray().size()==1,"missing folder retained as directory dependency");require(missingAssets(missingDirectory).contains(tmp.path()+"/missing-folder"),"missing directory reported instead of silently dropped");
 require(relinkAsset(missingDirectory,missingDirectory["directories"].toArray().first().toObject()["id"].toString(),web,error),"missing directory can be manually relinked");require(missingDirectory["sources"].toArray().first().toObject()["settings"].toObject()["folder"]==web,"directory relink rewrites actual source setting");require(missingAssets(missingDirectory).isEmpty(),"directory relink clears missing dependency");
 const QString link=web+"/junction",outside=tmp.path()+"/outside";require(QDir().mkpath(outside),"reparse fixture target created");write(outside+"/outside.png");QProcess junction;junction.start("cmd.exe",{"/c","mklink","/J",QDir::toNativeSeparators(link),QDir::toNativeSeparators(outside)});require(junction.waitForFinished(10000)&&junction.exitCode()==0,"Windows junction fixture created");auto linked=withSettings(QJsonArray{QJsonObject{{"folder",web}}});discoverAssets(linked);bool reported=false,followed=false;for(const auto &value:linked["requirements"].toArray())reported|=value.toString().contains(link);for(const auto &value:linked["assets"].toArray())followed|=value.toObject()["original_path"].toString().contains("outside.png");require(reported&&!followed,"reparse directory skipped with explicit warning");require(QDir().rmdir(link),"junction fixture removed without touching target");
 auto pointerFixture=withSettings(QJsonArray{QJsonObject{{"file",path}}});discoverAssets(pointerFixture);const QJsonArray badPointers{"/sources/0/settings/missing","/sources/0/settings/file/value","/sources/0/settings/file~2","/sources/00/settings/file","/sources/9/settings/file","/sources/0/settings",42};for(const auto &pointer:badPointers){auto broken=pointerFixture;auto entries=broken["assets"].toArray();auto first=entries.first().toObject();first["pointers"]=QJsonArray{"/sources/0/settings/file",pointer};entries[0]=first;broken["assets"]=entries;const auto before=broken;require(!relinkAsset(broken,first["id"].toString(),image,error)&&broken==before,"invalid JSON pointer rejected without partial relink");}
 std::atomic_bool canceled{true};auto canceledDocument=browser;const auto beforeCancel=canceledDocument;discoverAssets(canceledDocument,&canceled);require(canceledDocument==beforeCancel,"pre-canceled discovery leaves document unchanged");
 return 0;
}
