# v3.2 开发记录

依据用户已接受的镜头与动画重制计划。当前目录没有Git仓库；以work/v3.2/baseline为v3.1源码基线，历史交付哈希保留在work/v3.2/historical-hashes.json。

## 设计落地

- src/animation.hpp承载TimeUs、参数元数据、COW Curve和插值，替代计划中多个小型time/tracks/parameter_registry文件；参数轨二分求值，同点更新，无静默关键帧数量裁剪。
- src/scene_evaluator.hpp/.cpp统一AuthoringPose、WorldPose、相机、精确身份灯光位置；渲染、属性和结构预览共同调用。保留v3.1的RzRyRx、1500参考距离与源布局补偿。
- src/editor.hpp/.cpp、editor_timeline.cpp、editor_audio.cpp分别负责控制器/属性、虚拟轨道与曲线、音频/事务；全部是OBS原生Qt6 Dock子部件。
- src/model.cpp保存schema4和独立legacy播放范围；src/stage.cpp使用不可变快照和轻量clock API，seek不全量复制/序列化十万帧，后置合成复用GPU资源。
- src/audio包含独立Windows MF解码与拍点算法；系统不支持的文件返回可见错误。UI本地离线分析、实时15秒捕获、手工BPM、候选预览和后台原子批次相互隔离。
- 旧隐藏摄像机控件暂保留为兼容桥，实际标签页与所有可见编辑行为使用CameraEditor；景深控件已迁往FX页。后续可在有单独回归的维护改动中删去历史桥。

## 审查后的关键修正

撤销仅恢复运动数据，防止覆盖后续灯光/FX。场景/绑定顺序改变清除瞬时选区和历史；后台批次核对revision+完整目标身份+原曲线共享存储+媒体指纹。媒体指纹覆盖UUID、类型、local_file、大小/mtime、速度；解码完成、候选预览、写入开始/提交四个边界检查。

贝塞尔用de Casteljau及绝对数值手柄保留零值差过冲；旧分段三次用归一化legacy_from/to保存子区间，奇数微秒拆段也通过逐点回归。旧播放结束时间独立于最长关键帧保存，既不丢范围外帧也不捏造内容尾帧。

媒体联动明确由用户选择，来源/偏移修改发布新快照；渲染从有效媒体时间读取主时钟，UI处理结束/循环。锁定对象/轨的输入、粘贴、曲线手柄和等比缩放辅助都检查锁定。

最终高级原生检查发现OBS主窗口抢走时间轴Ctrl+C/V，editor_timeline.cpp增加ShortcutOverride事件处理；复制/粘贴/删除由时间轴原处理器执行。修复后45项高级检查通过，OBS32/33镜头操作各101项、音频集成43项通过；最终版本号3.2.0。最终DLL与旧30分钟测试DLL的哈希边界、最终60秒测试和未实现的交互细节均在VALIDATION.md单列，不混为同一轮完整验收。

## 实际文件更改清单

| 文件 | 相对v3.1基线 |
|---|---|
| BUILDING.md | NEW |
| CMakeLists.txt | MODIFIED |
| README.md | MODIFIED |
| data/effects/spatial.effect | MODIFIED |
| docs/architecture/SPATIAL_COORDINATE_CONTRACT.md | MODIFIED |
| docs/audio-v32.md | NEW |
| docs/superpowers/plans/2026-10-31-camera-timeline-remake.md | MODIFIED |
| docs/v3.2/DEVELOPMENT_RECORD.md | NEW |
| docs/v3.2/MIGRATION.md | NEW |
| docs/v3.2/USER_GUIDE.md | NEW |
| docs/v3.2/VALIDATION.md | NEW |
| installer/setup.cpp | MODIFIED |
| installer/setup.rc.in | MODIFIED |
| src/animation.hpp | NEW |
| src/audio/audio_decoder_win.cpp | NEW |
| src/audio/audio_decoder_win.hpp | NEW |
| src/audio/beat_analyzer.cpp | NEW |
| src/audio/beat_analyzer.hpp | NEW |
| src/audio/energy_accumulator.hpp | NEW |
| src/dock.cpp | MODIFIED |
| src/dock.hpp | MODIFIED |
| src/dock_actions.cpp | MODIFIED |
| src/dock_v2.cpp | MODIFIED |
| src/dock_v3.cpp | MODIFIED |
| src/editor.cpp | NEW |
| src/editor.hpp | NEW |
| src/editor_audio.cpp | NEW |
| src/editor_timeline.cpp | NEW |
| src/model.cpp | MODIFIED |
| src/model.hpp | MODIFIED |
| src/plugin.cpp | MODIFIED |
| src/scene_evaluator.cpp | NEW |
| src/scene_evaluator.hpp | NEW |
| src/stage.cpp | MODIFIED |
| src/stage.hpp | MODIFIED |
| src/widgets.cpp | MODIFIED |
| tests/animation_tests.cpp | NEW |
| tests/audio_probe.cpp | NEW |
| tests/beat_tests.cpp | NEW |
| tests/camera_remake_probe.py | NEW |
| tests/camera_stability_probe.py | NEW |
| tests/generate_audio_fixtures.py | NEW |
| tests/model_tests.cpp | MODIFIED |
| tests/native_control.py | MODIFIED |
| tests/obs_qa_driver.cpp | MODIFIED |
| tests/scene_evaluator_tests.cpp | NEW |
| tests/v31_parameter_probe.py | MODIFIED |
| tests/v32_qa.inc | NEW |
| tests/v3_performance_probe.py | MODIFIED |
| tests/v3_render_probe.py | MODIFIED |
| tools/package_v32.py | NEW |
| tools/run_v32_installed_qa.ps1 | NEW |
| tools/verify_v32.py | NEW |

## 验证与边界

构建/运行命令、截图、实际数字和NOT RUN项目集中在VALIDATION.md；坐标规则集中在../architecture/SPATIAL_COORDINATE_CONTRACT.md。历史红测试、初次错误日志、审查和修复记录保留在证据包，不以最后通过覆盖之前实际发生的问题。

未做人工参与者上手实验，不能将面板内四步指引等同于已测用户学习成本。未提供音乐识别通用准确率；变速/网络/VLC时间映射也不列为完整媒体主时钟支持。
