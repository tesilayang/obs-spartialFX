from pathlib import Path
p=Path("src/dock.hpp");s=p.read_text(encoding="utf-8-sig");s=s.replace('void message(const QString&', '''QComboBox *sceneChoice=nullptr,*lightChoice=nullptr,*lightBind=nullptr,*receiverChoice=nullptr,*motionChoice=nullptr;
    QListWidget *sourceList=nullptr;QLineEdit *layerName=nullptr;
    std::array<QCheckBox*,6> layerFlags{};
    std::array<QDoubleSpinBox*,3> layerExtra{},targetFields{};
    std::array<QDoubleSpinBox*,4> lightExtra{};
    QDoubleSpinBox *speedField=nullptr,*durationField=nullptr,*focusField=nullptr,*apertureField=nullptr;
    QCheckBox *lookAtCheck=nullptr,*previewCheck=nullptr,*lightVisible=nullptr;
    void initV2(QFormLayout*,QFormLayout*,QFormLayout*);void syncV2();void syncLayerV2();void refreshSources();
    spatial::Light &selectedLight();void syncLight();void addLight(int type);void cameraMove(int action);void motionShot();
    void message(const QString&''');p.write_text(s,encoding="utf-8")
p=Path("src/dock.cpp");s=p.read_text(encoding="utf-8-sig");s=s.replace('1.0 · OBS NATIVE','2.0 · OBS NATIVE');s=s.replace('model.light','selectedLight()')
s=s.replace('auto*fxForm=page(tabs,"FX 特效");','initV2(layerForm,cameraForm,lightForm);\n    auto*fxForm=page(tabs,"FX 特效");')
s=s.replace('Alpha 简化阴影 + 深色接收背景','素材间 Alpha 软阴影')
s=s.replace('单点漫反射 · 平面 Alpha 投影 · 无 CUDA 依赖。\\\\n简化阴影为清晰轮廓，不含柔化或多物体遮挡求解。','点光源 / 面光源 · 接收层与前景交互 · 无 CUDA 依赖。')
s=s.replace('loading=false;syncLayers();syncKeys();','loading=false;syncLayers();syncKeys();syncV2();')
s=s.replace('layerVisible->setChecked(l.visible);}loading=false;}','layerVisible->setChecked(l.visible);}loading=false;syncLayerV2();}')
p.write_text(s,encoding="utf-8")
p=Path("CMakeLists.txt");s=p.read_text(encoding="utf-8-sig").replace('src/dock.cpp src/dock_actions.cpp','src/dock.cpp src/dock_actions.cpp src/dock_v2.cpp');p.write_text(s,encoding="utf-8")
