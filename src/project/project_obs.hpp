#pragma once
#include <QJsonObject>
#include <QStringList>
#include <atomic>
#include <obs.h>
namespace spatial::projects {
struct ImportResult { bool success=false; QString sceneName,sceneUuid,error; QStringList warnings; };
// UI-thread only. Sources are detached JSON; file IO belongs to background tasks.
QJsonObject captureScene(obs_source_t *sceneSource, QString &error);
// nameMatch=true: replace the identically named scene after caller confirmation.
// false: always create a uniquely named scene. Never mutate shared child sources.
ImportResult restoreScene(const QJsonObject &document,bool nameMatch);
bool validateDocument(const QJsonObject &,QString &error);
void discoverAssets(QJsonObject &document,std::atomic_bool *cancellation=nullptr);
QStringList missingAssets(const QJsonObject &document);
bool relinkAsset(QJsonObject &document,const QString &assetId,const QString &path,QString &error);
QString matchingSceneUuid(const QJsonObject &document);
}
