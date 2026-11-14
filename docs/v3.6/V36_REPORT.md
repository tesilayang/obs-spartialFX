# OBS Spatial FX v3.6.0 开发与交付记录

日期：2026-11-13。基线：v3.5.0，提交 `055034ce9380c80cf19460bd127aafea9f4551d3`。本次交付对应 `codex/v3.6-presets` 工作区源码快照，精确文件清单与哈希见交付清单。本次未提交、推送或发布 GitHub，未更新使用说明书。

## 1. 本版功能

- 预设页增加八张原生卡片：色块开场、斜带交叉、圆窗聚焦、分层漂浮、卡片展示、波普律动、快门切换、极简缓动。卡片提供程序动画缩略图及变体。
- 直接绑定当前舞台已导入图层；主体、A/B、背景、前景、遮罩接收者及锚点按 instanceId 保存。同源多实例独立，重命名和重排保持关系。
- 可调动作强度、开始时间、周期、次数、配色及空间位置。默认只移动绑定图层，摄像机需主动开启。支持秒制与 1/4、1/2、1、2、4、8 拍，读取现有 BPM 和拍点偏移。
- 有限动作生成可编辑参数关键帧；循环动作使用绝对微秒求值的有界块，定位可重现。有限区间可烘焙为关键帧，最多 10001 个采样时刻。
- 原生程序图形提供色块、斜带、圆/椭圆、圆环、条纹、网格、棋盘和多边形。支持彩色装饰、裁切遮罩及组合模式，可放到指定深度或跟随图层。
- 程序图形是独立时间轴对象，可选中、隐藏、调整空间参数及确认后删除，不占普通素材的 32 个导入槽。每个舞台最多 128 个图形、512 个循环块，应用前按派生副本总数检查。
- 临时预览、取消、应用、整次撤销、移除和烘焙均接入完整文档事务；锁定、无效角色及区间冲突阻止静默覆盖。
- 保存、重启、工程 JSON、完整 ZIP 场景包及旧整套预设均纳入身份重映射；旧整套预设入口保留在折叠区。
- 安装器升级为 3.6.0，覆盖升级保留配置；继续提供控制面板卸载与 OBS 运行拦截。

本版使用 C++/Qt6/libobs 原生实现，无新增运行时依赖。研究仓库用于构图、效果和职责设计参考；未引入其脚本、演示素材或渲染运行时。使用预设无需联网或 AI 账号。

## 2. 实际修改文件

| 文件 | 改动 |
|---|---|
| `src/presets/types.hpp`、`presets.hpp/.cpp` | 数据、模板目录、编译、循环、校验、移除、烘焙 |
| `src/presets/graphics.hpp/.cpp` | 缓存几何绘制、世界/局部/屏幕遮罩、深度混排；shader 内嵌于 DLL |
| `src/presets/panel.hpp/.cpp` | 原生卡片、角色和选项、预览及事务回调 |
| `src/model.hpp/.cpp`、`src/scene_evaluator.hpp/.cpp` | 快照持久化、统一时间和位姿求值 |
| `src/stage.hpp/.cpp` | 原生图形与现有图层合成及资源生命周期 |
| `src/editor.hpp/.cpp`、`editor_timeline.cpp`、`editor_help.cpp` | 图形时间轴对象、完整文档撤销、确认操作与帮助 |
| `src/dock.hpp/.cpp`、`dock_actions.cpp`、`dock_v3.cpp` | 面板集成、预览隔离、旧预设更新与选择安全 |
| `src/widgets.cpp` | 大时间值保留数值框真实值，滑条先限幅再转换 |
| `src/project/project_obs.cpp` | 工程中的来源与新对象身份重映射 |
| `src/plugin.cpp`、`CMakeLists.txt`、`installer/setup.cpp`、`setup.rc.in` | 版本、目标、测试及安装器 |
| `tests/preset_tests.cpp`、`preset_panel_tests.cpp`、`v36_qa.inc`、`obs_qa_driver.cpp` | 纯数学、面板和真实 OBS 自动化验收 |
| `tests/v36_render_probe.py`、`v36_template_probe.py`、`v36_panel_contract.md` | 实际像素、模板、性能与控件契约 |
| `tools/test-installer-v36.ps1`、`package-v36.py` | 升级/回滚测试与最终证据门槛、打包 |
| `docs/architecture/SPATIAL_COORDINATE_CONTRACT.md`、`docs/v3.6/*` | 坐标扩展规则、研究、计划和本记录 |

