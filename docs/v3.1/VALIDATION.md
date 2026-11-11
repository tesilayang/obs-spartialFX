# OBS Spatial FX v3.1 验收记录

日期：2026-10-31（Asia/Shanghai）。工作区：E:/obs spartialFX develop。版本：3.1.0。

## 本轮交付与修改

- 图层 XYZ 滑条固定映射 −2000～2000，0 在中间；每步 0.01。滑条拖动不再使用 v3.0 的现值 ±500 移动窗口。
- 数值框仍接受 −100000～100000；超范围时只把滑块显示在端点，不修改真实图层/关键帧数值。
- 三个进阶参数增加常驻说明；“深度顺序”更名为“同深度叠放顺序”。实际效果、生效条件、公式和局限见 PARAMETERS.md。
- 安装器、Dock 标题、模块日志与控制面板登记更新到 3.1.0；沿用原插件身份覆盖升级。

实际修改：src/widgets.hpp / widgets.cpp、src/dock.cpp / dock_v2.cpp / plugin.cpp、CMakeLists.txt、installer/setup.cpp / setup.rc.in、tests/obs_qa_driver.cpp、README.md、BUILDING.md。

新增：tests/v31_qa.inc、tests/v31_parameter_probe.py、tools/package_v31.py / verify_v31.py、docs/v3.1/PARAMETERS.md / VALIDATION.md。

完整差异：work/v3.1/source-diff.txt、modified-files.json。无 Git 仓库，编辑前文件保存在 work/v3.1/baseline；旧 v1/v2/v3.0 具名交付文件按哈希保留。对 v3.0 源码包比对确认 src/stage.cpp、src/core.cpp、src/model.cpp 及 shader 字节未变，本轮没有修改三个参数的渲染算法或配置 schema。

## 真实构建和运行命令

工作区 PowerShell：

    & (Join-Path $PWD 'tools/build.cmd') 4
    & (Join-Path $PWD 'tools/test.cmd')
    ./tools/launch-v3-qa.ps1 -Major 33 -Scenario v31 -Output work/v3.1/obs33-final
    ./tools/launch-v3-qa.ps1 -Major 32 -Scenario v31 -Output work/v3.1/obs32-final
    ./tools/launch-v3-qa.ps1 -Major 33 -Scenario v3 -Output work/v3.1/regression33
    ./tools/launch-v3-qa.ps1 -Major 32 -Scenario v3 -Output work/v3.1/regression32
    ./tools/launch-v3-qa.ps1 -Major 33 -Scenario v3hold -Output work/v3.1/parameters-gpu33
    python tests/v31_parameter_probe.py work/v3.1/parameters-gpu33
    python tests/v3_render_probe.py work/v3.1/parameters-gpu33
    build/OBS_Spatial_FX_V3.1_Setup.exe /SILENT /LOG="E:/obs spartialFX develop/work/v3.1/upgrade-install.log"

实际 Python：C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe。GPU 两个脚本使用同一个 hold 实例；4457 仅在隔离 OBS33 测试时打开，结束已恢复关闭。安装器使用 Start-Process -PassThru 和 WaitForExit；安装版再通过其 /LAUNCH 参数启动 runtime/obs33 执行同一 v31 场景。

工具链：VS2022 MSVC 19.44.35229.0 x64、CMake 4.4.5、Ninja、Qt6；/utf-8 /W4 /permissive- /we4996。实际测试 OBS32.2.2 和 OBS33.0.1；本轮产品构建仅 SPATIAL_PHASE=4。

## 实测结果

