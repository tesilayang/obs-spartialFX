# OBS Spatial FX v3.6.1 开发与交付记录

日期：2026-11-13。在 `codex/v3.6-presets` 工作区继续 v3.6.0；Git 基线为 `055034ce9380c80cf19460bd127aafea9f4551d3`。源码交付包含本次工作区改动。未更新使用说明书，未提交、推送或发布 GitHub。

## 功能改动

- 预设首页改为：勾选现有时间轴素材、选择八种风格卡片、调整动感/装饰量/时长/循环、生成一套、换一套、应用或取消。主操作固定底部，详细控制和内容独立滚动。
- 详细控制默认折叠，手动选择运动、露出遮罩和程序装饰，和随机模式共享同一组合器。支持只换运动、只换装饰、只换配色，固定运动、装饰及配色。
- 删除“我的整套预设”界面；历史读取能力和用户原有预设文件保留。
- 灯光、阴影、预设配色及 FX 统一为“整条底色预览与十六进制值＋选择颜色”。支持实时画面预览、取消恢复；灯光颜色确认可整次撤销。开发标准已加入强制要求。
- 默认保留基础构图和深度，摄像机和露出遮罩默认关闭。选中实例直接参与，无需重新导入或分配角色。重复生成替换临时预览，一次应用对应一次撤销。
- 保存完整解析图形、动作样点、零件ID和独立随机种子；重启和导入不重新抽签。同源多实例使用稳定身份。

## 本次实现范围

| 类别 | 数量 | ID |
|---|---:|---|
| 素材运动 | 18 | M02/M04/M08/M17/M18/M20/M21/M25/M26/M27/M28/M29/M31/M33/M34/M35/M39/M44 |
| 露出遮罩 | 22 | T01/T02/T03/T08/T09/T10/T11/T12/T13/T14/T16/T17/T18/T19/T24/T25/T26/T27/T28/T33/T35/T57 |
| 程序装饰 | 20 | D01/D03/D04/D05/D06/D07/D09/D10/D13/D14/D15/D17/D19/D20/D21/D23/D25/D26/D30/D31 |

140项研究目录保留为候选池。重采样图像转场、景深接力、曲面卷页、物理碎片、液态融合和彩雾留作后续评估。当前遮罩裁切指定实时图层；双主体交班控制两个已有实例的透明度。

## 修改文件

| 文件 | 内容 |
|---|---|
| `src/presets/composer.hpp/.cpp` | 八风格组合库、60零件、独立随机流、原子生成和完整配方 |
| `src/presets/types.hpp`、`presets.cpp`、`graphics.cpp` | 样点运动、解析遮罩、真实五角星、持久化及缓存渲染 |
| `src/presets/panel.hpp/.cpp` | 初学者卡片界面、详细控制、固定底部操作、临时预览和局部重抽 |
| `src/dock.hpp/.cpp`、`dock_v2.cpp`、`dock_actions.cpp`、`widgets.hpp/.cpp` | 删除旧整套入口、颜色预览条、灯光颜色事务与取消恢复 |
| `CMakeLists.txt`、`src/plugin.cpp`、`installer/setup.cpp`、`setup.rc.in` | 3.6.1版本、编译和安装器 |
| `tests/composer_tests.cpp`、`preset_panel_tests.cpp` | 数学、格式、种子、事务、坐标及窄视窗回归 |
| `tests/v361_*.py`、`v361_ui_bridge.inc`、`obs_qa_driver.cpp` | 原生控件、GPU画面、保存重启和性能验证；QA不进入安装包 |
| `tools/test-installer-v361.ps1`、`package-v361.py` | 安装器验证、证据门槛、源码和交付物校验 |
| `DEVELOPMENT_STANDARDS.md`、坐标契约、实施计划及本记录 | 固定颜色规范、统一局部/世界坐标规则与验收记录 |

## 研发中发现并修复

