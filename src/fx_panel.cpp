#include "fx_panel.hpp"
#include "widgets.hpp"
#include "hover_help.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QToolButton>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QFileDialog>
#include <QColorDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QSignalBlocker>
#include <QPointer>
#include <QRegularExpression>
#include <algorithm>
#include <vector>
#include <map>
using namespace spatial;
namespace {
QString q(const std::string&s){return QString::fromStdString(s);}
QString node(const std::string&instance,const std::string&key){return "spatial.fx34."+q(instance)+"."+q(key);}
QString parameterHelp(const std::string&help){auto text=q(help);text.remove(QRegularExpression(QStringLiteral("\\[shape=[^\\]]*\\]\\s*")));return text.trimmed();}
std::vector<int> parameterShapes(const std::string&help){std::vector<int> result;const auto match=QRegularExpression(QStringLiteral("\\[shape=([0-9,]+)\\]")).match(q(help));if(match.hasMatch())for(const auto&value:match.captured(1).split(','))result.push_back(value.toInt());return result;}
struct ConditionalFxRow {QPointer<QWidget> field,label;std::vector<int> modes;};
QIcon icon(int group){
 QPixmap px(28,28);px.fill(Qt::transparent);QPainter p(&px);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(QColor("#c1c7ce"),1.4));p.setBrush(Qt::NoBrush);
 switch(group){
 case 1:for(int r:{4,7,10})p.drawEllipse(QPoint(14,14),r,r);break;
 case 2:p.drawLine(6,22,14,4);p.drawLine(14,4,23,22);p.drawLine(9,17,20,17);break;
 case 3:for(int x:{9,14,19})p.drawEllipse(QPoint(x,x==14?10:17),5,5);break;
 case 4:for(int i=0;i<8;++i){p.save();p.translate(14,14);p.rotate(i*45);p.drawLine(0,4,0,11);p.restore();}break;
 case 5:p.drawEllipse(6,5,16,16);p.drawLine(14,21,14,25);p.drawLine(10,24,18,24);break;
 case 6:p.drawRoundedRect(5,5,18,18,4,4);p.drawRoundedRect(8,8,12,12,2,2);break;
 case 7:p.drawArc(4,5,20,12,0,180*16);for(int x:{8,14,20})p.drawLine(x,16,x-2,23);break;
 case 8:for(int y:{6,10,14,18,22}){int x=y%8;p.drawLine(4+x,y,24-x,y);}break;
 case 9:p.drawLine(5,23,21,5);p.drawLine(21,5,24,8);p.drawLine(24,8,8,25);p.drawLine(5,23,8,25);break;
 case 10:for(int y:{7,14,21}){QPainterPath path;path.moveTo(3,y);path.cubicTo(10,y-9,18,y+9,25,y);p.drawPath(path);}break;
 case 11:p.drawPolygon(QPolygonF{QPointF(14,3),QPointF(24,9),QPointF(24,20),QPointF(14,26),QPointF(4,20),QPointF(4,9)});p.drawLine(4,9,14,15);p.drawLine(24,9,14,15);p.drawLine(14,15,14,26);break;
 case 12:p.drawRect(4,4,15,15);p.drawEllipse(10,10,14,14);break;
 default:p.drawRect(4,7,20,15);p.drawLine(4,7,14,2);p.drawLine(24,7,14,2);for(int x:{9,14,19})p.drawPoint(x,14);break;
 }return QIcon(px);
}
}
FxPanel::FxPanel(StageModel*m,QWidget*p):QWidget(p),model(m){setObjectName("spatial.fx-panel34");column=new QVBoxLayout(this);column->setContentsMargins(6,6,6,6);column->setSpacing(4);
 setStyleSheet("QWidget[fxParameters=\"true\"] QDoubleSpinBox, QWidget[fxParameters=\"true\"] QComboBox, QWidget[fxParameters=\"true\"] QPushButton { min-height:0; padding-top:0; padding-bottom:0; }");
 auto*help=new QLabel(QString::fromUtf8("\"受光与阴影\"特效组效果独立，子功能需要的附加光源会独立于灯光页主灯单独生效。"));help->setObjectName("spatial.fx-help34");help->setWordWrap(true);help->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Minimum);help->setStyleSheet("color:#b8c4d2; padding:0;");column->addWidget(help);
 auto*row=new QHBoxLayout;auto*add=new QPushButton("+");add->setObjectName("spatial.fx-add34");add->setFixedSize(32,28);add->setStyleSheet("QPushButton { padding:0; min-width:0; font-size:22px; font-weight:500; }");add->setToolTip(QString::fromUtf8("选择已导入当前舞台时间轴的素材，添加独立 FX。相同素材的不同实例分别设置。尚未导入时，请先到镜头与动画页导入。"));row->addWidget(add);row->addStretch();column->addLayout(row);connect(add,&QPushButton::clicked,this,[this]{addObject();});column->addStretch();refresh(true);}
