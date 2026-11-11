#include "widgets.hpp"
#include "scene_evaluator.hpp"
#include "hover_help.hpp"
#include <QSlider>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QPainter>
#include <QMouseEvent>
#include <QEvent>
#include <QLineEdit>
#include <QDialog>
#include <QCloseEvent>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTabBar>
#include <QTimer>
#include <algorithm>
#include <cmath>
ContentTabs::ContentTabs(QWidget*p):QTabWidget(p){
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    connect(this,&QTabWidget::currentChanged,this,[this]{scheduleHeight();});
}
void ContentTabs::watchPages(){for(int i=0;i<count();++i)widget(i)->installEventFilter(this);scheduleHeight();}
bool ContentTabs::eventFilter(QObject*,QEvent*e){if(e->type()==QEvent::LayoutRequest||e->type()==QEvent::Show||e->type()==QEvent::Resize)scheduleHeight();return false;}
void ContentTabs::scheduleHeight(){if(pending)return;pending=true;QTimer::singleShot(0,this,[this]{pending=false;fitHeight();});}
void ContentTabs::fitHeight(){
    auto*p=currentWidget();if(!p||!p->layout())return;p->layout()->activate();
    auto*layout=p->layout();int page=std::max(p->minimumSizeHint().height(),layout->totalSizeHint().height());
    if(layout->hasHeightForWidth())page=std::max(page,layout->totalHeightForWidth(p->width()));
    const int required=page+tabBar()->sizeHint().height()+8;
    if(height()!=required||minimumHeight()!=required){setFixedHeight(required);updateGeometry();}
}
NumericSlider::NumericSlider(QDoubleSpinBox*f,bool d):field(f),discrete(d){
    auto*row=new QHBoxLayout(this);row->setContentsMargins(0,0,0,0);row->setSpacing(8);
    slider=new QSlider(Qt::Horizontal);slider->setObjectName(f->objectName()+".slider");
    slider->setMinimumWidth(70);slider->setSingleStep(1);slider->setPageStep(d?1:100);
    f->setButtonSymbols(QAbstractSpinBox::NoButtons);f->setDecimals(d?0:2);f->setSingleStep(d?1:.01);
    f->setMinimumWidth(92);f->setMaximumWidth(114);f->setToolTip(QString());
    row->addWidget(slider,1);row->addWidget(f);f->installEventFilter(this);
    connect(f,&QDoubleSpinBox::valueChanged,this,[this]{sync();});
    // QSignalBlocker in Dock refreshes only suppresses the spinbox, not its editor.
    // Keep the slider synchronized even during atomic multi-field UI refreshes.
    if(auto*editor=f->findChild<QLineEdit*>())connect(editor,&QLineEdit::textChanged,this,[this]{sync();});
    connect(slider,&QSlider::valueChanged,this,[this](int n){field->setValue(n/(discrete?1.0:100.0));});
    connect(slider,&QSlider::sliderReleased,this,[this]{sync();});sync();
}
void NumericSlider::setSliderRange(double minimum,double maximum){
    fixedRange=true;sliderMinimum=minimum;sliderMaximum=maximum;sync();
}
void NumericSlider::sync(){
    if(slider->isSliderDown())return;QSignalBlocker block(slider);const double factor=discrete?1:100;
    double lo=field->minimum(),hi=field->maximum();
    // Long world-coordinate ranges get a local +/-500 world-unit drag window.
    // Direct numeric entry still has the complete range; arrow keys step exactly .01.
    if(fixedRange){lo=sliderMinimum;hi=sliderMaximum;}
    else if(hi-lo>2000){lo=std::max(lo,field->value()-500);hi=std::min(hi,field->value()+500);}
    slider->setRange(int(std::lround(lo*factor)),int(std::lround(hi*factor)));slider->setValue(int(std::lround(field->value()*factor)));
}
bool NumericSlider::eventFilter(QObject*o,QEvent*e){if(o==field&&e->type()==QEvent::EnabledChange)slider->setEnabled(field->isEnabled());return QWidget::eventFilter(o,e);}
SpatialTimeline::SpatialTimeline(QWidget*p):QWidget(p){setObjectName("spatial.timeline");setMinimumWidth(360);setMouseTracking(true);setAccessibleName(QString::fromUtf8("运动时间轴：点击移动播放头，拖动菱形调整关键帧时间"));}
void SpatialTimeline::refresh(){setMinimumHeight(48+int(names.size())*34);update();}
double SpatialTimeline::timeAt(double x)const{return std::round(std::clamp((x-132)/std::max(1,width()-148),0.0,1.0)*duration*100)/100;}
double SpatialTimeline::xAt(double t)const{return 132+std::clamp(t/std::max(.01,duration),0.0,1.0)*(width()-148);}
void SpatialTimeline::paintEvent(QPaintEvent*){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor("#19212c"));
    p.setPen(QColor("#a4b4c8"));p.drawText(10,23,QString::fromUtf8("对象 / 秒"));
    for(int i=0;i<=6;++i){double t=duration*i/6;int x=int(xAt(t));p.setPen(QColor("#344256"));p.drawLine(x,30,x,height());p.setPen(QColor("#b4c5dc"));p.drawText(QRect(x-23,4,46,22),Qt::AlignCenter,QString::number(t,'f',duration<10?1:0));}
    for(int i=0;i<names.size();++i){const int y=36+i*34;if(i==selected)p.fillRect(0,y,width(),34,QColor("#293e55"));p.setPen(QColor("#d8e7f4"));p.drawText(QRect(8,y,118,34),Qt::AlignVCenter,p.fontMetrics().elidedText(names[i],Qt::ElideRight,116));
        if(size_t(i)<keys.size())for(size_t j=0;j<keys[size_t(i)].size();++j){const double t=(dragTrack==i&&dragKey==int(j))?pending:keys[size_t(i)][j];const QPointF c(xAt(t),y+17);QPolygonF d;d<<c+QPointF(0,-6)<<c+QPointF(6,0)<<c+QPointF(0,6)<<c+QPointF(-6,0);p.setPen(Qt::NoPen);p.setBrush(QColor("#f2c574"));p.drawPolygon(d);}}
    p.setPen(QPen(QColor("#4ee0d5"),2));p.drawLine(QPointF(xAt(position),29),QPointF(xAt(position),height()));
}
void SpatialTimeline::mousePressEvent(QMouseEvent*e){
    if(e->button()!=Qt::LeftButton)return;const int row=std::clamp(int((e->position().y()-36)/34),0,std::max(0,int(names.size())-1));selected=row;
    if(e->position().x()<132){if(scrub)scrub(row,position);update();return;}
    dragTrack=-1;dragKey=-1;dragging=true;
    if(e->position().y()>=36&&size_t(row)<keys.size())for(size_t j=0;j<keys[size_t(row)].size();++j)if(std::abs(xAt(keys[size_t(row)][j])-e->position().x())<9){dragTrack=row;dragKey=int(j);pending=keys[size_t(row)][j];break;}
    position=timeAt(e->position().x());if(scrub)scrub(row,position);update();
}
void SpatialTimeline::mouseMoveEvent(QMouseEvent*e){if(!dragging)return;position=timeAt(e->position().x());if(dragKey>=0)pending=position;else if(scrub)scrub(selected,position);update();}
void SpatialTimeline::mouseReleaseEvent(QMouseEvent*){if(!dragging)return;dragging=false;const int t=dragTrack,k=dragKey;dragTrack=dragKey=-1;if(k>=0&&moveKey)moveKey(t,k,pending);else if(scrub)scrub(selected,position);update();}
StructurePreview::StructurePreview(QWidget*p):QWidget(p){setObjectName("spatial.structure");setMinimumHeight(150);setAccessibleName(QString::fromUtf8("斜二测图层结构预览，点击选择图层"));}
void StructurePreview::paintEvent(QPaintEvent*){
    using namespace spatial;QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),QColor("#18212d"));shapes.clear();
    const bool motion=model.timelinePreview||model.playing;const double time=model.timelinePosition;
    std::vector<std::array<Vec3,4>> quads;double minX=1e9,minY=1e9,maxX=-1e9,maxY=-1e9;
    auto iso=[](Vec3 v){return QPointF(v.x+.55*v.z,v.y-.32*v.z);};
    std::vector<SourceSize> sizes;for(auto&l:model.layers)sizes.push_back({l.importWidth,l.importHeight});const auto evaluated=evaluateSceneAt(model,toTime(time),sizes,motion);Camera camera=evaluated.camera;
    auto bounds=[&](QPointF a){minX=std::min(minX,a.x());maxX=std::max(maxX,a.x());minY=std::min(minY,a.y());maxY=std::max(maxY,a.y());};bounds(iso(camera.position));
    for(const auto&l:model.layers){auto plane=evaluated.world[quads.size()].plane;quads.push_back(corners(plane,l.importWidth>0?l.importWidth:640,l.importHeight>0?l.importHeight:360));for(auto v:quads.back())bounds(iso(v));}
    const double scale=std::min((width()-50)/std::max(200.0,maxX-minX),(height()-56)/std::max(200.0,maxY-minY));
    auto screen=[&](Vec3 v){auto a=iso(v);return QPointF(25+(a.x()-minX)*scale,30+(a.y()-minY)*scale);};
    p.setPen(QColor("#9bb3cb"));p.drawText(10,18,QString::fromUtf8("斜二测 · X→ Y↓ Z↗ · 点击选层（示意比例）"));
    for(size_t i=0;i<quads.size();++i){QPolygonF polygon;for(int n:{0,1,3,2})polygon<<screen(quads[i][size_t(n)]);shapes.push_back(polygon);QColor c=QColor::fromHsv(int(i*67)%360,120,220);p.setPen(QPen(int(i)==selected?QColor("#ffe5a3"):c,int(i)==selected?3:1));c.setAlpha(55);p.setBrush(c);p.drawPolygon(polygon);p.setPen(QColor("#e3edf7"));p.drawText(polygon.boundingRect().topLeft()+QPointF(3,13),QString::number(i+1)+" "+QString::fromStdString(model.layers[i].label).left(16));}
    const auto cam=screen(camera.position);p.setBrush(QColor("#4ee0d5"));p.setPen(Qt::NoPen);p.drawEllipse(cam,5,5);p.setPen(QColor("#4ee0d5"));p.drawText(cam+QPointF(8,4),QString::fromUtf8("相机"));p.drawLine(cam,screen(add(camera.position,rotate({0,0,500},camera.rotation))));
}
void StructurePreview::mousePressEvent(QMouseEvent*e){for(int i=int(shapes.size())-1;i>=0;--i)if(shapes[size_t(i)].containsPoint(e->position(),Qt::OddEvenFill)){if(select)select(i);return;}}
StructurePanel::StructurePanel(StructurePreview*p,QWidget*parent):QWidget(parent),preview(p){
    setObjectName("spatial.structure-panel33");
    auto*layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(3);
    auto*header=new QHBoxLayout;expand=new QPushButton(QString::fromUtf8("▾ 图层结构"));expand->setObjectName("spatial.structure-expand33");expand->setCheckable(true);expand->setChecked(true);
    expand->setToolTip(QString::fromUtf8("折叠或展开斜二测结构示意图。位置、旋转、深度、选择和播放头与时间轴同步；只是观察视图，不会改变OBS输出。"));
    popout=new QPushButton(QString::fromUtf8("↗ 独立窗口"));popout->setObjectName("spatial.structure-detach33");popout->setToolTip(QString::fromUtf8("把同一个结构预览移到可调整大小的独立浮动窗口。关闭浮窗或点击收回，预览会返回时间轴下方并展开。"));
    header->addWidget(expand,1);header->addWidget(popout);layout->addLayout(header);
    body=new QWidget;body->setObjectName("spatial.structure-body33");bodyLayout=new QVBoxLayout(body);bodyLayout->setContentsMargins(0,0,0,0);bodyLayout->addWidget(preview);preview->setMaximumHeight(220);layout->addWidget(body);
    connect(expand,&QPushButton::toggled,this,[this](bool on){body->setVisible(on&&!floating);expand->setText(QString::fromUtf8(on?"▾ 图层结构":"▸ 图层结构"));});
    connect(popout,&QPushButton::clicked,this,[this]{if(floating)reattach();else detach();});
}
StructurePanel::~StructurePanel(){if(floating){floating->removeEventFilter(this);delete floating;floating=nullptr;}}
void StructurePanel::detach(){
    if(floating)return;floating=new QDialog(this,Qt::Window);floating->setObjectName("spatial.structure-float33");floating->setWindowTitle(QString::fromUtf8("Spatial FX · 图层结构"));floating->setAttribute(Qt::WA_DeleteOnClose,false);
    auto*layout=new QVBoxLayout(floating);auto*back=new QPushButton(QString::fromUtf8("↙ 收回时间轴下方"));back->setObjectName("spatial.structure-return33");back->setToolTip(popout->toolTip());layout->addWidget(back);layout->addWidget(preview,1);preview->setMaximumHeight(QWIDGETSIZE_MAX);
    connect(back,&QPushButton::clicked,this,[this]{reattach();});floating->installEventFilter(this);installHoverHelp(floating);floating->resize(720,430);body->hide();popout->setText(QString::fromUtf8("↙ 收回"));floating->show();
}
void StructurePanel::reattach(){
    if(!floating)return;auto*window=floating;floating=nullptr;window->removeEventFilter(this);bodyLayout->addWidget(preview);preview->setMaximumHeight(220);expand->setChecked(true);body->show();preview->show();popout->setText(QString::fromUtf8("↗ 独立窗口"));window->hide();window->deleteLater();
}
bool StructurePanel::eventFilter(QObject*o,QEvent*e){
    if(o==floating&&e->type()==QEvent::Close){static_cast<QCloseEvent*>(e)->ignore();reattach();return true;}return QWidget::eventFilter(o,e);
}
