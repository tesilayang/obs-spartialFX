#include "project_panel.hpp"
#include "project_archive.hpp"
#include "project_obs.hpp"
#include "../hover_help.hpp"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointer>
#include <QProgressDialog>
#include <QPushButton>
#include <QSaveFile>
#include <QSet>
#include <QTableWidget>
#include <QThread>
#include <QUuid>
#include <QVBoxLayout>
#include <exception>

namespace spatial::projects {
namespace {
QString text(const char *value){return QString::fromUtf8(value);}
QString safeName(QString name){for(auto c:QString("<>:\"/\\|?*"))name.replace(c,'_');return name.trimmed().left(100);}
bool saveJson(const QJsonObject &document,const QString &path,std::atomic_bool &cancel,QString &error){
    if(cancel.load())return false;
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly)){error=file.errorString();return false;}
    const auto bytes=QJsonDocument(document).toJson(QJsonDocument::Indented);
    if(file.write(bytes)!=bytes.size()){error=file.errorString();return false;}
    if(cancel.load()){file.cancelWriting();return false;}
    if(!file.commit()){error=file.errorString();return false;}
    return true;
}
QString requirementsText(const QJsonObject &document){
    QStringList list;
    for(const auto &item:document.value("requirements").toArray()){
        if(item.isString())list<<item.toString();
        else if(item.isObject())list<<QString::fromUtf8(QJsonDocument(item.toObject()).toJson(QJsonDocument::Compact));
    }
    return list.join('\n');
}
QByteArray sceneRevision(const QJsonObject &document){
    // Capture timestamps and the stage transport clock are not user edits.
    // Keep all source settings except those five stage-only runtime fields.
    auto sources=document.value("sources").toArray();
    auto withoutClock=[](QJsonObject settings){for(const auto *key:{"timeline_position_us","timeline_position","timeline_preview","playing","play_token"})settings.remove(key);return settings;};
    for(int i=0;i<sources.size();++i){auto source=sources[i].toObject();if(source.value("id")=="obs_spatial_fx_stage"||source.value("versioned_id")=="obs_spatial_fx_stage"){source["settings"]=withoutClock(source.value("settings").toObject());auto settings=source.value("private_settings").toObject();if(settings.value("spatial_fx_model").isObject())settings["spatial_fx_model"]=withoutClock(settings.value("spatial_fx_model").toObject());source["private_settings"]=settings;sources[i]=source;}}
    return QJsonDocument(QJsonObject{{"scene_name",document.value("scene_name")},{"root_uuid",document.value("root_uuid")},{"sources",sources}}).toJson(QJsonDocument::Compact);
}
// The dialog edits a detached document. Cancel leaves the current OBS project intact.
bool resolveMissing(QWidget *parent,QJsonObject &document){
    auto missing=missingAssets(document);
    if(missing.isEmpty())return true;
    QDialog dialog(parent);dialog.setWindowTitle(text("重新链接缺失素材"));dialog.setObjectName("spatial.project-relink");dialog.resize(680,390);
    auto *layout=new QVBoxLayout(&dialog);
    auto *table=new QTableWidget;table->setObjectName("spatial.project-missing-assets");table->setColumnCount(2);table->setHorizontalHeaderLabels({text("原素材路径"),text("状态")});table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);table->setSelectionBehavior(QAbstractItemView::SelectRows);table->setSelectionMode(QAbstractItemView::SingleSelection);table->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(table);
    auto *message=new QLabel;message->setWordWrap(true);layout->addWidget(message);
    auto *relink=new QPushButton(text("重新链接选中素材…"));relink->setObjectName("spatial.project-relink-file");relink->setToolTip(text("选择替代文件后更新此素材的全部引用。继续导入会保留尚未找到的原路径，OBS 可能显示素材不可用。"));layout->addWidget(relink);
    QStringList ids;QList<bool> directoryRows;
    auto directoryEntries=document.value("directories").toArray();for(int i=0;i<directoryEntries.size();++i){auto entry=directoryEntries[i].toObject();if(entry.value("id").toString().isEmpty()){entry["id"]="directory-"+QUuid::createUuid().toString(QUuid::WithoutBraces);directoryEntries[i]=entry;}}document["directories"]=directoryEntries;
    auto refresh=[&]{
        ids.clear();directoryRows.clear();table->setRowCount(0);const auto unavailable=missingAssets(document);
        for(const auto *kind:{"assets","directories"})for(const auto &entry:document.value(kind).toArray()){
            const auto asset=entry.toObject();const auto path=asset.value("original_path").toString();if(!unavailable.contains(path))continue;const bool directory=QString::fromLatin1(kind)=="directories";
            int row=table->rowCount();table->insertRow(row);ids<<asset.value("id").toString();directoryRows<<directory;table->setItem(row,0,new QTableWidgetItem(path));table->setItem(row,1,new QTableWidgetItem(text(directory?"文件夹不可用":"文件不可用")));
        }
        message->setText(text("缺失 %1 项。可逐项重新链接，或保留工程信息继续导入。").arg(table->rowCount()));if(table->rowCount())table->selectRow(0);
    };
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Cancel);auto *apply=buttons->addButton(text("继续导入"),QDialogButtonBox::AcceptRole);apply->setObjectName("spatial.project-import-with-missing");layout->addWidget(buttons);
    QObject::connect(relink,&QPushButton::clicked,&dialog,[&]{int row=table->currentRow();if(row<0||row>=ids.size())return;const auto path=directoryRows[row]?QFileDialog::getExistingDirectory(&dialog,text("重新链接素材文件夹")):QFileDialog::getOpenFileName(&dialog,text("重新链接素材"));if(path.isEmpty())return;QString error;if(!relinkAsset(document,ids[row],path,error)){message->setText(error);return;}refresh();});
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    refresh();installHoverHelp(&dialog);return dialog.exec()==QDialog::Accepted;
}
}

