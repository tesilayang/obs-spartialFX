#include "project/project_archive.hpp"
#include "project/vendor/miniz/miniz.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QtEndian>
#include <cstdio>
#include <stdexcept>
#ifdef Q_OS_WIN
#include <windows.h>
#include <winioctl.h>
#endif
using namespace spatial::projects;
static void require(bool value, const QString &what) { if (!value) throw std::runtime_error(what.toStdString()); }
static void save(const QString &path, const QByteArray &data) { QFile f(path); require(f.open(QIODevice::WriteOnly), path); require(f.write(data) == data.size(), path); }
static QByteArray load(const QString &path) { QFile f(path); require(f.open(QIODevice::ReadOnly), path); return f.readAll(); }
static QJsonObject document(const QString &path, const QString &relative = QStringLiteral("assets/素材.bin")) {
 return {{"format", "obs-spatial-fx-project"}, {"version", 1}, {"scene_name", "archive test"}, {"sources", QJsonArray{QJsonObject{{"settings", QJsonObject{{"file", path}}}}}},
 {"assets", QJsonArray{QJsonObject{{"id", "a1"}, {"original_path", path}, {"relative_path", relative}, {"pointers", QJsonArray{"/sources/0/settings/file"}}}}}};
}
static QByteArray fixture(const QList<QPair<QByteArray,QByteArray>> &entries, bool zip64 = false, int compression = 6) {
 mz_zip_archive zip{}; require(mz_zip_writer_init_heap_v2(&zip,0,0,zip64 ? MZ_ZIP_FLAG_WRITE_ZIP64 : 0), "fixture init");
 for (const auto &entry : entries) require(mz_zip_writer_add_mem(&zip,entry.first.constData(),entry.second.constData(),static_cast<size_t>(entry.second.size()),static_cast<mz_uint>(compression)), "fixture add");
 void *bytes = nullptr; size_t size = 0;
 require(mz_zip_writer_finalize_heap_archive(&zip,&bytes,&size), "fixture data");
 QByteArray result(static_cast<const char *>(bytes),static_cast<qsizetype>(size)); mz_free(bytes); mz_zip_writer_end(&zip); return result;
}
static QByteArray manifestFor(const QByteArray &data, const QString &relative = "assets/file.bin") {
 auto doc = document("irrelevant",relative); auto assets = doc["assets"].toArray(); auto asset = assets[0].toObject();
 asset["size"] = data.size(); asset["sha256"] = QString::fromLatin1(QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex()); assets[0] = asset; doc["assets"] = assets;
 return QJsonDocument(doc).toJson();
}
static void patch32(QByteArray &data, qsizetype at, quint32 value) { require(at>=0 && at+4<=data.size(),"patch location"); qToLittleEndian(value,data.data()+at); }
static void assertRejected(const QString &root, const QByteArray &bytes, const QString &label) {
 static int serial = 0; const QString zip=root+"/reject-"+QString::number(++serial)+".zip", target=root+"/reject-dir-"+QString::number(serial);
 save(zip,bytes); QString error; std::atomic_bool cancel{false};
 require(!extractProjectPackage(zip,target,cancel,{},error), label+" was accepted"); require(!error.isEmpty(),label+" error missing"); require(!QFileInfo::exists(target),label+" published a destination");
 require(QDir(root).entryList({".spatial-*"},QDir::AllEntries|QDir::Hidden|QDir::NoDotAndDotDot).isEmpty(),label+" left staging files");
}
#ifdef Q_OS_WIN
static bool junction(const QString &path, const QString &target) {
 if (!QDir().mkpath(path)) return false;
 const HANDLE handle=CreateFileW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr);
 if(handle==INVALID_HANDLE_VALUE)return false;
 const QString substitute="\\??\\"+QDir::toNativeSeparators(target), display=QDir::toNativeSeparators(target);
 QByteArray buffer(8+8+(substitute.size()+1+display.size()+1)*2,0);
 patch32(buffer,0,IO_REPARSE_TAG_MOUNT_POINT); qToLittleEndian<quint16>(static_cast<quint16>(buffer.size()-8),buffer.data()+4);
 qToLittleEndian<quint16>(0,buffer.data()+8); qToLittleEndian<quint16>(static_cast<quint16>(substitute.size()*2),buffer.data()+10);
 qToLittleEndian<quint16>(static_cast<quint16>((substitute.size()+1)*2),buffer.data()+12); qToLittleEndian<quint16>(static_cast<quint16>(display.size()*2),buffer.data()+14);
 memcpy(buffer.data()+16,substitute.utf16(),static_cast<size_t>(substitute.size()*2)); memcpy(buffer.data()+16+(substitute.size()+1)*2,display.utf16(),static_cast<size_t>(display.size()*2));
 DWORD returned=0; const bool ok=DeviceIoControl(handle,FSCTL_SET_REPARSE_POINT,buffer.data(),static_cast<DWORD>(buffer.size()),nullptr,0,&returned,nullptr)!=0; CloseHandle(handle); return ok;
}
#endif
int main(int argc, char **argv) {
 QCoreApplication app(argc, argv);
 try {
 QTemporaryDir temp; require(temp.isValid(), "temporary directory"); const QString root = temp.path();
 const QString original = root + "/原始.bin"; const QByteArray data("original bytes\0\xff", 17); save(original, data);
 auto doc = document(original); QString error; std::atomic_bool cancel{false};
 const QString zip = root + "/工程.zip", extracted = root + "/unpacked";
 const bool written = writeProjectPackage(doc, zip, cancel, {}, error); require(written, "export: " + error);
 require(load(zip).contains(QByteArray("PK\x06\x06", 4)), "export is ZIP64");
 const bool unpacked = extractProjectPackage(zip, extracted, cancel, {}, error); require(unpacked, "extract: " + error);
 const auto restored = readProjectFolder(extracted, cancel, {}, error);
 require(!restored.isEmpty(), "read: " + error);
 const QString absolute = restored["sources"].toArray()[0].toObject()["settings"].toObject()["file"].toString();
 require(QFileInfo(absolute).isAbsolute() && load(absolute) == data, "assets copied and pointers rewritten");
 const auto asset = restored["assets"].toArray()[0].toObject();
 require(asset["original_path"].toString() == absolute, "reexport uses restored asset");
 require(asset["sha256"].toString().toLatin1() == QCryptographicHash::hash(data,QCryptographicHash::Sha256).toHex(), "sha metadata");
 const QString longChinese="assets/"+QString(180,QChar(0x7d20))+".bin";
 require(writeProjectPackage(document(original,longChinese),root+"/long-chinese.zip",cancel,{},error),"long Chinese name export: "+error);
 require(extractProjectPackage(root+"/long-chinese.zip",root+"/long-chinese",cancel,{},error),"long Chinese name import: "+error);
 require(load(root+"/long-chinese/"+longChinese)==data,"long Chinese filename preserves bytes");
 save(absolute, "broken");
 require(readProjectFolder(extracted, cancel, {}, error).isEmpty() && !error.isEmpty(), "SHA/size corruption rejected");
 require(!extractProjectPackage(zip, extracted, cancel, {}, error), "existing destination rejected");
 require(load(zip).size() > 0, "original package preserved");
 cancel = true; require(!writeProjectPackage(doc, root + "/cancel.zip", cancel, {}, error), "cancel export");
 require(!QFileInfo::exists(root + "/cancel.zip"), "cancel has no output");
 require(!extractProjectPackage(zip, root + "/cancel-dir", cancel, {}, error), "cancel extraction");
 require(!QFileInfo::exists(root + "/cancel-dir"), "cancel extraction has no output"); cancel = false;
 require(!writeProjectPackage(document(original,"../escape"), root + "/bad.zip", cancel, {}, error), "traversal rejected");
 require(!QFileInfo::exists(root + "/bad.zip"), "failure has no output");
 const QByteArray payload(200000,'x');
 const auto normal = fixture({{"assets/file.bin",payload},{"project.json",manifestFor(payload)}});
 save(root+"/deflated.zip",normal);
 const bool deflated=extractProjectPackage(root+"/deflated.zip",root+"/deflated",cancel,{},error); require(deflated,"normal deflate: "+error);
 require(load(root+"/deflated/assets/file.bin")==payload,"normal deflate content");
 const auto zip64Deflated=fixture({{"assets/file.bin",payload},{"project.json",manifestFor(payload)}},true);
 save(root+"/zip64-deflated.zip",zip64Deflated);
 const bool zip64Good=extractProjectPackage(root+"/zip64-deflated.zip",root+"/zip64-deflated",cancel,{},error); require(zip64Good,"ZIP64 deflate/descriptors: "+error);
 for (const QByteArray &name : {QByteArray("../escape"),QByteArray("C:/evil"),QByteArray("a\\evil"),QByteArray("assets/NUL.txt"),QByteArray("assets/file:stream"),QByteArray("assets/trailing. ")})
  assertRejected(root,fixture({{name,"evil"},{"project.json",manifestFor("evil")}}),"unsafe "+QString::fromLatin1(name));
 auto absoluteZip=fixture({{"x/escape","evil"},{"project.json",manifestFor("evil")}}); absoluteZip.replace("x/escape","/escapeX"); assertRejected(root,absoluteZip,"absolute name");
 assertRejected(root,fixture({{"assets/A","one"},{"assets/a","two"},{"project.json",manifestFor("one")}}),"casefold duplicate");
 assertRejected(root,fixture({{"assets/A","one"},{"assets/A","two"},{"project.json",manifestFor("one")}}),"duplicate name");
 assertRejected(root,fixture({{"assets","file"},{"assets/a","two"},{"project.json",manifestFor("one")}}),"file directory conflict");
 auto symlink=fixture({{"assets/file.bin",payload},{"project.json",manifestFor(payload)}}); auto central=symlink.indexOf(QByteArray("PK\x01\x02",4));
 patch32(symlink,central+38,0120777u<<16); assertRejected(root,symlink,"symlink archive entry");
 auto reparse=fixture({{"assets/file.bin",payload},{"project.json",manifestFor(payload)}}); central=reparse.indexOf(QByteArray("PK\x01\x02",4));
 patch32(reparse,central+38,0x400); assertRejected(root,reparse,"reparse archive entry");
 auto crc=fixture({{"assets/file.bin",data},{"project.json",manifestFor(data)}},false,0); const auto content=crc.indexOf(data); require(content>=0,"stored fixture bytes"); crc[content]=char(crc[content]^0x44); assertRejected(root,crc,"CRC corruption");
 auto sizes=normal; central=sizes.indexOf(QByteArray("PK\x01\x02",4)); patch32(sizes,central+24,400000); assertRejected(root,sizes,"bad uncompressed size");
 auto truncated=normal.left(normal.size()-17); assertRejected(root,truncated,"truncated archive");
 auto shaDoc=QJsonDocument::fromJson(manifestFor(payload)).object(); auto shaAssets=shaDoc["assets"].toArray(); auto shaAsset=shaAssets[0].toObject(); shaAsset["sha256"]=QString(64,'0'); shaAssets[0]=shaAsset; shaDoc["assets"]=shaAssets;
 assertRejected(root,fixture({{"assets/file.bin",payload},{"project.json",QJsonDocument(shaDoc).toJson()}}),"SHA mismatch");
 save(root+"/large.bin",QByteArray(4*1024*1024,'L'));
 cancel=false; const bool cancelledExport=writeProjectPackage(document(root+"/large.bin"),root+"/during.zip",cancel,[&](qint64 done,qint64,const QString&){if(done>0)cancel=true;},error);
 require(!cancelledExport && !QFileInfo::exists(root+"/during.zip"),"midstream export cancellation");
 cancel=false; const bool cancelledImport=extractProjectPackage(root+"/deflated.zip",root+"/during-import",cancel,[&](qint64 done,qint64,const QString&){if(done>0)cancel=true;},error);
 require(!cancelledImport && !QFileInfo::exists(root+"/during-import"),"midstream import cancellation");
 cancel=false; require(!readProjectFolder(root+"/deflated",cancel,[&](qint64 done,qint64,const QString&){if(done>0)cancel=true;},error).size(),"midstream validation cancellation"); cancel=false;
 auto emptyDoc=document(original); emptyDoc["sources"]=QJsonArray{QJsonObject{{"settings",QJsonObject{{"file",original},{"directory","empty"}}}}};
 require(QDir().mkpath(root+"/source-empty"),"create empty source directory");
 emptyDoc["directories"]=QJsonArray{QJsonObject{{"original_path",root+"/source-empty"},{"relative_path","empty"},{"pointers",QJsonArray{"/sources/0/settings/directory"}}}};
 const bool emptyWritten=writeProjectPackage(emptyDoc,root+"/empty-dir.zip",cancel,{},error); require(emptyWritten,"empty directory export: "+error);
 const bool emptyRead=extractProjectPackage(root+"/empty-dir.zip",root+"/empty-dir",cancel,{},error); require(emptyRead,"empty directory import: "+error);
 require(QFileInfo(root+"/empty-dir/empty").isDir(),"empty directory preserved");
 save(root+"/empty-file",{}); const bool zeroWritten=writeProjectPackage(document(root+"/empty-file"),root+"/zero.zip",cancel,{},error); require(zeroWritten,"zero file export: "+error);
 const bool zeroRead=extractProjectPackage(root+"/zero.zip",root+"/zero",cancel,{},error); require(zeroRead,"zero file import: "+error);
 QDir().mkpath(root+"/one"); QDir().mkpath(root+"/two"); save(root+"/one/shared.bin","first"); save(root+"/two/shared.bin","second");
 auto names=document(root+"/one/shared.bin","assets/a/shared.bin");
 names["sources"]=QJsonArray{QJsonObject{{"settings",QJsonObject{{"file",root+"/one/shared.bin"},{"a/b~c",root+"/two/shared.bin"}}}}};
 auto nameAssets=names["assets"].toArray(); nameAssets.append(QJsonObject{{"id","second"},{"original_path",root+"/two/shared.bin"},{"relative_path","assets/b/shared.bin"},{"pointers",QJsonArray{"/sources/0/settings/a~1b~0c"}}}); names["assets"]=nameAssets;
 const bool namesWritten=writeProjectPackage(names,root+"/names.zip",cancel,{},error); require(namesWritten,"same basename export: "+error);
 const bool namesRead=extractProjectPackage(root+"/names.zip",root+"/names",cancel,{},error); require(namesRead,"same basename import: "+error);
 const auto namesDoc=readProjectFolder(root+"/names",cancel,{},error); require(!namesDoc.isEmpty(),"same basename read: "+error);
 require(load(root+"/names/assets/a/shared.bin")=="first" && load(root+"/names/assets/b/shared.bin")=="second","same basename assets isolated");
 require(namesDoc["sources"].toArray()[0].toObject()["settings"].toObject()["a/b~c"].toString()==root+"/names/assets/b/shared.bin","RFC6901 escaped pointer");
 const QByteArray preserved=load(root+"/names.zip"); require(!writeProjectPackage(names,root+"/names.zip",cancel,{},error) && load(root+"/names.zip")==preserved,"existing archive preserved byte for byte");
 const bool reexport=writeProjectPackage(namesDoc,root+"/reexport.zip",cancel,{},error); require(reexport,"restored package reexport: "+error);
#ifdef Q_OS_WIN
 require(junction(root+"/junction",root+"/deflated"),"create real NTFS junction fixture");
 require(readProjectFolder(root+"/junction",cancel,{},error).isEmpty(),"real reparse project folder rejected");
 require(!extractProjectPackage(root+"/deflated.zip",root+"/junction/escape",cancel,{},error),"real reparse destination ancestor rejected");
 require(!writeProjectPackage(document(root+"/junction/assets/file.bin"),root+"/junction-source.zip",cancel,{},error),"real reparse source ancestor rejected");
 auto directoryLinkDoc=emptyDoc; auto directoryLinkRecord=directoryLinkDoc["directories"].toArray()[0].toObject(); directoryLinkRecord["original_path"]=root+"/junction"; directoryLinkDoc["directories"]=QJsonArray{directoryLinkRecord};
 require(!writeProjectPackage(directoryLinkDoc,root+"/junction-directory.zip",cancel,{},error),"real reparse directory record rejected");
 require(RemoveDirectoryW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(root+"/junction").utf16()))!=0,"remove only junction fixture");
 require(load(root+"/deflated/assets/file.bin")==payload,"junction target preserved");