| 项目 | 状态 | 真实证据（均位于 work/v3.1） |
|---|---|---|
| 真实编译、链接 x64 DLL 和 3.1 安装器 | PASS | green-build.log、final-build.log；无编译警告/错误 |
| Core / Model CTest | PASS，2/2 | ctest.log |
| OBS33 新控件回归 | PASS，147 项，0 失败 | obs33-final/dock-smoke.json |
| OBS32 新控件回归 | PASS，147 项，0 失败 | obs32-final/dock-smoke.json |
| 原有 v3.0 Dock、图层/时间轴/锚定回归 | PASS，各 183 项，0 失败 | regression32、regression33 的 dock-smoke.json |
| 三个图层参数实际 GPU 像素对照 | PASS，11 项，0 失败 | parameters-gpu33/parameter-tests.json、parameter-gpu.log |
| 原有光照、阴影、景深、视差及动画 GPU 回归 | PASS，17 项，0 失败 | parameters-gpu33/render-tests.json、render-regression.log |
| 3.0.0→3.1.0 实际覆盖安装 | PASS，退出码 0 | upgrade-before.json、upgrade-after.json、upgrade-install.log |
| 安装后从 ProgramData 实际加载唯一新 DLL | PASS | installed-module-path.json；已安装 DLL 与 build SHA256 相等 |
| 最终安装版的同一组控件操作 | PASS，147 项，0 失败 | installed33/dock-smoke.json、installed-launch.log |
| OBS 运行时安装拒绝覆盖 | PASS，退出码 10 | install-guard.json、install-while-running.log |
| 真实用户 OBS 配置保留 | PASS，66 个 JSON/INI 文件哈希不变，无新增 | user-config-before.json、cleanup-and-user-config.json |
| QA 退出、测试端口及驱动清理 | PASS | cleanup-and-user-config.json；0 个 OBS，4457 无监听，两套隔离 WebSocket 关闭 |
| 交付包完整性、历史包保留、安装载荷审计 | PASS，29 项，0 失败 | final-package-audit.json、package-verification.log |

控件回归包含真实 Qt 鼠标拖动至 ±2000、松手后的滑块位置、0.01 方向键步进、手工输入 ±3456.78 及 ±100000、模型值、刷新和切层同步，未把只检查控件存在当作功能通过。

实际画面对照确认：同深度交换顺序有差异，不同深度交换顺序像素完全一致；默认正对镜头下视差倍率 0/1 初始画面一致，横移相机后差异明显；Z=0 不受倍率影响。FX关闭、aperture=0、处于焦平面这三种状态下，景深倍率 0/4 的对照分别像素完全一致。开启 FX、景深及离焦后，倍率 0/1/4 的原生 1080p GPU 截图有明确差异。

截图：obs33-final/layer-parameter-help.png、installed33/layer-parameter-help.png；parameters-gpu33/blur-comparison.png 是三个真实 GPU 截图的 1:1 局部裁切拼图，没有生成或模拟效果图。对应全尺寸原图一并保留。OBS 主界面与五个标签页截图在各 UI 证据目录。

## 失败过程与限制

- 修复前在真实 OBS33 执行新增测试有 68 个失败；保留 red-obs33/dock-smoke.json、red-build.log。这是确认旧行为的红测试，不计为最终通过。
- 首次 GPU 脚本在 OBS 完成初始化前发 CreateScene，收到 207 “OBS is not ready”；属于 QA 启动时序。已改为等待 GetSceneList 可用后执行，并完整重跑通过。原日志保留 parameter-startup-first.log；无插件崩溃。
- 该版本景深仍为五点纹理采样，中心采样占 50%，并非完整光圈散景；增加半径可能产生高频重影。本轮只确认、说明原算法，不声称已升级为影视级景深。
- 部分回归 OBS 会话退出报告 1 个内存泄漏计数；最终安装版控件会话为 0。全部原日志保留，不宣称所有会话零泄漏。
- v3.1 专项长时间性能/压力测试、多小时直播、推流录制、NVIDIA/Win10 实机、未来 OBS 版本、重新完整卸载流程：NOT RUN。本轮保留原卸载入口和加载逻辑，并实际验证升级及安装版加载；历史 v3.0 结果见 ../v3/VALIDATION.md。
- 原偶发桌面白蒙版的底层根因仍未可靠复现；本轮不新增根因修复声明。

最终安装器、源码、证据、符号包位于 dist/OBS_Spatial_FX_V3.1_*；SHA256SUMS.txt 和 V31_RELEASE.json 记录最终哈希。包完整性及历史文件保留情况见 work/v3.1/final-package-audit.json。
