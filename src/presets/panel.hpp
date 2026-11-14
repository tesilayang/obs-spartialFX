#pragma once
#include <QWidget>
#include <QString>
#include <functional>
#include <memory>
#include <cstdint>
#include <utility>

namespace spatial { struct StageModel; }
namespace spatial::presets {
class PresetPanel final : public QWidget {
public:
 explicit PresetPanel(StageModel*,QWidget*parent=nullptr);
 ~PresetPanel() override;
 std::function<void(const StageModel&)> applyDocument,previewDocument;
 std::function<void()> cancelPreview;
 std::function<void(QString,bool)> message;
 // Hosts may supply their video rate; otherwise the panel queries OBS.
 std::function<std::pair<uint32_t,uint32_t>()> videoFrameRate;
 void setDocument(QString);
 void refresh();
 void cancelCurrentPreview();
private:
 struct Impl;
 std::unique_ptr<Impl> d;
};
}
