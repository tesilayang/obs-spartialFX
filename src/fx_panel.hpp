#pragma once
#include "model.hpp"
#include <QWidget>
#include <QPointer>
#include <functional>
class QVBoxLayout;
class QColorDialog;
class FxPanel final : public QWidget {
public:
 explicit FxPanel(spatial::StageModel*,QWidget*parent=nullptr);
 ~FxPanel() override;
 std::function<void()> changed;
 std::function<bool(const std::string&,const std::string&,const std::string&,double)> editParameter;
 std::function<void(const spatial::StageModel&)> previewDocument,applyDocument;
 std::function<void(const spatial::StageModel&)> cancelPreview;
 void syncEvaluatedValues();
 void cancelColorEdit();
 void refresh(bool force=false);
 void setDocument(const QString&);
private:
 spatial::StageModel*model;QVBoxLayout*column;QWidget*cards=nullptr;QString signature,document;
 QPointer<QColorDialog> colorDialog;
 uint64_t colorDocument=0;
 std::vector<std::function<void()>> valueRefreshers;
 spatial::Layer*layer(const std::string&);
 void rebuild();void addObject();
};