## 3. 运行及开发环境

- Windows 10 22H2 x64，build 19045；AMD Ryzen 7 5800X，8 核 16 线程；32 GB 内存。
- AMD Radeon RX 6900 XT，驱动 32.0.21045.20006，D3D11。
- VS2022，MSVC 14.44.35207，C++17，`/utf-8`；本地固定 Qt6 开发依赖。构建缓存记录编译器和路径。
- 唯一兼容性验收目标：隔离便携 OBS 33.0.1；运行时 Qt 6.11.1。沿用现有 OBS32 SDK 头构建，通过真实 OBS33 加载验收；未重新执行 OBS32 验收。
- 安装载荷位于 `C:\ProgramData\obs-studio\plugins\obs-spatial-fx`，经安装器启动隔离 OBS，已核对实际加载 DLL 路径与 SHA256。

## 4. 实际命令

以下命令在项目根目录执行；`$py` 为本机 Python 运行时。测试日志存放于 `work/v3.6/`，交付证据 ZIP 保留对应目录结构。

```powershell
$env:OBS_ROOT=Join-Path $PWD 'runtime/obs33'
tools/build.cmd 4
tools/test.cmd
tools/test-installer-v36.ps1
tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v36 -Output work/v3.6/verified-ui -Wait
tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v36-persist -Output work/v3.6/verified-restart -Wait
tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.6/verified-bridge
$py='C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
& $py tests/v36_render_probe.py work/v3.6/verified-gpu work/v3.6/verified-bridge
& $py tests/v36_template_probe.py work/v3.6/verified-templates work/v3.6/verified-bridge work/v3.6/verified-ui
& $py tests/v34_render_probe.py work/v3.6/verified-fx work/v3.6/verified-bridge
& $py tests/v34_coordinate_probe.py work/v3.6/verified-fx work/v3.6/verified-bridge
git -c core.whitespace=blank-at-eol,blank-at-eof,space-before-tab,cr-at-eol diff --check
& $py tools/package-v36.py
```

安装器专项的完整参数和实际命令以 `installer-final.log` 及脚本为准。运行中拦截实际执行 `Start-Process build/OBS_Spatial_FX_V3.6.0_Setup.exe`，参数 `/SILENT /OBSROOT="<隔离OBS目录>" /LOG="<证据日志>"`；退出码 10，安装前后 DLL 哈希一致。

## 5. 最终载荷验收

| 验收项 | 状态 | 数量与证据 |
|---|---|---|
| 最终真实编译、链接 DLL/Setup | PASS | `build-final.log` |
| CTest | PASS | 12 组；`test-final.log` |
| 原生面板、编辑、预览/撤销、工程往返 | PASS | 560 项；`verified-ui/dock-smoke.json` |
| 保存及重启恢复 | PASS | 14 项；`verified-restart/dock-smoke.json` |
| 原生几何与遮罩实际像素 | PASS | 24 项；`verified-gpu/geometry-tests.json` |
| 八模板有限/循环输出、定位及五组性能 | PASS | 58 项；`verified-templates/template-tests.json` |
| 原有 FX 管线回归 | PASS：95 项效果、202 项检查 | `verified-fx/render-tests.json` |
| 原有空间坐标 GPU 回归 | PASS：8 项检查 | `verified-fx/coordinates/coordinate-tests.json` |
| v3.5 覆盖升级、回滚、卸载重装、配置保护 | PASS | 18 项；`installer-final/installer-tests.json` |
| OBS 运行中安装拦截 | PASS | ExitCode=10、HashUnchanged=true；`verified-running-guard.json` |
| 最终载荷正常卸载模块与清理 | PASS | `verified-bridge/obs-runtime.log`、`environment-restored.json` |

UI 检查直接操作原生 Qt 控件，并读取实际模型；图像探针通过已安装 OBS 的输出采样，不以“shader 能编译”替代画面检查。工程包测试执行实际 ZIP 导出、解压和独立场景恢复。物理第二台机器迁移未执行。