- 有限重复遮罩第二轮不再保持全遮挡。
- D05同向斜带和D06交叉双带使用不同布局、数量与时序。
- T02柔边作用于移动前沿，满开不再永久淡化素材外框。
- 循环弹簧的存储样点闭合位置及接缝速度；有限弹簧保留入场响应。
- 配方颜色和空间信息改为对象，避免 OBS 标量数组规范化丢失。
- 显式零件ID重放固定消耗选择随机值，保持解析输出一致。
- 局部遮罩使用独立 `localMaskPlane`，禁止继承世界前方偏移；无世界图形时不检查无关深度引用。
- 取消灯光/阴影选色恢复权威基线，避免临时颜色残留。
- 有限预览结束后“换一套”重新开始播放；独立预览标记同时避开前次预览与当前发布值，取消后立即重新生成及53位序列边界均可重启。原始失败证据保留为 `preview-clock-red.json`。
- 原始和失败日志保留；审查记录见证据包的 core/panel review 报告。
- 验收脚本曾以 `locked` 构造图层锁定状态，当前格式实际字段为 `animation_locked`。该次运行保留为 `ui-lock-fixture-failure/native-tests.json`（FAIL），修正输入后从全新 OBS 进程重新执行；不把旧失败改写为通过。

## 实际环境和命令

Windows 10 22H2 x64 build19045；AMD Ryzen7 5800X；32GB内存；AMD Radeon RX6900XT，驱动32.0.21045.20006；OBS33.0.1/D3D11。C++17、VS2022/MSVC14.44、Qt6、UTF-8和 `/utf-8`。硬件原始信息见 `hardware.json`。

```powershell
$env:OBS_ROOT=Join-Path $PWD 'runtime/obs33'
$env:PATH=(Join-Path $PWD 'third_party/cmake/cmake-4.4.5-windows-x86_64/bin')+';'+$env:PATH
cmake --build build --target clean
tools/build.cmd 4
tools/test.cmd
tools/test-installer-v361.ps1
tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.6.1/verified-ui
$py='C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
& $py tests/v361_probe.py work/v3.6.1/verified-ui work/v3.6.1/verified-ui
# 每轮通过后，经 QA finish 请求正常保存退出，并保存退出码与 OBS 日志。
tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.6.1/verified-restart
& $py tests/v361_restart_probe.py work/v3.6.1/verified-restart work/v3.6.1/verified-restart work/v3.6.1/verified-ui
tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.6.1/verified-bridge
& $py tests/v36_render_probe.py work/v3.6.1/verified-gpu work/v3.6.1/verified-bridge
& $py tests/v361_analytic_probe.py work/v3.6.1/verified-analytic work/v3.6.1/verified-bridge
& $py tests/v34_render_probe.py work/v3.6.1/verified-fx work/v3.6.1/verified-bridge
& $py tests/v34_coordinate_probe.py work/v3.6.1/verified-fx work/v3.6.1/verified-bridge
& work/v3.6.1/restore-environment.ps1
& $py tools/package-v361.py
```

以上编译与测试命令已实际执行。三个最终 OBS 进程均正常保存退出，退出码为0。验收来自 ProgramData 内安装后的载荷，已核对实际加载 DLL 与全部着色器哈希。

## 最终验收

