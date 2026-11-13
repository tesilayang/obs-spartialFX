#pragma once
#include <QJsonObject>
#include <QPointer>
#include <QWidget>
#include <atomic>
#include <functional>
#include <memory>
class QLabel;
class QPushButton;
class QThread;

namespace spatial::projects {
// File operations run asynchronously. Scene capture and application stay on the
// widget's UI thread; callers can also use these methods with explicit paths.
class ProjectPanel final : public QWidget {
public:
    explicit ProjectPanel(QWidget *parent=nullptr);
    ~ProjectPanel() override;
    std::function<void()> changed;
    void exportJson(const QString &path);
    void exportPackage(const QString &path);
    void importJson(const QString &path);
    void importFolder(const QString &path);
    void importZip(const QString &zip,const QString &parentDirectory);
    void importDocument(QJsonObject document,bool nameMatch=true);
private:
    struct TaskResult { QJsonObject document; QString error,path; };
    using Progress = std::function<void(qint64,qint64,const QString&)>;
    using FileTask = std::function<void(TaskResult&,std::atomic_bool&,Progress)>;
    using Completion = std::function<void(const TaskResult&)>;
    std::shared_ptr<std::atomic_bool> cancellation;
    QLabel *status=nullptr;
    QPushButton *actions[4]{};
    bool busy=false;
    QPointer<QThread> activeThread;
    void runFileTask(const QString &title,FileTask task,Completion completed,std::function<void()> settled={});
    QJsonObject captureCurrent();
    void showPackageImport();
    void reviewPackage(QJsonObject document,const QString &path);
    void applyDocument(const QJsonObject &,bool nameMatch,const QString &expectedUuid=QString(),const QByteArray &expectedSnapshot=QByteArray());
    void report(const QString &,bool error=false);
};
}
