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
 std::function<void()> changed;
 void refresh(bool force=false);
 void setDocument(const QString&);
private:
 spatial::StageModel*model;QVBoxLayout*column;QWidget*cards=nullptr;QString signature,document;
 QPointer<QColorDialog> colorDialog;
 uint64_t colorDocument=0;
 spatial::Layer*layer(const std::string&);
 void rebuild();void addObject();
};
