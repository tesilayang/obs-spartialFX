#pragma once
#include <QWidget>
#if SPATIAL_PHASE >= 2
#include "model.hpp"
#include "widgets.hpp"
#include <array>
#include <QString>
class FxPanel;class CameraEditor;class QCheckBox;class QComboBox;class QDoubleSpinBox;class QLabel;class QListWidget;class QLineEdit;class QTabWidget;class QTableWidget;class QTimer;class QFormLayout;
class SpatialDock final : public QWidget {
public:
    explicit SpatialDock(QWidget *parent=nullptr);
    void refreshScene(bool force=false);
private:
    CameraEditor *editor=nullptr;FxPanel *fxPanel=nullptr;
    spatial::StageModel model;
    QString editorStageUuid;
    QString stageUuid,sceneUuid,lastStageIds,timelineSignature;
    bool loading=false;
    QComboBox *stageChoice=nullptr,*presetChoice=nullptr;
    QTabWidget *tabs=nullptr;
    QCheckBox *layerVisible=nullptr,*lightEnabled=nullptr,*shadows=nullptr;
    std::array<QDoubleSpinBox*,8> lightFields{};
    std::array<QDoubleSpinBox*,3> fxFields{};
    QLineEdit *presetName=nullptr;
    QLabel *sceneLabel=nullptr,*status=nullptr,*performance=nullptr,*lightColorPreview=nullptr,*shadowColorPreview=nullptr;
    QTimer *saveTimer=nullptr;
    QDoubleSpinBox *number(QFormLayout*,const char*,const char*,double,double,double=1);
    void commit();void loadStage();void syncControls();void syncLayers();void syncLayerFields();void syncKeys();
    void createStage(const spatial::StageModel *initial=nullptr);void importLayers(bool selected);void restoreLayer(const spatial::Layer&);void removeLayer();void confirmRemoveLayer();void restoreAll();void createDemo();
    void loadPresets();void savePreset();void applyPreset();QString configFile(const char *name);
    QComboBox *sceneChoice=nullptr,*lightChoice=nullptr,*lightBind=nullptr,*receiverChoice=nullptr;
    QListWidget *sourceList=nullptr;QLineEdit *layerName=nullptr;
    std::array<QCheckBox*,6> layerFlags{};
    std::array<QDoubleSpinBox*,3> layerExtra{};
    std::array<QDoubleSpinBox*,4> lightExtra{};
    QDoubleSpinBox *focusField=nullptr,*apertureField=nullptr;
    QCheckBox *lightVisible=nullptr;
    void initV2(QFormLayout*,QFormLayout*,QFormLayout*);void syncV2();void syncLayerV2();void refreshSources();
    spatial::Light &selectedLight();void syncLight();void addLight(int type);
    void message(const QString&,bool error=false);void updateStats();
    QCheckBox *fxEnabled=nullptr;SpatialTimeline *timeline=nullptr;StructurePreview *structure=nullptr;QComboBox *trackChoice=nullptr;
    std::array<QDoubleSpinBox*,2> rimFields{};
    void initV3(QFormLayout*,QFormLayout*,QFormLayout*);void syncTimeline();void scrub(int,double);
    
    void syncEvaluatedFields();
    int selectedLayer() const;
};
#else
class SpatialDock final : public QWidget {public:explicit SpatialDock(QWidget *parent=nullptr);};
#endif
