#pragma once
#include "scene_evaluator.hpp"
#include <QWidget>
#include <QElapsedTimer>
#include <functional>
#include <atomic>
#include <thread>
class QComboBox;class QDoubleSpinBox;class QCheckBox;class QLabel;class QPushButton;class QFormLayout;class QVBoxLayout;
namespace spatial {namespace audio {struct Analysis;}}
struct KeyRef {int object=-1;std::string parameter;spatial::TimeUs time=0;};
class TimelineWidget final:public QWidget {
public:
 explicit TimelineWidget(spatial::StageModel*,QWidget*parent=nullptr);
 spatial::StageModel *model;double viewStart=0,viewSpan=6;int active=-1;std::string parameter;spatial::TimeUs position=0;
 std::vector<spatial::TimeUs> candidates;std::vector<KeyRef> selection;
 std::function<void(int,std::string)> select;std::function<void(int,int)> reorder;
 void remapLayers(int from,int to);
 std::function<void(spatial::TimeUs)> seek;
 std::function<void(const std::vector<KeyRef>&,spatial::TimeUs)> move;
 std::function<void()> remove,record,copy,paste;
 void rebuild();void zoom(double,double anchor=-1);void pan(double);void reveal(double);void fit();
 double snap(double seconds)const;bool snapping=true;
protected:
 bool event(QEvent*)override;
 void paintEvent(QPaintEvent*)override;void mousePressEvent(QMouseEvent*)override;void mouseMoveEvent(QMouseEvent*)override;void mouseReleaseEvent(QMouseEvent*)override;void mouseDoubleClickEvent(QMouseEvent*)override;void wheelEvent(QWheelEvent*)override;void keyPressEvent(QKeyEvent*)override;
private:
 struct Row {int object;std::string param;QString label;bool summary=false;};std::vector<Row> rows;std::vector<int> collapsed;int knownLayers=0;int firstRow=0;bool dragging=false,panning=false,rubber=false;QPointF origin,current;spatial::TimeUs delta=0;
 int reorderFrom=-1,reorderTo=-1;bool reordering=false;
 double xAt(double)const;double timeAt(double)const;std::vector<KeyRef> keys(const Row&)const;int rowAt(double)const;
};
class CurveWidget final:public QWidget {
public:
 spatial::Curve curve;spatial::TimeUs selected=0;std::function<void(spatial::AnimKey,spatial::AnimKey)> handles;
 std::function<void(spatial::TimeUs)> selectKey;std::function<void(spatial::TimeUs,spatial::AnimKey)> editKey;
 bool positionsLocked=false,readOnly=false;
 explicit CurveWidget(QWidget*p=nullptr);void setCurve(const spatial::Curve&,spatial::TimeUs,const std::string& parameter="x");
 void fit();
protected:void paintEvent(QPaintEvent*)override;void mousePressEvent(QMouseEvent*)override;void mouseMoveEvent(QMouseEvent*)override;void mouseReleaseEvent(QMouseEvent*)override;void keyPressEvent(QKeyEvent*)override;
private:
 QPointF p0,p1,p2,p3,origin;spatial::AnimKey left,right,originalKey;spatial::Curve dragCurve;spatial::TimeUs dragSelected=0;int drag=0;bool segment=false,moved=false;
 double valueLow=0,valueHigh=1,timeLow=0,timeHigh=1;std::string parameterId="x";
 QRectF plot;QPointF point(spatial::TimeUs,double)const;void geometry(bool fitRange);void change(QPointF);
};
class CameraEditor final:public QWidget {
public:
 explicit CameraEditor(spatial::StageModel*,QWidget*parent=nullptr);~CameraEditor();
 std::function<void()> changed,clockChanged,removeObjectRequested;std::function<void(int)> layerSelected;
 std::function<void(QString,bool)> message;
 QFormLayout *importForm=nullptr,*layerPropertiesForm=nullptr;QVBoxLayout *previewLayout=nullptr;
 TimelineWidget *timeline=nullptr;int active=-1;std::string activeParameter="x";
 void resetDocument();void refresh();void refreshValues();void tick(double);void selectLayer(int);
 void reorderLayer(int from,int to);
 void editValue(int,const std::string&,double);void record();void removeKeys();void play();void pause();void seek(spatial::TimeUs);void reset(bool all);void undo();void redo();
private:
 spatial::StageModel *model;std::vector<spatial::StageModel> undoStates,redoStates;std::vector<KeyRef> clipboard;std::vector<spatial::AnimKey> copied;
 QString identity,bindingSignature;uint64_t editRevision=0,bakeGeneration=0;std::atomic_bool cancelBake{false};std::thread bakeThread;bool syncing=false;QElapsedTimer coalesce;std::string lastEdit;
 struct Field {QWidget*row;QCheckBox*animated;QDoubleSpinBox*value;QPushButton*diamond;};std::map<std::string,Field> fields;
 QComboBox*objectChoice=nullptr,*interpolation=nullptr,*rotationMode=nullptr,*mediaChoice=nullptr,*beatRatio=nullptr,*curveChoice=nullptr;
 QCheckBox*orbitMode=nullptr,*lookAt=nullptr,*lock=nullptr,*scaleLock=nullptr,*mediaLink=nullptr;
 QWidget*orbitGroup=nullptr;QPushButton*autoKey=nullptr,*positionLock=nullptr;std::string curveParameter;spatial::TimeUs selectedKeyTime=0;
 QLabel*state=nullptr,*length=nullptr,*audioStatus=nullptr;QDoubleSpinBox*playhead=nullptr,*keyTime=nullptr,*keyValue=nullptr,*outX=nullptr,*outY=nullptr,*inX=nullptr,*inY=nullptr;
 QDoubleSpinBox*bpm=nullptr,*offset=nullptr,*minimum=nullptr,*audioStart=nullptr,*audioEnd=nullptr,*customRatio=nullptr;
 CurveWidget*curve=nullptr;QLabel*segmentLabel=nullptr;QPushButton*resetCurrent=nullptr,*removeObject=nullptr;std::atomic_bool cancelAudio{false};std::thread audioThread;uint64_t audioGeneration=0;std::shared_ptr<spatial::audio::Analysis> analysis;
 QString analyzedUuid,analyzedFile,analyzedStamp,candidateStamp;QWidget*waveform=nullptr;QElapsedTimer tapTimer;std::vector<qint64> taps;
 bool validParameter(int,const std::string&)const;QString objectName(int)const;QString objectId(int)const;
 void remember(const std::string&group="");void apply();void selectObject(int,const std::string& parameter="");void enableParameter(const std::string&,bool);
 void configureHelp();void setKey(const std::string&,double);void keySelection();void changeKey();void changeSegment();void editCurveKey(spatial::TimeUs,spatial::AnimKey);void moveKeys(const std::vector<KeyRef>&,spatial::TimeUs);void navigate(const std::string&,bool);void navigateCurrent(bool);
 void initAudio(QFormLayout*);void refreshMedia();void analyze();void previewBeats();void bakeBeats();void syncMedia(bool playing);
 void restoreMotion(const spatial::StageModel&);void copyKeys();void pasteKeys();
 struct LiveCapture {std::vector<float> pcm;std::atomic<size_t> frames{0};unsigned channels=0,rate=0;obs_source_t*source=nullptr;};
 std::unique_ptr<LiveCapture> liveCapture;void captureLive();void stopCapture();static void audioCapture(void*,obs_source_t*,const audio_data*,bool);
};
