#include "dock.hpp"
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>
SpatialDock::SpatialDock(QWidget *parent) : QWidget(parent) {
    setObjectName("spatial.dock");
    setMinimumWidth(340);
    auto *layout = new QVBoxLayout(this);
    auto *title = new QLabel(QString::fromUtf8("OBS Spatial FX · 空间合成"), this);
    layout->addWidget(title);
    auto *tabs = new QTabWidget(this);
    tabs->setObjectName("spatial.tabs");
    for (const char *name : {"空间图层", "摄像机", "灯光", "FX 特效", "预设"}) {
        auto *page = new QWidget(tabs);
        auto *column = new QVBoxLayout(page);
        column->addWidget(new QLabel(QString::fromUtf8("原生 Dock 已加载"), page));
        column->addStretch();
        tabs->addTab(page, QString::fromUtf8(name));
    }
    layout->addWidget(tabs);
}

