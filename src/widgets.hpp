#pragma once
#include "model.hpp"
#include <QWidget>
#include <QDoubleSpinBox>
#include <QStringList>
#include <QTabWidget>
#include <functional>
class QSlider;
class QDialog;class QVBoxLayout;class QPushButton;
class QLabel;class QColor;
void setColorValuePreview(QLabel*,const QColor&);
// Only the active page determines height; the Dock owns vertical scrolling.
class ContentTabs final : public QTabWidget {
public:
    explicit ContentTabs(QWidget*parent=nullptr);
    void watchPages();
protected:
    bool eventFilter(QObject*,QEvent*) override;
private:
    bool pending=false;
    void scheduleHeight();void fitHeight();
};
// Inline widgets only: no native popup/tooltip surfaces on numeric hover.
class NumericSlider final : public QWidget {
public:
    NumericSlider(QDoubleSpinBox *field,bool discrete); void refresh(){sync();}
    void setSliderRange(double minimum,double maximum);
protected:
    bool eventFilter(QObject*,QEvent*) override;
private:
    QDoubleSpinBox *field;QSlider *slider;bool discrete;
    bool fixedRange=false;double sliderMinimum=0,sliderMaximum=0;
    void sync();
};
class SpatialTimeline final : public QWidget {
public:
    explicit SpatialTimeline(QWidget*parent=nullptr);
    QStringList names;std::vector<std::vector<double>> keys;double duration=6,position=0;int selected=0;
    std::function<void(int,double)> scrub;
    std::function<void(int,int,double)> moveKey;
    void refresh();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    int dragTrack=-1,dragKey=-1;bool dragging=false;double pending=0;
    double timeAt(double x)const;double xAt(double t)const;
};
class StructurePreview final : public QWidget {
public:
    explicit StructurePreview(QWidget*parent=nullptr);
    spatial::StageModel model;int selected=-1;std::function<void(int)> select;
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
private:
    std::vector<QPolygonF> shapes;
};
// Owns one preview. A close request on the floating window docks that same widget.
class StructurePanel final : public QWidget {
public:
    explicit StructurePanel(StructurePreview*,QWidget*parent=nullptr);
    ~StructurePanel() override;
protected:
    bool eventFilter(QObject*,QEvent*) override;
private:
    StructurePreview*preview;QWidget*body;QVBoxLayout*bodyLayout;
    QDialog*floating=nullptr;QPushButton*expand,*popout;
    void detach();void reattach();
};