#endif
 if (app.arguments().contains("--large")) {
  const qint64 largeSize=(qint64(1)<<32)+1048576; QFile large(root+"/over4gb.bin"); require(large.open(QIODevice::WriteOnly),"large source create"); require(large.resize(largeSize),"large source resize");
  require(large.seek(largeSize-4) && large.write("TAIL",4)==4,"large source marker"); large.close();
  const bool largeWritten=writeProjectPackage(document(large.fileName()),root+"/over4gb.zip",cancel,{},error); require(largeWritten,"over 4GB export: "+error);
  require(QFileInfo(root+"/over4gb.zip").size()>largeSize,"over 4GB archive size");
  const bool largeExtracted=extractProjectPackage(root+"/over4gb.zip",root+"/over4gb",cancel,{},error); require(largeExtracted,"over 4GB import: "+error);
  const auto largeRestored=readProjectFolder(root+"/over4gb",cancel,{},error); require(!largeRestored.isEmpty(),"over 4GB validate: "+error);
  QFile restoredLarge(root+"/over4gb/assets/素材.bin"); require(restoredLarge.size()==largeSize && restoredLarge.open(QIODevice::ReadOnly) && restoredLarge.seek(largeSize-4) && restoredLarge.read(4)=="TAIL","over 4GB content marker");
  std::puts("PASS actual 4,296,015,872-byte file ZIP64 export/import/SHA/marker");
 }
 require(QDir(root).entryList({".spatial-*"},QDir::AllEntries|QDir::Hidden|QDir::NoDotAndDotDot).isEmpty(),"no owned staging remains");
 std::puts("PASS project archive: ZIP64/deflate, Chinese paths, SHA/CRC, overwrite, traversal, duplicates, links, sizes, empty files/directories, midstream cancellation");
 return 0;
 } catch (const std::exception &e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
}