最终运行截图：`verified-ui/obs-main.png`、`verified-ui/preset-*.png`、`verified-ui/timeline-geometry.png`、`verified-ui/complete-package-restored.png`；渲染接触表：`verified-templates/template-contact-sheet.png`。均为实际测试记录，使用自行绘制的测试素材。

## 6. 性能记录

| 场景 | 时长/秒 | 平均FPS | 平均CPU/% | OBS平均帧耗时/ms | 新增渲染跳帧 | 内存增量/MiB |
|---|---:|---:|---:|---:|---:|---:|
| 关闭预设 | 30.03 | 60.000 | 0.299 | 0.354 | 0 | 0.0000 |
| 有限运动 | 30.03 | 60.000 | 0.338 | 0.370 | 0 | -0.0156 |
| 纯装饰 | 30.03 | 60.000 | 0.368 | 0.431 | 0 | 0.0039 |
| 单目标遮罩 | 30.03 | 60.000 | 0.322 | 0.425 | 0 | 0.0000 |
| 三层遮罩组合 | 30.03 | 60.000 | 0.355 | 0.460 | 0 | 0.0000 |

三份图像图层，同一隔离 OBS，1920×1080/60fps，各组预热后连续采样至少 30 秒，采样阶段不抓图。CPU 为 OBS 进程采样均值；帧耗时为 OBS `averageFrameRenderTime`，不是硬件 GPU timestamp。记录资源分配计数、渲染跳帧增量和内存变化。未启动编码推流，不能据此推导长时间推流、核显设备或其他机器的资源与能耗表现。

## 7. 已修复问题与失败证据

- 早期 QA 驱动在模板应用被拒绝后访问空图形集合导致测试 OBS 异常退出；已增加角色选择与失败保护。原始 `ui-first/obs-crash.txt`、运行日志保留，不将该次记为 PASS。
- 面板禁用 B 角色仍参与校验；超大时间值滑条整数转换溢出；色块模板无符号算术使左块位置异常；已分别修复并保留 RED/GREEN 日志。
- 画布比例变化的布局基点与图形尺寸补偿；圆窗装饰/裁切共用时序及孔径；局部遮罩多接收者派生锚定；各变体区分；均完成专项检查。
- 临时预览被编辑器定时回写、预览被工程导出、旧整套预设后选中对象失效、空 Delete 插入撤销记录、删除确认期间切文档等边界均已修正。
- 整次撤销改用完整文档事务，恢复灯光、FX、图形和模板状态；最终 UI 回归覆盖。
- 人工查看输出后修正横条/竖条快门方向。修正前结果留在 `pre-shutter-validation/`；交付门槛只读取修正后的 `verified-*`，最终 DLL、shader 和安装器哈希写入 `DELIVERY_MANIFEST.json`。

## 8. 边界与未执行项

- 非均匀父缩放叠加倾斜子旋转可能产生剪切，现有 Plane 表示不保证精确表达；烘焙保证采样时刻一致，采样间由曲线插值。
- 平面中心深度排序用于 2.5D 构图；相交斜平面不作为逐像素三维遮挡能力。遮罩先裁切后景深，边界可随景深柔化。
- 本版转场针对舞台内绑定图层；不接管 OBS 全局场景转场，不自动抠人或重建背景。
- NOT RUN：Windows11、NVIDIA、核显、物理第二台机器、长期推流、实际编码负载、硬件 GPU timestamp、功耗测量。
- PASS：最终隔离 OBS 正常退出，运行日志记录 0 memory leaks；153 个测试前配置文件逐一哈希及文件总数恢复一致，QA 驱动已移出运行目录，4457 测试监听已关闭。诊断现场保留在 work/v3.6/post-run-config。

## 9. 本地交付

`dist/v3.6/` 提供安装器、源码 ZIP、调试符号 ZIP、真实证据 ZIP、本报告、交付清单和 SHA256。源码快照含工作区本次变更；未发布版本标签。保留 `dist/v3.5` 原交付物；用户未跟踪的 v3.4.1 HTML 手册保留本地且排除出本版源码包。