| 检查 | 状态 | 数量 / 证据 |
|---|---|---|
| 清理构建、插件及安装器真实链接 | PASS | `build-final.log`，退出码0 |
| CTest | PASS | 13/13，`test-final.log` |
| 原生控件、八种风格、60零件、实际像素、撤销、颜色及 JSON/ZIP 往返 | PASS | 321/321，`verified-ui/native-tests.json` |
| 有限预览结束后换一套、取消后重生成及最大序列值 | PASS | `verified-ui/preview-clock-green.json`；原始 RED 单独保留 |
| 正常重启后完整图形、动作、种子与素材关联 | PASS | 11/11，`verified-restart/restart-tests.json` |
| 程序图形、空间深度、绑定及资源缓存 | PASS | 24/24，`verified-gpu/geometry-tests.json` |
| 22种遮罩、满开边缘、两两画面差异 | PASS | 341/341，`verified-analytic/analytic-tests.json` |
| 已有95个 FX 的 GPU 回归 | PASS | 202/202，`verified-fx/render-tests.json` |
| 已有坐标 GPU 对照 | PASS | 8/8，`verified-fx/coordinates/coordinate-tests.json` |
| v3.6.0覆盖升级、锁文件回滚、配置保留、卸载及重装 | PASS | 18/18，`installer-final/installer-tests.json` |
| OBS运行中禁止覆盖 | PASS | 退出码10且DLL哈希不变，`verified-running-guard.json` |
| 环境恢复 | PASS | 153个配置文件字节核对，OBS退出、QA移除、无4457监听，`environment-restored.json` |

截图和原始结果位于交付证据包：`verified-ui/preset-simple.png` 为简化入口，`light-color-dialog.png` 为统一颜色条，`library-contact-sheet.jpg` 及每个零件的 `*-sequence.jpg` 为真实 OBS 画面。每个最终 OBS 目录包含 `obs-runtime.log` 和 `installed-path.json`。中间失败记录保持原状态，最终结论只引用 `verified-*` 目录。

## 性能实测

实际 OBS33.0.1，1920×1080、60fps、三个640×360测试图层。CPU为OBS进程平均占用；渲染时间来自OBS统计。每项单独测量约30.1秒，不同时启用全部零件。

| 场景 | 平均FPS | CPU % | 帧渲染 ms | 新增渲染丢帧 | 渲染计数增量 | 缓存分配 前→后 | 进程内存变化 MiB |
|---|---:|---:|---:|---:|---:|---|---:|
| 预设关闭 | 60.00 | 0.345 | 0.462 | 0 | 1806 | 4→4 | 0.00 |
| 运动 M17 | 60.00 | 0.302 | 0.456 | 0 | 1806 | 4→4 | +0.09 |
| 装饰 D19 | 60.00 | 0.302 | 0.478 | 0 | 1806 | 4→4 | −52.24 |
| 遮罩 T35 | 60.00 | 0.312 | 0.499 | 0 | 1806 | 4→4 | −75.46 |
| M17＋T09＋D01 | 60.00 | 0.367 | 0.610 | 0 | 1808 | 4→4 | −54.11 |

原始每秒样本、实际时长及前后资源计数保存在 `verified-ui/native-tests.json` 的 `workloads`。五项图形失败计数均为0。内存值为整个OBS进程的变化，包含此前测试资源回收。已有模糊/调色/光效链另执行30秒缓存与渲染存活回归，见 `verified-fx/performance.json`。

## 载荷标识与交付

- 插件 DLL SHA256：`8C86ED678865CCF4BA4B818D43E4DF84DBDB4B19DCD84601F0917D21F86CF8AF`。
- 安装包 SHA256：`57BFE4A795AD2102D5E9CBF00130896B843C96A9C547A749C4D6563FEB84F765`。
- 安装器文件版本与产品版本均为3.6.1，可覆盖升级；未打包Qt、OBS运行库及QA驱动。
- 安装包、源码、PDB符号、证据、交付清单及SHA256统一保存于 `dist/v3.6.1/`。源码包保留工作区版本，manifest明确记录未提交状态与Git基线。
- 不更新使用说明书，不提交、推送或发布GitHub；原有正式交付物和个人说明书保持原样。

## 验证边界

Windows11、NVIDIA、集成显卡、真实第二台机器转移、长时间推流及物理功耗测量均为 NOT RUN。性能仅代表上述开发设备与明确记录的场景，不能推断其他设备相同消耗。新增实现无第三方运行时依赖；数学和解析图形为原生编写，上游研究记录保留来源与许可证。