Layer*FxPanel::layer(const std::string&id){for(auto&l:model->layers)if(l.instanceId==id)return &l;return nullptr;}
void FxPanel::setDocument(const QString&id){if(document!=id){++colorDocument;if(colorDialog)colorDialog->reject();document=id;signature.clear();refresh(true);}}
void FxPanel::refresh(bool force){QString next=document+"|";for(auto&l:model->layers)next+=q(l.instanceId)+":"+q(l.label)+":"+QString::number(l.instanceNumber)+":"+QString::number(l.fx.attached)+"|";if(force||signature!=next){signature=next;rebuild();}}
void FxPanel::addObject(){QMenu menu(this);for(auto&l:model->layers)if(!l.fx.attached){auto*a=menu.addAction(q(layerDisplayName(l)));const auto id=l.instanceId;connect(a,&QAction::triggered,this,[this,id]{if(auto*l=layer(id)){l->fx.attached=true;if(changed)changed();refresh(true);}});}if(menu.isEmpty()){auto*a=menu.addAction(QString::fromUtf8(model->layers.empty()?"请先在镜头与动画页导入素材":"全部时间轴素材已添加"));a->setEnabled(false);}menu.exec(mapToGlobal(QPoint(6,34)));}
void FxPanel::rebuild(){
 if(cards){column->removeWidget(cards);cards->hide();cards->deleteLater();}cards=new QWidget;auto*list=new QVBoxLayout(cards);list->setContentsMargins(0,0,0,0);list->setSpacing(4);column->insertWidget(2,cards);
 for(auto&entry:model->layers){if(!entry.fx.attached)continue;const auto id=entry.instanceId;auto*card=new QWidget;card->setObjectName(node(id,"card"));card->setProperty("fxCard",true);card->setStyleSheet("QWidget[fxCard=\"true\"] { border:1px solid #555b62; border-radius:3px; }");auto*body=new QVBoxLayout(card);body->setContentsMargins(7,5,7,6);body->setSpacing(4);list->addWidget(card);
  auto*head=new QHBoxLayout;auto*label=new QLabel(q(layerDisplayName(entry)));label->setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Preferred);label->setToolTip(label->text());head->addWidget(label,1);auto*remove=new QToolButton;remove->setText("×");remove->setObjectName(node(id,"remove"));remove->setToolTip(QString::fromUtf8("移除此素材的全部 FX，恢复原始画面。时间轴中的素材和关键帧会保留。"));head->addWidget(remove);body->addLayout(head);connect(remove,&QToolButton::clicked,this,[this,id]{if(QMessageBox::question(this,QString::fromUtf8("移除素材 FX"),QString::fromUtf8("移除此素材的所有 FX 并恢复原始画面？"),QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;if(auto*l=layer(id)){l->fx={};if(changed)changed();refresh(true);}});
  auto*icons=new QHBoxLayout;icons->setSpacing(0);icons->setContentsMargins(0,0,0,0);body->addLayout(icons);auto*details=new QWidget;auto*detailLayout=new QVBoxLayout(details);detailLayout->setContentsMargins(0,2,0,0);detailLayout->setSpacing(3);body->addWidget(details);
  auto buttons=std::make_shared<std::array<QToolButton*,13>>();auto panels=std::make_shared<std::array<QWidget*,13>>();
  for(int group=1;group<=13;++group){const auto index=size_t(group-1);const auto&g=fxGroups()[index];auto*b=new QToolButton;b->setObjectName(node(id,"group"+std::to_string(group)));b->setIcon(icon(group));b->setIconSize(QSize(24,24));b->setCheckable(true);b->setChecked(entry.fx.groups[index]);b->setMinimumWidth(20);b->setMaximumWidth(34);b->setMinimumHeight(30);b->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);b->setToolTip(q(g.name)+"\n"+q(g.help)+QString::fromUtf8("\n点击启用并展开；再次点击暂停这一组。只有同时勾选的子功能才处理画面。"));b->setStyleSheet("QToolButton { border:0; border-radius:3px; padding:0; } QToolButton:checked { background:#426c32; border:1px solid #a6d873; } QToolButton:hover { background:#50565c; }");icons->addWidget(b);(*buttons)[index]=b;
   auto*panel=new QWidget;auto*options=new QVBoxLayout(panel);options->setContentsMargins(0,0,0,0);options->setSpacing(0);detailLayout->addWidget(panel);panel->hide();(*panels)[index]=panel;auto*groupTitle=new QLabel(q(g.name));groupTitle->setStyleSheet("font-weight:600;");options->addWidget(groupTitle);
   std::map<int,QVBoxLayout*>qualitySections;
   for(const auto&def:fxCatalog()){if(def.group!=group)continue;auto*effectOptions=options;
    if(group==2&&def.shader>=2){
     const int section=def.shader==4?1:0;
     if(!qualitySections.count(section)){
      auto*fold=new QToolButton;fold->setObjectName(node(id,section?"quality.clarity.fold":"quality.aa.fold"));fold->setText(QString::fromUtf8(section?"清晰化":"抗锯齿"));fold->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);fold->setCheckable(true);fold->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
      fold->setToolTip(QString::fromUtf8(section?"展开 RCAS 清晰化设置。展开本身不启用效果；勾选子功能才处理画面。单遍五点采样，建议从较低程度开始。":"展开 SMAA / FXAA 设置。两种算法二选一；展开本身不启用效果。资源紧张时优先尝试 FXAA。这里只处理素材当前像素，不会消除后续舞台透视变换新产生的锯齿。"));
      auto*sectionBody=new QWidget;sectionBody->setObjectName(node(id,section?"quality.clarity.body":"quality.aa.body"));auto*items=new QVBoxLayout(sectionBody);items->setContentsMargins(8,0,0,0);items->setSpacing(0);options->addWidget(fold);options->addWidget(sectionBody);qualitySections[section]=items;
      bool expanded=false;for(const auto&candidate:fxCatalog())if(candidate.group==2&&(section?candidate.shader==4:(candidate.shader==2||candidate.shader==3))){auto*f=findFx(entry.fx,candidate.id);expanded|=f&&f->enabled;}
      fold->setChecked(expanded);fold->setArrowType(expanded?Qt::DownArrow:Qt::RightArrow);sectionBody->setVisible(expanded);
      connect(fold,&QToolButton::toggled,this,[fold,sectionBody](bool on){fold->setArrowType(on?Qt::DownArrow:Qt::RightArrow);sectionBody->setVisible(on);});
     }
     effectOptions=qualitySections[section];
    }
    const auto effectId=def.id;const auto*existing=findFx(entry.fx,effectId);auto*check=new QCheckBox(q(def.name));check->setObjectName(node(id,effectId+".enabled"));check->setToolTip(q(def.help));check->setChecked(existing&&existing->enabled);effectOptions->addWidget(check);
    check->setStyleSheet("QCheckBox { min-height:0; padding-top:0; padding-bottom:0; }");check->ensurePolished();check->setFixedHeight(std::max(17,check->fontMetrics().height()+2));
    auto*parameters=new QWidget;parameters->setProperty("fxParameters",true);parameters->setObjectName(node(id,effectId+".parameters"));auto*form=new QFormLayout(parameters);form->setContentsMargins(5,0,0,2);form->setVerticalSpacing(1);form->setHorizontalSpacing(6);form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);form->setLabelAlignment(Qt::AlignVCenter|Qt::AlignLeft);effectOptions->addWidget(parameters);parameters->setVisible(check->isChecked());
    // Build controls lazily on first enable. Disabled children have no parameter widgets or GPU work.
    auto build=[this,id,effectId,form,parameters]{if(parameters->property("built").toBool())return;parameters->setProperty("built",true);auto*l=layer(id);auto*definition=fxDefinition(effectId);if(!l||!definition)return;auto*f=findFx(l->fx,effectId);if(!f){l->fx.effects.push_back(makeFx(*definition));f=&l->fx.effects.back();}
     // Keep UI initialization independent of model storage: changed() may replace the model.
     const FxInstance initial=*f;
     auto edit=[this,id,effectId](std::function<void(FxInstance&)>fn){if(auto*l=layer(id))if(auto*f=findFx(l->fx,effectId)){fn(*f);if(changed)changed();}};
     auto conditionalRows=std::make_shared<std::vector<ConditionalFxRow>>();
     auto updateVisibility=[conditionalRows](int selected){for(const auto&row:*conditionalRows){const bool visible=std::find(row.modes.begin(),row.modes.end(),selected)!=row.modes.end();if(row.field)row.field->setVisible(visible);if(row.label)row.label->setVisible(visible);}};
     auto registerRow=[form,effectId,conditionalRows](const FxParameter&meta,QWidget*field){auto*label=form->labelForField(field);const auto help=parameterHelp(meta.help);field->setToolTip(help);if(label)label->setToolTip(help);std::vector<int> modes;if(effectId=="FX12.0"||effectId=="FX12.9")modes=parameterShapes(meta.help);else if(effectId=="FX11.0"&&meta.key=="radius")modes={0};if(!modes.empty())conditionalRows->push_back({field,label,std::move(modes)});};
     for(size_t j=0;j<definition->parameters.size();++j){const auto&meta=definition->parameters[j];if(effectId=="FX12.9"&&meta.key=="reverse"){auto*reverse=new QCheckBox;reverse->setObjectName(node(id,effectId+"."+meta.key));reverse->setChecked(initial.values[j]>0.5);form->addRow(q(meta.label),reverse);registerRow(meta,reverse);connect(reverse,&QCheckBox::toggled,this,[edit,j](bool on){edit([j,on](FxInstance&f){f.values[j]=on?1.0:0.0;});});}
      else if(!meta.choices.empty()){auto*mode=new QComboBox;mode->setFixedHeight(22);mode->setObjectName(node(id,effectId+"."+meta.key));for(auto&choice:meta.choices)mode->addItem(q(choice));mode->setCurrentIndex(int(initial.values[j]));form->addRow(q(meta.label),mode);registerRow(meta,mode);const bool selector=(effectId=="FX12.0"&&meta.key=="shape")||((effectId=="FX11.0"||effectId=="FX12.9")&&meta.key=="mode");connect(mode,&QComboBox::currentIndexChanged,this,[edit,j,selector,updateVisibility](int v){if(selector)updateVisibility(v);edit([j,v](FxInstance&f){f.values[j]=v;});});}
      else {auto*n=new QDoubleSpinBox;n->setObjectName(node(id,effectId+"."+meta.key));n->setRange(meta.minimum,meta.maximum);n->setValue(initial.values[j]);n->setKeyboardTracking(false);n->setFixedHeight(22);auto*slider=new NumericSlider(n,false);slider->setFixedHeight(22);if(meta.maximum-meta.minimum<=2000)slider->setSliderRange(meta.minimum,meta.maximum);form->addRow(q(meta.label),slider);registerRow(meta,slider);connect(n,&QDoubleSpinBox::valueChanged,this,[edit,j](double v){edit([j,v](FxInstance&f){f.values[j]=v;});});}}
     auto*n=new QDoubleSpinBox;n->setObjectName(node(id,effectId+".strength"));n->setRange(0,1);n->setValue(initial.strength);n->setKeyboardTracking(false);n->setFixedHeight(22);auto*slider=new NumericSlider(n,false);slider->setFixedHeight(22);slider->setToolTip(QString::fromUtf8("与原画面的混合比例：0 保留原画面，1 完整使用此效果。不会修改素材的基础坐标。"));form->addRow(QString::fromUtf8("混合强度"),slider);connect(n,&QDoubleSpinBox::valueChanged,this,[edit](double v){edit([v](FxInstance&f){f.strength=v;});});
     for(int c=0;c<2;++c)if(c?definition->color2:definition->color){
      const auto key=effectId+(c?".color2":".color");auto*row=new QWidget;row->setObjectName(node(id,key+".row"));auto*layout=new QHBoxLayout(row);layout->setContentsMargins(0,0,0,0);layout->setSpacing(4);
      auto*preview=new QLabel;preview->setObjectName(node(id,key+".preview"));preview->setAlignment(Qt::AlignCenter);preview->setMinimumWidth(80);preview->setFixedHeight(22);preview->setTextInteractionFlags(Qt::TextSelectableByMouse);layout->addWidget(preview,1);
      auto*b=new QPushButton(QString::fromUtf8("选择颜色"));b->setFixedHeight(22);b->setObjectName(node(id,key));b->setMinimumWidth(0);layout->addWidget(b,1);
      auto update=[preview=QPointer<QLabel>(preview)](Vec3 col){if(!preview)return;auto qcol=QColor::fromRgbF(float(col.x),float(col.y),float(col.z));preview->setText(qcol.name(QColor::HexArgb).toUpper());const auto ink=qcol.lightnessF()>.55?"#111111":"#ffffff";preview->setStyleSheet(QString("QLabel { background:%1; color:%2; border:1px solid #555b62; border-radius:2px; padding:2px; }").arg(qcol.name(),ink));};update(c?initial.color2:initial.color);
      row->setToolTip(QString::fromUtf8("左侧显示当前颜色及 ARGB 色值；点击选择颜色，调整时舞台实时预览，确定保留，取消或关闭恢复原颜色。不改变素材透明度。"));
      form->addRow(QString::fromUtf8(effectId=="FX12.9"?(c?"结束颜色":"起始颜色"):(effectId=="FX04.1"?(c?"目标颜色":"光效颜色"):(effectId=="FX05.5"?"阴影颜色":(c?"辅助颜色":"效果颜色")))),row);if(effectId=="FX12.9"&&c)conditionalRows->push_back({row,form->labelForField(row),{1,2}});
      connect(b,&QPushButton::clicked,this,[this,id,effectId,c,edit,update]{
       if(colorDialog){colorDialog->raise();colorDialog->activateWindow();return;}auto*l=layer(id);auto*f=l?findFx(l->fx,effectId):nullptr;if(!f)return;const auto original=c?f->color2:f->color;const auto epoch=colorDocument;
       auto*dialog=new QColorDialog(QColor::fromRgbF(float(original.x),float(original.y),float(original.z)),this);colorDialog=dialog;dialog->setObjectName("spatial.fx-color-dialog34");dialog->setWindowTitle(QString::fromUtf8("FX 颜色 · 实时预览"));dialog->setOption(QColorDialog::DontUseNativeDialog);dialog->setAttribute(Qt::WA_DeleteOnClose);dialog->setWindowModality(Qt::WindowModal);
       connect(dialog,&QColorDialog::currentColorChanged,this,[this,epoch,c,edit,update](const QColor&color){if(epoch!=colorDocument||!color.isValid())return;Vec3 v{color.redF(),color.greenF(),color.blueF()};edit([c,v](FxInstance&f){(c?f.color2:f.color)=v;});update(v);});
       connect(dialog,&QDialog::finished,this,[this,dialog,epoch,original,c,edit,update](int result){if(epoch==colorDocument&&result!=QDialog::Accepted){edit([c,original](FxInstance&f){(c?f.color2:f.color)=original;});update(original);}if(colorDialog==dialog)colorDialog.clear();});dialog->open();
      });
     }
     updateVisibility(int(initial.values[0]));
     if(definition->asset==FxAsset::Source||definition->asset==FxAsset::Image){auto*source=new QComboBox;source->setObjectName(node(id,effectId+".reference"));source->addItem(QString::fromUtf8("当前素材 / 无辅助源"),QString());for(auto&other:model->layers)source->addItem(q(layerDisplayName(other)),q(other.instanceId));source->setCurrentIndex(std::max(0,source->findData(q(initial.reference))));source->setToolTip(QString::fromUtf8("选择当前时间轴中的素材作为辅助画面。此引用读取原始素材，不会递归叠加其 FX。绑定到实例 ID，重排和改名不改变关联。"));form->addRow(QString::fromUtf8("辅助素材"),source);connect(source,&QComboBox::currentIndexChanged,this,[edit,source]{const auto ref=source->currentData().toString().toStdString();edit([ref](FxInstance&f){f.reference=ref;f.path.clear();f.pixels.reset();f.assetError.clear();});});}
     if(definition->asset==FxAsset::Image||definition->asset==FxAsset::Lut){auto*row=new QWidget;auto*buttons=new QHBoxLayout(row);buttons->setContentsMargins(0,0,0,0);buttons->setSpacing(3);auto*pick=new QPushButton(QString::fromUtf8(definition->asset==FxAsset::Lut?"加载 LUT":"加载纹理"));pick->setObjectName(node(id,effectId+".file"));auto*clear=new QToolButton;clear->setText("×");clear->setToolTip(QString::fromUtf8("清除本地文件，保留当前效果参数。"));buttons->addWidget(pick,1);buttons->addWidget(clear);form->addRow(QString::fromUtf8("本地文件"),row);auto*state=new QLabel;state->setWordWrap(true);state->setObjectName(node(id,effectId+".file-status"));form->addRow(state);auto update=[this,id,effectId,pick,state]{auto*l=layer(id);auto*f=l?findFx(l->fx,effectId):nullptr;if(!f)return;pick->setToolTip(q(f->path));state->setText(f->assetError.empty()?QString():q(f->assetError));};update();connect(pick,&QPushButton::clicked,this,[this,id,effectId,edit,update]{const auto*d=fxDefinition(effectId);auto path=QFileDialog::getOpenFileName(this,QString::fromUtf8("选择效果文件"),QString(),d->asset==FxAsset::Lut?"3D LUT (*.cube *.png)":"Images (*.png *.jpg *.jpeg *.bmp *.webp)");if(path.isEmpty())return;edit([path,d](FxInstance&f){f.path=path.toStdString();f.reference.clear();loadFxAsset(f,d->asset);});update();});connect(clear,&QToolButton::clicked,this,[edit,update]{edit([](FxInstance&f){f.path.clear();f.pixels.reset();f.assetError.clear();});update();});}
     auto*reset=new QPushButton(QString::fromUtf8("恢复此效果默认值"));reset->setFixedHeight(22);reset->setObjectName(node(id,effectId+".reset"));reset->setToolTip(QString::fromUtf8("恢复本效果的参数、强度、颜色和辅助素材默认值，保留启用状态。"));form->addRow(reset);connect(reset,&QPushButton::clicked,this,[this,id,effectId]{if(auto*l=layer(id)){const auto*d=fxDefinition(effectId);auto*f=findFx(l->fx,effectId);if(!d||!f)return;const bool enabled=f->enabled;*f=makeFx(*d);f->enabled=enabled;}else return;QPointer<FxPanel> panel(this);if(changed)changed();if(panel)QTimer::singleShot(0,panel.data(),[panel]{if(panel)panel->refresh(true);});});
     // OBS themes apply fonts and size constraints during polish. Compact only
     // after inheriting the final theme, so larger theme fonts are not clipped.
     for(auto*w:parameters->findChildren<QWidget*>())if(qobject_cast<QDoubleSpinBox*>(w)||qobject_cast<QComboBox*>(w)||qobject_cast<QPushButton*>(w)||dynamic_cast<NumericSlider*>(w)||w->objectName().endsWith(".preview")){w->ensurePolished();w->setFixedHeight(std::max(22,w->fontMetrics().height()+4));}
     installHoverHelp(parameters);
    };
    if(check->isChecked())build();connect(check,&QCheckBox::toggled,this,[this,id,effectId,parameters,build](bool on){auto*l=layer(id);auto*d=fxDefinition(effectId);if(!l||!d)return;
     if(on&&(effectId=="FX02.2"||effectId=="FX02.3")){
      const std::string sibling=effectId=="FX02.2"?"FX02.3":"FX02.2";if(auto*other=findFx(l->fx,sibling))other->enabled=false;
      if(auto*control=cards->findChild<QCheckBox*>(node(id,sibling+".enabled"))){QSignalBlocker block(control);control->setChecked(false);}
      if(auto*body=cards->findChild<QWidget*>(node(id,sibling+".parameters")))body->hide();
     }
     auto*f=findFx(l->fx,effectId);if(!f){l->fx.effects.push_back(makeFx(*d));f=&l->fx.effects.back();}f->enabled=on;if(on)build();parameters->setVisible(on);if(changed)changed();});
   }
   connect(b,&QToolButton::toggled,this,[this,id,index,panels,detailLayout](bool on){if(auto*l=layer(id)){l->fx.groups[index]=on;auto&order=l->fx.groupOrder;const int group=int(index)+1;order.erase(std::remove(order.begin(),order.end(),group),order.end());if(on){order.push_back(group);detailLayout->removeWidget((*panels)[index]);detailLayout->addWidget((*panels)[index]);}(*panels)[index]->setVisible(on);if(changed)changed();}});
   if(entry.fx.groups[index])panel->show();
  }
  for(int group:entry.fx.groupOrder)if(group>=1&&group<=13&&entry.fx.groups[size_t(group-1)]){auto*w=(*panels)[size_t(group-1)];detailLayout->removeWidget(w);detailLayout->addWidget(w);}
 }
 installHoverHelp(cards);
}
