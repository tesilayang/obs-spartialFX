// Include after v35_qa.inc. Required driver headers before its anonymous namespace:
// <QFileDialog>, <QDialogButtonBox>, <QPointer>, <QFileInfo>, <QEvent>.
// Drives only public Qt widgets, dynamic result properties, and libobs queries.
namespace v35uiqa {
class WorkerObserver final : public QObject {
public:
    int starts = 0, settles = 0;
    explicit WorkerObserver(QObject *parent) : QObject(parent) {}
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (event->type() == QEvent::DynamicPropertyChange &&
            static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName() == "operationInProgress") {
            if (watched->property("operationInProgress").toBool()) ++starts; else ++settles;
        }
        return false;
    }
};
enum class Phase {
    SaveJsonDialog, JsonExport, SaveZipDialog, ExportReview, ZipExport,
    ImportJsonDialog, SameNameImport, ZipDialog, ZipImport, FolderDialog,
    FolderImport, NewJsonDialog, NewJsonImport, RelinkJsonDialog,
    RelinkDialog, RelinkFileDialog, RelinkContinue, RelinkImport, Finished
};
struct State {
    QPointer<QWidget> manager;
    QPointer<QWidget> handledModal;
    WorkerObserver *observer = nullptr;
    QTimer *timer = nullptr;
    QElapsedTimer elapsed;
    Phase phase = Phase::SaveJsonDialog;
    QString token, json, zip, destination, extracted, originalUuid, previousUuid;
    QString originalName, newJson, newName, relinkJson, relinkName, replacement;
    QJsonObject document;
    int baselineStarts = 0;
    bool overwriteSeen = false, aborted = false, done = false, relinkAvailable = false;
};
bool queueClick(QWidget *parent, const char *name) {
    auto *button = parent ? parent->findChild<QPushButton *>(QString::fromLatin1(name)) : nullptr;
    const bool ready = button && button->isEnabled();
    check(ready, QString("v35 UI button ready: %1").arg(name));
    if (ready) {
        QPointer<QPushButton> guard(button);
        QTimer::singleShot(0, qApp, [guard] { if (guard) guard->click(); });
    }
    return ready;
}
bool screenshot(QWidget *widget, const QString &name) {
    const bool ok = widget && widget->grab().save(output + "/" + name + ".png");
    check(ok, "v35 UI screenshot: " + name); return ok;
}
void chooseFile(QFileDialog *dialog, const QString &path) {
    dialog->setOption(QFileDialog::DontUseNativeDialog, true);
    const QString absolute = QFileInfo(path).absoluteFilePath();
    dialog->setDirectory(QFileInfo(absolute).absolutePath());
    QPointer<QFileDialog> guard(dialog);
    // Static QFileDialog helpers restore their initial filename while their
    // directory model is loading. Wait for that update, then enter the entire
    // path in the same filename field used by a person.
    QTimer::singleShot(180, dialog, [guard, absolute] {
        if (!guard) return;
        auto *edit = guard->findChild<QLineEdit *>("fileNameEdit");
        check(edit != nullptr, "v35 UI nonnative file chooser filename field exists");
        if (!edit) { guard->reject(); return; }
        guard->selectFile(absolute); edit->setText(QDir::toNativeSeparators(absolute)); edit->setFocus();
        QTimer::singleShot(80, guard, [guard, absolute] {
            if (!guard) return;
            auto *edit = guard->findChild<QLineEdit *>("fileNameEdit");
            if (edit) edit->setText(QDir::toNativeSeparators(absolute));
            const auto selected = guard->selectedFiles();
            const auto normalized = [](QString value) { return QDir::cleanPath(QDir::fromNativeSeparators(value)); };
            const bool correct = selected.size() == 1 && normalized(selected[0]).compare(normalized(absolute), Qt::CaseInsensitive) == 0;
            blog(LOG_INFO, "[SpatialFX QA] file chooser expected=%s field=%s selected=%s", absolute.toUtf8().constData(), edit ? edit->text().toUtf8().constData() : "<missing>", selected.join(" | ").toUtf8().constData());
            check(correct, "v35 UI file chooser selected exact requested path: " + absolute);
            screenshot(guard, "ui-file-dialog-" + QFileInfo(absolute).completeBaseName());
            if (!correct) { guard->reject(); return; }
            QObject::connect(guard, &QDialog::accepted, qApp, [guard, absolute] {
                if (!guard) return;
                blog(LOG_INFO, "[SpatialFX QA] file chooser accepted expected=%s selected=%s", absolute.toUtf8().constData(), guard->selectedFiles().join(" | ").toUtf8().constData());
            });
            QMetaObject::invokeMethod(guard, "accept", Qt::QueuedConnection);
        });
    });
}
QJsonValue setPointer(QJsonValue value, const QStringList &parts, int at, const QString &replacement) {
    if (at == parts.size()) return replacement;
    QString key = parts[at]; key.replace("~1", "/").replace("~0", "~");
    if (value.isArray()) { auto array = value.toArray(); bool ok = false; int index = key.toInt(&ok); if (ok && index >= 0 && index < array.size()) array[index] = setPointer(array[index], parts, at + 1, replacement); return array; }
    auto object = value.toObject(); if (object.contains(key)) object[key] = setPointer(object[key], parts, at + 1, replacement); return object;
}
void complete(const std::shared_ptr<State> &state) {
    if (state->done) return;
    state->done = true; state->timer->stop(); state->timer->deleteLater();
    const int starts = state->manager && state->observer ? state->observer->starts : 0;
    const int settles = state->manager && state->observer ? state->observer->settles : 0;
    check(state->manager && !state->manager->property("operationInProgress").toBool(), "v35 UI final worker settled");
    check(starts > 0 && starts == settles, "v35 UI observed asynchronous operation start and settlement");
    screenshot(state->manager, "ui-project-management-final"); screenshot(mainWidget(), "ui-obs-final");
    QJsonObject evidence{{"elapsed_ms", state->elapsed.elapsed()}, {"worker_starts", starts},
        {"worker_settles", settles}, {"json", state->json}, {"zip", state->zip},
        {"persistent_folder", state->extracted}, {"same_name_confirmation", state->overwriteSeen},
        {"relink_exercised", state->relinkAvailable}, {"aborted", state->aborted},
        {"lastImportSucceeded", state->manager ? state->manager->property("lastImportSucceeded").toBool() : false},
        {"lastImportedSceneUuid", state->manager ? state->manager->property("lastImportedSceneUuid").toString() : QString()}};
    v35::save(output + "/ui-workflows.json", evidence); obs_frontend_save();
    QTimer::singleShot(150, qApp, finish);
}
void startImport(const std::shared_ptr<State> &state, Phase phase) {
    state->baselineStarts = state->observer->starts; state->phase = phase;
    if (!queueClick(state->manager, "spatial.project-import-json")) state->aborted = true;
}
bool settled(const std::shared_ptr<State> &state) {
    return state->observer->starts > state->baselineStarts &&
        state->observer->starts == state->observer->settles &&
        !state->manager->property("operationInProgress").toBool() && !QApplication::activeModalWidget();
}
QString importedUuid(const std::shared_ptr<State> &state) { return state->manager->property("lastImportedSceneUuid").toString(); }
bool successfulImport(const std::shared_ptr<State> &state, const QString &label) {
    const QString uuid = importedUuid(state);
    auto *source = uuid.isEmpty() ? nullptr : obs_get_source_by_uuid(uuid.toUtf8().constData());
    const bool good = state->manager->property("lastImportSucceeded").toBool() && source;
    check(good, label + " result properties reference a real OBS scene");
    if (source) { check(obs_scene_from_source(source) != nullptr, label + " imported object is a scene"); obs_source_release(source); }
    if (!good) state->aborted = true;
    return good;
}
void poll(const std::shared_ptr<State> &state) {
    if (state->done) return;
    if (!state->manager) { check(false, "v35 UI project panel survived workflow"); state->aborted = true; complete(state); return; }
    if (!state->aborted && state->elapsed.elapsed() > 44000) { check(false, "v35 UI workflows completed before 44-second deadline"); state->aborted = true; }
    QWidget *modal = QApplication::activeModalWidget();
    if (state->aborted) {
        if (modal && modal != state->handledModal) {
            state->handledModal = modal;
            QMetaObject::invokeMethod(modal, modal->objectName() == "spatial.project-progress" ? "cancel" : "reject", Qt::QueuedConnection);
        }
        if (!state->manager->property("operationInProgress").toBool() && !modal) complete(state);
        else if (state->elapsed.elapsed() > 47500) complete(state);
        return;
    }
    if (auto *message = qobject_cast<QMessageBox *>(modal)) {
        if (state->handledModal == message) return;
        state->handledModal = message;
        if (message->windowTitle().contains(QString::fromUtf8("工程运行要求"))) {
            screenshot(message, "ui-runtime-requirements");
            if (auto *button = message->button(QMessageBox::Ok)) QTimer::singleShot(0, button, [button] { button->click(); });
            else state->aborted = true;
        } else if (message->windowTitle().contains(QString::fromUtf8("覆盖同名场景")) && state->phase == Phase::SameNameImport) {
            state->overwriteSeen = true; screenshot(message, "ui-same-name-confirmation");
            if (auto *button = message->button(QMessageBox::Yes)) QTimer::singleShot(0, button, [button] { button->click(); });
            else state->aborted = true;
        } else { check(false, "v35 UI unexpected message: " + message->windowTitle() + " " + message->text()); state->aborted = true; state->handledModal.clear(); }
        return;
    }
    if (auto *chooser = qobject_cast<QFileDialog *>(modal)) {
        if (state->handledModal == chooser) return;
        state->handledModal = chooser;
        switch (state->phase) {
        case Phase::SaveJsonDialog: state->phase = Phase::JsonExport; chooseFile(chooser, state->json); return;
        case Phase::SaveZipDialog: state->phase = Phase::ExportReview; chooseFile(chooser, state->zip); return;
        case Phase::ImportJsonDialog: state->phase = Phase::SameNameImport; chooseFile(chooser, state->json); return;
        case Phase::NewJsonDialog: state->phase = Phase::NewJsonImport; chooseFile(chooser, state->newJson); return;
        case Phase::RelinkJsonDialog: state->phase = Phase::RelinkDialog; chooseFile(chooser, state->relinkJson); return;
        case Phase::RelinkFileDialog: state->phase = Phase::RelinkContinue; chooseFile(chooser, state->replacement); return;
        default: check(false, "v35 UI unexpected file chooser phase"); state->aborted = true; state->handledModal.clear(); return;
        }
    }
    switch (state->phase) {
    case Phase::JsonExport:
        if (!settled(state) || !QFileInfo::exists(state->json)) break;
        { QFile file(state->json); const bool opened = file.open(QIODevice::ReadOnly); state->document = opened ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
          check(!state->document.isEmpty() && state->document["root_uuid"] == state->originalUuid, "v35 UI JSON export captures current scene");
          if (state->document.isEmpty()) { state->aborted = true; break; } }
        screenshot(state->manager, "ui-json-export-complete"); state->baselineStarts = state->observer->starts; state->phase = Phase::SaveZipDialog;
        if (!queueClick(state->manager, "spatial.project-export-package")) state->aborted = true;
        break;
    case Phase::ExportReview:
        if (modal && modal->objectName() == "spatial.project-export-review") {
            screenshot(modal, "ui-package-export-review");
            check(modal->findChild<QTableWidget *>("spatial.project-export-assets") != nullptr, "v35 UI export review lists assets");
            state->baselineStarts = state->observer->starts; state->phase = Phase::ZipExport;
            if (!queueClick(modal, "spatial.project-confirm-export")) state->aborted = true;
        }
        break;
    case Phase::ZipExport:
        if (settled(state) && QFileInfo(state->zip).size() > 0) {
            check(true, "v35 UI ZIP export completed through review confirmation"); screenshot(state->manager, "ui-zip-export-complete"); startImport(state, Phase::ImportJsonDialog);
        }
        break;
    case Phase::SameNameImport:
        if (!settled(state)) break;
        if (!successfulImport(state, "v35 UI same-name JSON")) break;
        check(state->overwriteSeen && importedUuid(state) == state->originalUuid, "v35 UI confirmed JSON overwrite retains target identity");
        { auto *status = state->manager->findChild<QLabel *>("spatial.project-status"); const QString marker = QString::fromUtf8("恢复 JSON：");
          const QString recovery = status ? status->text().section(marker, -1).trimmed() : QString(); QFile file(recovery);
          const bool opened = status && status->text().contains(marker) && file.open(QIODevice::ReadOnly);
          const auto recovered = opened ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
          check(opened && recovered["format"] == "obs-spatial-fx-project" && recovered["root_uuid"] == state->originalUuid, "v35 UI overwrite saved a reusable recovery JSON"); }
        screenshot(state->manager, "ui-json-overwrite-complete"); state->previousUuid = importedUuid(state); state->baselineStarts = state->observer->starts; state->phase = Phase::ZipDialog;
        if (!queueClick(state->manager, "spatial.project-import-package")) state->aborted = true;
        break;
    case Phase::ZipDialog:
        if (modal && modal->objectName() == "spatial.project-package-dialog") {
            auto *zip = modal->findChild<QLineEdit *>("spatial.project-zip"); auto *destination = modal->findChild<QLineEdit *>("spatial.project-destination");
            check(zip && destination, "v35 UI ZIP import location fields exist");
            if (!zip || !destination) { state->aborted = true; break; }
            zip->setText(state->zip); destination->setText(state->destination); screenshot(modal, "ui-zip-import-locations"); state->phase = Phase::ZipImport;
            if (!queueClick(modal, "spatial.project-import-zip")) state->aborted = true;
        }
        break;
    case Phase::ZipImport:
        if (!settled(state)) break;
        if (!successfulImport(state, "v35 UI ZIP package")) break;
        check(importedUuid(state) != state->previousUuid, "v35 UI ZIP import always creates new scene identity");
        { const auto folders = QDir(state->destination).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
          check(folders.size() == 1, "v35 UI ZIP creates one persistent extracted folder");
          if (folders.size() != 1) { state->aborted = true; break; }
          state->extracted = QDir(state->destination).filePath(folders[0]); check(QFileInfo::exists(state->extracted + "/project.json"), "v35 UI extracted directory contains project.json"); }
        screenshot(state->manager, "ui-zip-import-complete"); state->previousUuid = importedUuid(state); state->baselineStarts = state->observer->starts; state->phase = Phase::FolderDialog;
        if (!queueClick(state->manager, "spatial.project-import-package")) state->aborted = true;
        break;
    case Phase::FolderDialog:
        if (modal && modal->objectName() == "spatial.project-package-dialog") {
            auto *folder = modal->findChild<QLineEdit *>("spatial.project-folder"); check(folder != nullptr, "v35 UI extracted folder location field exists");
            if (!folder) { state->aborted = true; break; }
            folder->setText(state->extracted); screenshot(modal, "ui-folder-import-location"); state->phase = Phase::FolderImport;
            if (!queueClick(modal, "spatial.project-import-folder")) state->aborted = true;
        }
        break;
    case Phase::FolderImport:
        if (!settled(state)) break;
        if (!successfulImport(state, "v35 UI extracted folder")) break;
        check(importedUuid(state) != state->previousUuid && importedUuid(state) != state->originalUuid, "v35 UI folder import creates another independent scene");
        screenshot(state->manager, "ui-folder-import-complete"); state->previousUuid = importedUuid(state);
        { auto document = state->document; document["scene_name"] = state->newName; v35::save(state->newJson, document); }
        startImport(state, Phase::NewJsonDialog); break;
    case Phase::NewJsonImport:
        if (!settled(state)) break;
        if (!successfulImport(state, "v35 UI new-name JSON")) break;
        { auto *source = obs_get_source_by_uuid(importedUuid(state).toUtf8().constData());
          check(source && QString::fromUtf8(obs_source_get_name(source)) == state->newName && importedUuid(state) != state->previousUuid, "v35 UI JSON creates scene using saved absent name"); obs_source_release(source); }
        screenshot(state->manager, "ui-json-new-name-complete"); state->previousUuid = importedUuid(state);
        { auto document = state->document; auto assets = document["assets"].toArray();
          for (int i = 0; i < assets.size(); ++i) { auto asset = assets[i].toObject(); const auto pointers = asset["pointers"].toArray(); const QString original = asset["original_path"].toString();
              if (pointers.isEmpty() || !QFileInfo(original).isFile()) continue;
              state->replacement = original; state->relinkAvailable = true; const QString missing = output + "/ui-intentionally-missing-" + state->token + ".bin";
              asset["original_path"] = missing; assets[i] = asset;
              for (const auto &pointer : pointers) document = setPointer(document, pointer.toString().mid(1).split('/'), 0, missing).toObject();
              break;
          }
          if (state->relinkAvailable) { document["assets"] = assets; document["scene_name"] = state->relinkName; v35::save(state->relinkJson, document); startImport(state, Phase::RelinkJsonDialog); }
          else { blog(LOG_INFO, "[SpatialFX QA] NOT RUN optional relink workflow: current scene has no available file asset with JSON pointers"); state->phase = Phase::Finished; }
        }
        break;
    case Phase::RelinkDialog:
        if (modal && modal->objectName() == "spatial.project-relink") {
            auto *table = modal->findChild<QTableWidget *>("spatial.project-missing-assets"); check(table && table->rowCount() > 0, "v35 UI missing material is listed before relink");
            if (!table || !table->rowCount()) { state->aborted = true; break; }
            table->selectRow(0); screenshot(modal, "ui-missing-material-relink"); state->phase = Phase::RelinkFileDialog;
            if (!queueClick(modal, "spatial.project-relink-file")) state->aborted = true;
        }
        break;
    case Phase::RelinkContinue:
        if (modal && modal->objectName() == "spatial.project-relink") {
            auto *table = modal->findChild<QTableWidget *>("spatial.project-missing-assets");
            check(table && table->rowCount() == 0, "v35 UI selected replacement resolves missing material row");
            if (!table || table->rowCount()) { state->aborted = true; break; }
            screenshot(modal, "ui-material-relinked"); state->phase = Phase::RelinkImport;
            if (!queueClick(modal, "spatial.project-import-with-missing")) state->aborted = true;
        }
        break;
    case Phase::RelinkImport:
        if (!settled(state)) break;
        if (!successfulImport(state, "v35 UI manually relinked JSON")) break;
        { auto *source = obs_get_source_by_uuid(importedUuid(state).toUtf8().constData()); QString error;
          auto document = source ? spatial::projects::captureScene(source, error) : QJsonObject(); obs_source_release(source); spatial::projects::discoverAssets(document);
          bool found = false; for (const auto &asset : document["assets"].toArray()) if (asset.toObject()["original_path"] == state->replacement) found = true;
          check(error.isEmpty() && found && importedUuid(state) != state->previousUuid, "v35 UI manually selected replacement reaches restored OBS source"); }
        screenshot(state->manager, "ui-relink-import-complete"); state->phase = Phase::Finished; break;
    case Phase::Finished: complete(state); break;
    default: break;
    }
}
}
void v35ui() {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs, true);
    auto *dock = panel(); auto *manager = dock ? dock->findChild<QWidget *>("spatial.project-panel") : nullptr;
    check(manager != nullptr, "v35 UI project manager available");
    if (!manager) { QTimer::singleShot(0, qApp, finish); return; }
    if (auto *tabs = dock->findChild<QTabWidget *>("spatial.tabs")) for (int i = 0; i < tabs->count(); ++i) if (tabs->tabText(i) == QString::fromUtf8("工程管理")) tabs->setCurrentIndex(i);
    auto state = std::make_shared<v35uiqa::State>(); state->manager = manager;
    state->token = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    state->json = output + "/ui-project-" + state->token + ".json"; state->zip = output + "/ui-project-" + state->token + ".zip";
    state->destination = output + "/ui-persistent-" + state->token; check(QDir().mkpath(state->destination), "v35 UI persistent extraction parent prepared");
    state->newJson = output + "/ui-new-name-" + state->token + ".json"; state->newName = "V35 UI new scene " + state->token;
    state->relinkJson = output + "/ui-relink-" + state->token + ".json"; state->relinkName = "V35 UI relink scene " + state->token;
    auto *current = obs_frontend_get_current_scene(); state->originalUuid = current ? QString::fromUtf8(obs_source_get_uuid(current)) : QString(); state->originalName = current ? QString::fromUtf8(obs_source_get_name(current)) : QString(); obs_source_release(current);
    check(!state->originalUuid.isEmpty(), "v35 UI current scene exists for export");
    state->observer = new v35uiqa::WorkerObserver(manager); manager->installEventFilter(state->observer);
    state->timer = new QTimer(qApp); state->timer->setInterval(35); state->elapsed.start();
    QObject::connect(state->timer, &QTimer::timeout, qApp, [state] { v35uiqa::poll(state); }); state->timer->start();
    v35uiqa::screenshot(manager, "ui-project-management-start");
    if (!v35uiqa::queueClick(manager, "spatial.project-export-json")) state->aborted = true;
}
