#pragma once
#include <QJsonObject>
#include <QString>
#include <atomic>
#include <functional>

namespace spatial::projects {
// Invoked on the calling worker thread; returning code must not touch UI objects.
using Progress = std::function<void(qint64 completed, qint64 total, const QString &detail)>;

// Files are streamed. Destinations must not exist; success publishes atomically.
bool writeProjectPackage(QJsonObject document, const QString &zipPath,
                         std::atomic_bool &cancel, const Progress &progress, QString &error);
QJsonObject readProjectFolder(const QString &folder, std::atomic_bool &cancel,
                              const Progress &progress, QString &error);
bool extractProjectPackage(const QString &zipPath, const QString &destination,
                           std::atomic_bool &cancel, const Progress &progress, QString &error);
}