ProjectPanel::ProjectPanel(QWidget *parent):QWidget(parent){
    setObjectName("spatial.project-panel");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(8,12,8,8);layout->setSpacing(8);
    const char *labels[]={"导出工程 JSON","导出完整工程包 ZIP","导入工程 JSON","导入完整工程包"};
    const char *names[]={"spatial.project-export-json","spatial.project-export-package","spatial.project-import-json","spatial.project-import-package"};
    const char *tips[]={"保存当前 OBS 场景及其依赖来源、舞台、动画、灯光、FX 与素材原路径。JSON 不复制素材。","将当前场景与依赖素材的原始副本打包为 ZIP，保留相对路径与校验值。硬件、网络及第三方插件仍需在目标电脑准备。","按存档中的原场景名称导入。存在同名场景时，确认并保存恢复 JSON 后覆盖；不存在时新建。缺失素材可手动重新链接。","始终新建场景。可直接读取已解压文件夹，或将 ZIP 解压到所选目录内的新文件夹；素材从持久目录读取。"};
    for(int i=0;i<4;++i){actions[i]=new QPushButton(text(labels[i]));actions[i]->setObjectName(names[i]);actions[i]->setToolTip(text(tips[i]));layout->addWidget(actions[i]);}
    status=new QLabel;status->setObjectName("spatial.project-status");status->setWordWrap(true);layout->addWidget(status);layout->addStretch();
    connect(actions[0],&QPushButton::clicked,this,[this]{auto path=QFileDialog::getSaveFileName(this,text("导出工程 JSON"),"project.json",text("工程 JSON (*.json)"));if(!path.isEmpty())exportJson(path);});
    connect(actions[1],&QPushButton::clicked,this,[this]{auto path=QFileDialog::getSaveFileName(this,text("导出完整工程包"),"project.zip",text("工程包 ZIP (*.zip)"));if(!path.isEmpty())exportPackage(path);});
    connect(actions[2],&QPushButton::clicked,this,[this]{auto path=QFileDialog::getOpenFileName(this,text("导入工程 JSON"),{},text("工程 JSON (*.json)"));if(!path.isEmpty())importJson(path);});
    connect(actions[3],&QPushButton::clicked,this,[this]{showPackageImport();});
}
ProjectPanel::~ProjectPanel(){
    if(cancellation)cancellation->store(true);
    // No worker waits for the UI. Joining here keeps plugin code and task data
    // alive during OBS shutdown, including when cancellation was requested.
    if(auto *thread=activeThread.data()){thread->wait();delete thread;activeThread.clear();}
}
void ProjectPanel::report(const QString &message,bool error){status->setText(message);status->setStyleSheet(error?"color:#ff8989;":"color:#a4c8ed;");}
void ProjectPanel::runFileTask(const QString &title,FileTask task,Completion completed,std::function<void()> settled){
    if(busy)return;busy=true;setProperty("operationInProgress",true);for(auto *button:actions)button->setEnabled(false);
    cancellation=std::make_shared<std::atomic_bool>(false);auto cancel=cancellation;auto result=std::make_shared<TaskResult>();
    auto *dialog=new QProgressDialog(title,text("取消"),0,1000,this);dialog->setObjectName("spatial.project-progress");dialog->setWindowModality(Qt::WindowModal);dialog->setMinimumDuration(0);dialog->setAutoClose(false);dialog->setAutoReset(false);dialog->setValue(0);
    connect(dialog,&QProgressDialog::canceled,this,[cancel]{cancel->store(true);});
    QPointer<QProgressDialog> guard(dialog);
    auto progress=[guard](qint64 done,qint64 total,const QString &label){QMetaObject::invokeMethod(qApp,[guard,done,total,label]{if(!guard)return;guard->setLabelText(label);guard->setRange(0,total>0?1000:0);if(total>0)guard->setValue(int(qMin<qint64>(1000,done*1000/total)));},Qt::QueuedConnection);};
    auto *thread=QThread::create([task=std::move(task),cancel,result,progress]{try{task(*result,*cancel,progress);}catch(const std::exception &e){result->error=QString::fromUtf8(e.what());}catch(...){result->error=text("文件操作失败。");}});
    connect(thread,&QThread::finished,thread,&QObject::deleteLater);
    connect(thread,&QThread::finished,this,[this,result,cancel,guard,completed=std::move(completed),settled=std::move(settled)]{activeThread=nullptr;if(guard){QObject::disconnect(guard.data(),nullptr,this,nullptr);guard->close();guard->deleteLater();}busy=false;setProperty("operationInProgress",false);cancellation.reset();for(auto *button:actions)button->setEnabled(true);if(settled)settled();if(cancel->load()){report(text("操作已取消。"));return;}if(!result->error.isEmpty()){report(result->error,true);return;}completed(*result);});
    activeThread=thread;thread->start();
}
QJsonObject ProjectPanel::captureCurrent(){
    auto *source=obs_frontend_get_current_scene();if(!source){report(text("没有可导出的当前场景。"),true);return {};}
    QString error;auto document=captureScene(source,error);obs_source_release(source);if(!error.isEmpty())report(error,true);return document;
}
void ProjectPanel::exportJson(const QString &path){
    if(busy)return;auto document=captureCurrent();if(document.isEmpty())return;
    runFileTask(text("正在导出工程 JSON…"),[document,path](TaskResult &result,std::atomic_bool &cancel,Progress progress)mutable{progress(0,0,text("正在分析素材依赖…"));discoverAssets(document,&cancel);saveJson(document,path,cancel,result.error);result.path=path;result.document=document;},[this](const TaskResult &result){const auto missing=missingAssets(result.document);report(text("工程 JSON 已保存：%1").arg(result.path)+(missing.isEmpty()?QString():text("\n缺失素材：\n%1").arg(missing.join('\n'))));});
}
void ProjectPanel::exportPackage(const QString &path){
    if(busy)return;auto document=captureCurrent();if(document.isEmpty())return;
    runFileTask(text("正在分析工程依赖…"),[document](TaskResult &result,std::atomic_bool &cancel,Progress)mutable{discoverAssets(document,&cancel);result.document=document;},[this,path](const TaskResult &result){
        reviewPackage(result.document,path);
    });
}
void ProjectPanel::reviewPackage(QJsonObject document,const QString &path){
    auto *dialog=new QDialog(this);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowModality(Qt::WindowModal);dialog->setWindowTitle(text("核对完整工程包素材"));dialog->setObjectName("spatial.project-export-review");dialog->resize(720,480);
    auto state=std::make_shared<QJsonObject>(std::move(document));auto *layout=new QVBoxLayout(dialog);auto *summary=new QLabel;summary->setWordWrap(true);summary->setObjectName("spatial.project-asset-summary");layout->addWidget(summary);
    auto *table=new QTableWidget;table->setObjectName("spatial.project-export-assets");table->setColumnCount(3);table->setHorizontalHeaderLabels({text("本地素材"),text("字节"),text("状态")});table->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Stretch);table->setEditTriggers(QAbstractItemView::NoEditTriggers);layout->addWidget(table);
    auto *requirements=new QLabel(requirementsText(*state));requirements->setObjectName("spatial.project-requirements");requirements->setWordWrap(true);layout->addWidget(requirements);
    auto *row=new QHBoxLayout;auto *addFile=new QPushButton(text("补充本地文件…"));addFile->setObjectName("spatial.project-add-file");auto *addFolder=new QPushButton(text("补充本地文件夹…"));addFolder->setObjectName("spatial.project-add-folder");row->addWidget(addFile);row->addWidget(addFolder);layout->addLayout(row);
    addFolder->setToolTip(text("补充未被自动识别的第三方插件或网页依赖文件，保留文件夹内部结构。自动检测无法识别脚本计算的路径；请在导入后核对这些插件或网页的自定义路径。"));addFile->setToolTip(addFolder->toolTip());
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Cancel);auto *exportButton=buttons->addButton(text("导出 ZIP"),QDialogButtonBox::AcceptRole);exportButton->setObjectName("spatial.project-confirm-export");layout->addWidget(buttons);
    auto refresh=[state,summary,table,exportButton]{qint64 total=0;const auto unavailable=missingAssets(*state);table->setRowCount(0);const auto assets=state->value("assets").toArray();for(const auto &entry:assets){const auto asset=entry.toObject();const auto file=asset.value("original_path").toString();const bool valid=!unavailable.contains(file);const auto size=asset.value("size").toVariant().toLongLong();if(size>0)total+=size;int index=table->rowCount();table->insertRow(index);table->setItem(index,0,new QTableWidgetItem(file));table->setItem(index,1,new QTableWidgetItem(QString::number(qMax<qint64>(0,size))));table->setItem(index,2,new QTableWidgetItem(text(valid?"可用":"不可用")));}for(const auto &value:state->value("directories").toArray()){const auto file=value.toObject().value("original_path").toString();if(!unavailable.contains(file))continue;int index=table->rowCount();table->insertRow(index);table->setItem(index,0,new QTableWidgetItem(file));table->setItem(index,1,new QTableWidgetItem("—"));table->setItem(index,2,new QTableWidgetItem(text("文件夹不可用")));}summary->setText(text("%1 个文件 · %2 MiB · 不可用 %3 项").arg(assets.size()).arg(double(total)/1048576.0,0,'f',2).arg(unavailable.size()));exportButton->setEnabled(unavailable.isEmpty());};
    QPointer<QDialog> guard(dialog);
    auto append=[this,state,guard,refresh,addFile,addFolder,exportButton](const QStringList &paths){if(paths.isEmpty()||busy)return;addFile->setEnabled(false);addFolder->setEnabled(false);exportButton->setEnabled(false);
        runFileTask(text("正在扫描补充素材…"),[document=*state,paths](TaskResult &out,std::atomic_bool &cancel,Progress progress)mutable{
            auto assets=document.value("assets").toArray();QSet<QString> existing;for(const auto &entry:assets)existing.insert(QFileInfo(entry.toObject().value("original_path").toString()).absoluteFilePath().toCaseFolded());
            auto appendFile=[&](const QString &file,const QString &relative,bool preservePlacement){QFileInfo info(file);if(!info.isFile()||!info.isReadable()){out.error=text("无法读取补充素材：%1").arg(file);return;}const auto canonical=info.absoluteFilePath().toCaseFolded();if(!preservePlacement&&existing.contains(canonical))return;existing.insert(canonical);const auto id="supplement-"+QUuid::createUuid().toString(QUuid::WithoutBraces);assets.append(QJsonObject{{"id",id},{"original_path",info.absoluteFilePath()},{"relative_path",relative},{"size",info.size()},{"sha256",""},{"pointers",QJsonArray{}}});};
            for(const auto &path:paths){if(cancel.load())return;const QFileInfo info(path);const auto group="assets/supplement-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"/";if(info.isDir()){QDir root(path);QDirIterator iterator(path,QDir::Files|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);while(iterator.hasNext()){if(cancel.load())return;const auto file=iterator.next();progress(0,0,file);appendFile(file,group+root.relativeFilePath(file),true);if(!out.error.isEmpty())return;}}else appendFile(path,group+info.fileName(),false);if(!out.error.isEmpty())return;}
            document["assets"]=assets;out.document=document;
        },[state,guard,refresh](const TaskResult &out){if(!guard)return;*state=out.document;refresh();},[guard,refresh,addFile,addFolder]{if(!guard)return;addFile->setEnabled(true);addFolder->setEnabled(true);refresh();});
    };
    connect(addFile,&QPushButton::clicked,dialog,[dialog,append]{append(QFileDialog::getOpenFileNames(dialog,text("补充本地素材")));});connect(addFolder,&QPushButton::clicked,dialog,[dialog,append]{const auto folder=QFileDialog::getExistingDirectory(dialog,text("补充本地素材文件夹"));if(!folder.isEmpty())append({folder});});
    connect(buttons,&QDialogButtonBox::rejected,dialog,&QDialog::reject);connect(dialog,&QDialog::rejected,this,[this]{if(busy&&cancellation)cancellation->store(true);});
    connect(buttons,&QDialogButtonBox::accepted,dialog,[this,dialog,state,path]{if(busy)return;auto document=*state;dialog->accept();runFileTask(text("正在导出完整工程包…"),[document,path](TaskResult &out,std::atomic_bool &cancel,Progress progress){if(!writeProjectPackage(document,path,cancel,progress,out.error)&&out.error.isEmpty()&&!cancel.load())out.error=text("完整工程包导出失败。");out.path=path;},[this](const TaskResult &out){report(text("工程包已保存：%1").arg(out.path));});});
    refresh();installHoverHelp(dialog);dialog->open();
}
void ProjectPanel::importJson(const QString &path){
    runFileTask(text("正在读取工程 JSON…"),[path](TaskResult &result,std::atomic_bool &cancel,Progress){if(cancel.load())return;QFile file(path);if(!file.open(QIODevice::ReadOnly)){result.error=file.errorString();return;}QJsonParseError parse;auto json=QJsonDocument::fromJson(file.readAll(),&parse);if(parse.error!=QJsonParseError::NoError||!json.isObject()){result.error=text("无效工程 JSON：%1").arg(parse.errorString());return;}result.document=json.object();if(!validateDocument(result.document,result.error))return;if(!result.document.value("assets").isArray())discoverAssets(result.document,&cancel);},[this](const TaskResult &result){importDocument(result.document,true);});
}
void ProjectPanel::importFolder(const QString &path){
    runFileTask(text("正在校验工程文件夹…"),[path](TaskResult &result,std::atomic_bool &cancel,Progress progress){result.document=readProjectFolder(path,cancel,progress,result.error);},[this](const TaskResult &result){importDocument(result.document,false);});
}
void ProjectPanel::importZip(const QString &zip,const QString &parentDirectory){
    if(busy)return;const auto parent=QFileInfo(parentDirectory);if(!parent.isDir()){report(text("请选择已存在的解压目标目录。"),true);return;}
    const auto destination=QDir(parentDirectory).filePath(safeName(QFileInfo(zip).completeBaseName())+"-"+QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    runFileTask(text("正在解压并校验工程包…"),[zip,destination](TaskResult &result,std::atomic_bool &cancel,Progress progress){if(!extractProjectPackage(zip,destination,cancel,progress,result.error))return;if(cancel.load())return;result.document=readProjectFolder(destination,cancel,progress,result.error);result.path=destination;},[this](const TaskResult &result){importDocument(result.document,false);});
}
void ProjectPanel::importDocument(QJsonObject document,bool nameMatch){
    if(busy)return;setProperty("lastImportSucceeded",false);QString error;if(!validateDocument(document,error)){report(error,true);return;}
    if(!nameMatch){const auto missing=missingAssets(document);if(!missing.isEmpty()){report(text("工程包素材不完整，请重新校验后导入：\n%1").arg(missing.join('\n')),true);return;}}
    if(!resolveMissing(this,document))return;
    const auto requirements=requirementsText(document);if(!requirements.isEmpty()&&QMessageBox::warning(this,text("工程运行要求"),requirements+text("\n\n确认目标环境满足这些要求后继续。"),QMessageBox::Ok|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Ok)return;
    const auto uuid=nameMatch?matchingSceneUuid(document):QString();
    if(uuid.isEmpty()){applyDocument(document,nameMatch);return;}
    const auto name=document.value("scene_name").toString();
    if(QMessageBox::question(this,text("覆盖同名场景"),text("存档的原场景“%1”已存在。覆盖前将保存可导入的恢复 JSON。\n是否覆盖此场景？").arg(name),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    auto *source=obs_get_source_by_uuid(uuid.toUtf8().constData());if(!source){report(text("目标场景已不存在，请重新导入。"),true);return;}
    auto recovery=captureScene(source,error);obs_source_release(source);if(!error.isEmpty()||recovery.isEmpty()){report(text("无法创建恢复快照：%1").arg(error),true);return;}
    char *raw=obs_module_config_path("project-recovery");const auto directory=QString::fromUtf8(raw?raw:"");bfree(raw);if(directory.isEmpty()){report(text("无法定位 OBS 插件恢复目录。"),true);return;}
    const auto path=QDir(directory).filePath(safeName(name)+"-"+QDateTime::currentDateTime().toString("yyyyMMdd-hhmmsszzz")+"-"+QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)+".json");
    const auto snapshot=sceneRevision(recovery);
    runFileTask(text("正在保存覆盖前恢复快照…"),[recovery,directory,path](TaskResult &result,std::atomic_bool &cancel,Progress)mutable{if(!QDir().mkpath(directory)){result.error=text("无法创建恢复目录：%1").arg(directory);return;}discoverAssets(recovery,&cancel);saveJson(recovery,path,cancel,result.error);result.path=path;},[this,document,uuid,snapshot](const TaskResult &result){applyDocument(document,true,uuid,snapshot);if(property("lastImportSucceeded").toBool())report(status->text()+text("\n恢复 JSON：%1").arg(result.path));});
}
void ProjectPanel::applyDocument(const QJsonObject &document,bool nameMatch,const QString &expectedUuid,const QByteArray &expectedSnapshot){
    setProperty("lastImportSucceeded",false);if(!expectedUuid.isEmpty()&&matchingSceneUuid(document)!=expectedUuid){report(text("目标场景在保存快照期间发生变化，请重新导入。"),true);return;}
    if(!expectedSnapshot.isEmpty()){auto *source=obs_get_source_by_uuid(expectedUuid.toUtf8().constData());QString error;const auto current=source?captureScene(source,error):QJsonObject();if(source)obs_source_release(source);if(!error.isEmpty()||sceneRevision(current)!=expectedSnapshot){report(text("目标场景在保存快照期间已修改，请重新导入。恢复快照已保留。"),true);return;}}
    const auto result=restoreScene(document,nameMatch);setProperty("lastImportSucceeded",result.success);setProperty("lastImportedSceneUuid",result.sceneUuid);
    if(!result.success){report(result.error,true);return;}obs_frontend_save();if(changed)changed();
    report(text("工程已导入场景：%1").arg(result.sceneName)+(result.warnings.isEmpty()?QString():"\n"+result.warnings.join('\n')));
}
void ProjectPanel::showPackageImport(){
    QDialog dialog(this);dialog.setWindowTitle(text("导入完整工程包"));dialog.setObjectName("spatial.project-package-dialog");dialog.resize(620,300);auto *layout=new QVBoxLayout(&dialog);
    auto pathRow=[&](QFormLayout *form,const char *label,const char *name,bool file){auto *row=new QHBoxLayout;auto *field=new QLineEdit;field->setObjectName(name);auto *browse=new QPushButton(text(file?"选择 ZIP":"选择文件夹"));browse->setObjectName(QString::fromUtf8(name)+"-browse");row->addWidget(field,1);row->addWidget(browse);form->addRow(text(label),row);connect(browse,&QPushButton::clicked,&dialog,[&,field,file]{const auto path=file?QFileDialog::getOpenFileName(&dialog,text("选择工程 ZIP"),field->text(),text("工程包 ZIP (*.zip)")):QFileDialog::getExistingDirectory(&dialog,text("选择文件夹"),field->text());if(!path.isEmpty())field->setText(path);});return field;};
    auto *folderGroup=new QGroupBox(text("已解压的工程文件夹"));auto *folderForm=new QFormLayout(folderGroup);auto *folder=pathRow(folderForm,"文件夹路径","spatial.project-folder",false);folder->setToolTip(text("选择含 project.json 和 assets 文件夹的工程根目录。校验后直接使用该目录中的素材，导入后请保留此文件夹。"));auto *importFolderButton=new QPushButton(text("导入文件夹"));importFolderButton->setObjectName("spatial.project-import-folder");folderForm->addRow(importFolderButton);layout->addWidget(folderGroup);
    auto *zipGroup=new QGroupBox(text("ZIP 工程包"));auto *zipForm=new QFormLayout(zipGroup);auto *zip=pathRow(zipForm,"ZIP 位置","spatial.project-zip",true);auto *destination=pathRow(zipForm,"解压目标目录","spatial.project-destination",false);destination->setToolTip(text("将在所选目录中创建唯一的新文件夹，解包并验证全部素材后导入。此文件夹是工程的持久素材目录，请保留。"));auto *importZipButton=new QPushButton(text("导入 ZIP"));importZipButton->setObjectName("spatial.project-import-zip");zipForm->addRow(importZipButton);layout->addWidget(zipGroup);
    auto *error=new QLabel;error->setWordWrap(true);error->setStyleSheet("color:#ff8989;");layout->addWidget(error);auto *buttons=new QDialogButtonBox(QDialogButtonBox::Cancel);layout->addWidget(buttons);connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    connect(importFolderButton,&QPushButton::clicked,&dialog,[&]{if(!QFileInfo(folder->text()).isDir()){error->setText(text("请选择有效的工程文件夹。"));return;}const auto path=folder->text();dialog.accept();importFolder(path);});
    connect(importZipButton,&QPushButton::clicked,&dialog,[&]{if(!QFileInfo(zip->text()).isFile()||!QFileInfo(destination->text()).isDir()){error->setText(text("请选择有效的 ZIP 文件和已存在的解压目标目录。"));return;}const auto zipPath=zip->text(),target=destination->text();dialog.accept();importZip(zipPath,target);});
    installHoverHelp(&dialog);dialog.exec();
}
}

