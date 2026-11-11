# v3.0 验证记录
日期：2026-10-31；工作区 E:/obs spartialFX develop。状态只依据实际执行结果。

## 修改文件
- src/model.hpp / model.cpp：schema3、FX默认OFF、图层运动轨道、播放头、轮廓参数与旧时间域迁移。
- src/core.hpp / core.cpp：图层插值、短弧角度、统一播放时间、灯光绑定含缩放。
- src/widgets.hpp / widgets.cpp（新增）：内嵌0.01数值滑条、斜二测预览、多对象可拖关键帧时间轴。
- src/dock.hpp / dock.cpp / dock_actions.cpp / dock_v2.cpp / dock_v3.cpp（新增）：FX启停、多选/双击导入、拖动管理顺序、时间轴录帧/擦洗/暂停及参数同步。
- src/stage.cpp、data/effects/spatial.effect：主开关旁路、动态图层与绑定灯求值、独立可见灯光实体、方向性软Alpha内轮廓。
- src/plugin.cpp、CMakeLists.txt、installer/setup.cpp / setup.rc.in：3.0版本、新旧加载布局、33便携启动语义与原卸载identity。
- tests/core_tests.cpp / model_tests.cpp / native_control.py / obs_qa_driver.cpp、tests/v3_qa.inc / v3_render_probe.py / v3_performance_probe.py、tools/launch-v3-qa.ps1：新增及扩展回归。
- README.md、BUILDING.md、docs/v3/*、tools/package_v3.py：说明、状态和打包。

## 真实命令与证据
在工作区 PowerShell：

    & (Join-Path $PWD 'tools/build.cmd') 4
    & (Join-Path $PWD 'tools/test.cmd')
    ./tools/launch-v3-qa.ps1 -Major 33 -Scenario v3 -Output work/v3/obs33-ui-release
    ./tools/launch-v3-qa.ps1 -Major 32 -Scenario v3 -Output work/v3/obs32-ui-final
    python tests/v3_render_probe.py work/v3/gpu33-final
    python tests/v3_performance_probe.py work/v3/gpu33-final

命令中的 python 实际执行路径：C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe。GPU脚本运行在已启动的隔离OBS33、同目录native桥接及临时4457测试端口。

真实编译日志：work/v3/ui-first-build.log、lighting-build.log、extended-build.log、cache-build.log；最终CTest：work/v3/final-ctest.log；最终身份匹配编译为identity-final-build.log。所有编辑前失败证据保留 fx-red-test.log、timeline-red-build.log。

| 验证 | 状态 | 证据 |
|---|---|---|
| MSVC x64真实编译、/utf-8、Qt6 | PASS | build日志、CMake源码，19.44.35229.0 |
| Core/Model两组测试、FX OFF、图层插值/绑定/JSON/迁移 | PASS | final-ctest.log，2/2 |
| OBS32.2.2 安装器实际安装payload加载与UI操作 | PASS | installed32-final/dock-smoke.json，183项，0失败 |
| OBS33.0.1 新结构根DLL加载与UI操作 | PASS | identity33-ui，183项；system-layout33验证新布局，0失败 |
| OBS33独立portable33启动路径 | PASS | release-installed-path.json，实际加载1个ProgramData下的DLL |
| 旧bin与新root布局并存，新root只加载一次 | PASS | system-layout33/module-path.json |
| 数值滑条0.01、FX默认OFF、图层曲线、鼠标拖菱形/列表、预览不覆盖base | PASS | 两版本真实Qt事件183项检查 |
| 实际GPU光影、主开关关闭像素完全一致、可见灯体、绑定动画 | PASS | release-gpu33/render-tests.json，17项，0失败 |
| 原窗口捕捉延迟就绪与无边界导入、跨场景预设绑定回归 | PASS | import32/dock-smoke.json，23项，0失败 |
| 1080p/60连续测量 | PASS | 68.187秒、4072帧、0渲染丢帧、均值60.000fps、最大报告平均渲染0.47925ms |
| 已安装33的80,008次连续空间/相机/灯光编辑 | PASS | release-stress33，19项检查，0失败；该次OBS退出计数0 leaks |
| 安装2.0.0→3.0.0覆盖，运行中拒绝写入 | PASS | upgrade-install.log、install-while-running.log，退出码分别0/10 |
| 安装版重启后图层曲线、FX、播放头、锚定与sceneitem身份等12项恢复 | PASS | release-restart33/restart.json，12项 |
| 控制面板卸载登记及登记命令真正移除，重装恢复3.0.0 | PASS | control-panel-entry.json、uninstall-validation.json |
| 卸载保留无关文件及3个真实用户配置文件哈希 | PASS | uninstall-validation.json |
| Qt原生悬停回归，没有额外桌面级透明窗口 | PASS | hover-windows.json / hover-desktop-0.png，数值增减箭头已移除 |

## 明确未完成的验证
- 原始“偶发桌面白色蒙版”在旧版本上的可靠复现及底层根因确认：NOT REPRODUCED。已移除原生增减箭头悬停入口，改内嵌滑条；实测未出现额外Qt顶层蒙版，不能把未复现的旧根因标为PASS。
- F:/Shoost_v0.17.4 的交互式视觉对照：NOT RUN。本轮未实际操作/观察其rimlight，未反编译或复制其实现。新算法独立实现并通过实际GPU差分；不能声称已做到与Shoost完全同等的观感。
- OBS34及后续尚未发布版本：NOT RUN。适配已公布的新加载目录，不承诺未来ABI。
- NVIDIA实机、Win10 1909实机、长时间多小时直播、实际推流/录制：NOT RUN。本机AMD RX6900 XT上的隔离1080p60预览与调参压力已执行。
- 控制面板窗口内人工鼠标点击：NOT RUN；已验证登记项和同一卸载命令的实际行为。
- 部分先前OBS会话结束报告1个内存泄漏，最终压力会话为0；不宣称所有会话零泄漏。

## 测试异常与修正
首轮UI的滑条同步检查失败：QSignalBlocker刷新spinbox时没有刷新滑条，已修正并在两版本重跑183项全部通过。首轮GPU脚本遇到Windows原子文件替换时PermissionError，修改QA读桥接重试后完整17项通过；保留首次失败日志，不记成插件崩溃。

## 兼容依据
官方OBS 33.0.1于2026-10-29发布。PluginModuleLoader_Windows.cpp 和 .hpp 证明OBS_PLUGINS_PATH仍为复数，33语义变为含模块子目录的结构根；旧平铺变量改为OBS_LEGACY_PLUGINS_PATH / DATA_PATH。系统新布局根DLL与data并列，32仍用bin/64bit。安装器便携33隔离子树避免顺带加载其他ProgramData插件。

参考官方路径：
https://obsproject.com/kb/legacy-plugin-locations
https://obsproject.com/kb/legacy-plugin-developer-info
https://github.com/obsproject/obs-studio/blob/33.0.1/frontend/plugin-manager/PluginModuleLoader_Windows.cpp
https://github.com/obsproject/obs-studio/blob/33.0.1/frontend/plugin-manager/PluginModuleLoader.hpp

新增精确锚定使用源UUID + 场景UUID + sceneitem ID。同一输入可重复导入并独立运动/锚定；旧UUID配置继续读取。移除锚定层时灯光暂停并在选择器提示关联失效，避免静默跳到另一个同源图层。对应失败编译测试与通过测试见identity-red-build.log / identity-test.log。

最终清理：隔离OBS/窗口捕捉fixture已退出，4457无监听，测试驱动移出runtime加载目录，用户日常OBS配置未改动。最终安装版仍为3.0.0；卸载与重新安装的退出码均0。见work/v3/test-cleanup.json。
