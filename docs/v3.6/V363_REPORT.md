# OBS Spatial FX v3.6.3 交付报告

日期：2026-11-14。状态：PASS，编译、专项实机验收和交付包校验通过。

## 本次变更

- 八种风格改为主体色块、辅助图形和有限纹理组合；丰富模式增加图形数量、面积和层次。
- 彩带加宽，圆环加粗，增加实心圆与椭圆；网点和棋盘限制在局部区域。
- 增加协调配色组合并更新风格卡片动画；保留手工颜色、独立随机种子及锁定。
- 底部增加“补充至时间轴”。使用正在预览的同一候选，在现有内容尾部的下一实际 OBS 视频帧开始；空动画从零开始。
- 保留已有动画和全场循环设置，整套补充支持一次撤销。冲突候选自动转为尾部预览，显示补充起点。
- 预览基线变更后禁止覆盖新编辑。装饰风格独立保存，固定装饰后切换运动风格保持原图形。
- 补充若会重新启用含旧关键帧的停用参数轨，则提示具体对象和参数并取消本次预览，保留原时间轴；确认该轨启用状态后可重新生成。

## 文件范围

| 文件 | 修改内容 |
|---|---|
| `src/presets/composer.cpp`、`composer.hpp` | 分层装饰配方、实心圆、解析配方重放、风格上下文、结构化冲突状态 |
| `src/presets/panel.cpp`、`panel.hpp` | 补充按钮、候选冻结、帧对齐、尾部预览、配色、动态缩略图及编辑保护 |
| `tests/composer_tests.cpp`、`preset_panel_tests.cpp` | 组合、独立随机、配色、锁定及追加边界测试 |
| `tests/v363_probe.py`、`v363_restart_probe.py`、`obs_qa_driver.cpp` | 最终安装载荷的真实画面、原动画、撤销、性能及保存重启验证；测试桥接防重复执行 |
| `CMakeLists.txt`、`src/plugin.cpp`、`src/dock.cpp`、`installer/setup.cpp`、`setup.rc.in` | 版本号与安装载荷 |
| `tools/package-v363.py` | 验收门槛、源码字节、安装包与证据哈希校验 |
| `docs/v3.6/V363_IMPLEMENTATION.md`、本报告 | 范围、实现约定、执行记录和验收结果 |
| `README.md` | 当前本地版本号 |

完整差异基于开发开始前的工作区快照生成，位于证据包 `changed-files.json` 和 `v363-task.diff`。早于本任务的未提交修改保留。

## 测试范围与状态

本次只验证预设生成、UI、补充、保存重启和直接受影响的绘制性能。真实 OBS 运行共 161 项检查通过，新进程重启 15 项通过，测试桥接锁文件复验 5 项通过。

| 项目 | 状态 | 证据 |
|---|---|---|
| VS2022 x64 编译插件与安装器 | PASS | `build-final.log`，实际 DLL 与安装器链接 |
| presets、composer、preset-panel 专项测试 | PASS | `test-final.log`，3/3 通过 |
| 安装最终载荷与配置保留 | PASS | `verified-install/install-tests.json` |
| OBS 33.0.1 八种风格少量与丰富输出 | PASS | `verified-presets/style-coverage.json`、各时刻截图 |
| 宽彩带、厚圆环、实心色面 | PASS | `D01/D05/D06-thickness.json` 及真实输出截图 |
| 候选保持、帧对齐、取消、撤销重做及原画面保持 | PASS | `verified-presets/preset-tests.json`、`append-*.json/png` |
| 保存与正常退出后的新进程恢复 | PASS | `verified-restart/restart-tests.json`，15/15 |
| 1080p60 三组各 30 秒关联性能检查 | PASS | `verified-presets/preset-tests.json` 的 `workloads` |
| 最终源码与交付物 SHA256 校验 | PASS | `package-audit.json`、`DELIVERY_MANIFEST.json`、`SHA256SUMS.txt` |
| 无关 FX、灯光、工程导入导出及整套安装器回归 | NOT RUN | 用户限定专项测试，无本次修改 |
| OBS32、Win11、NVIDIA、集成显卡、长时间推流与物理功耗 | NOT RUN | 本机不执行这些验收 |

失败复现及中间失败日志单独保留。测试代码曾出现临时 Qt JSON 引用失效，已修复。独立复核发现的停用轨重新激活、相邻帧保护关键帧手柄改变均先复现失败，再修复并通过专项测试。

画面检查：少量与丰富使用相同种子；在 1.5 秒测量独立装饰的可见覆盖率。该比例描述实际图形面积，不是性能指标。

| 风格 | 少量／丰富图形数 | 少量／丰富覆盖率 |
|---|---:|---:|
| 温柔舒缓 | 4／7 | 22.20%／45.21% |
| 简洁几何 | 6／9 | 36.53%／50.71% |
| 活泼波普 | 5／9 | 10.50%／17.59% |
| 纸片拼贴 | 5／9 | 21.31%／34.35% |
| 科技界面 | 8／13 | 16.09%／21.50% |
| 节奏快剪 | 6／9 | 21.80%／29.95% |
| 朦胧梦幻 | 7／13 | 27.68%／53.46% |
| 故障实验 | 5／8 | 16.17%／28.24% |

真实彩带高度约 122～140 像素，测试圆环描边 48.6 像素（舞台 1920×1080）。追加前后在 1.5 秒取同一帧比较，普通补充和冲突尾部补充的 RGB/Alpha 平均差值均为 0；原关键帧、手柄、几何和循环设置保留。60fps 的测试尾部 4,012,345µs，补充起点 4,016,667µs，符合下一视频帧规则。

首轮实机验收失败保存在 `runtime-first-failure.log` 和 `verified-first-failed-*`。测试素材名称及默认状态未按 Dock 初始化规则设置，导致撤销与取消的全图比较包含初始化差异；已修正测试基线。另通过锁定响应文件 800ms 复现 QA 驱动重复执行按钮命令（`bridge-lock-red.log`）；测试驱动改为执行一次、缓存响应并重试写回，5 项锁定复测全部通过（`verified-bridge-lock/bridge-tests.json`）。产品 DLL 未因这些测试设施问题修改。

## 实际命令

工作目录：`E:\obs spartialFX develop`。PowerShell，工具由本地固定依赖提供。

```powershell
$env:OBS_ROOT=Join-Path $PWD 'runtime/obs33'
& tools/build.cmd 4
& third_party/cmake/cmake-4.4.5-windows-x86_64/bin/ctest.exe --test-dir build --output-on-failure -R '^spatial-(presets|composer|preset-panel)$'
```

已执行安装与最终载荷启动：

```powershell
& work/v3.6.3/install-final.ps1
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.6.3/verified-bridge
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tests/v363_probe.py work/v3.6.3/verified-presets work/v3.6.3/verified-bridge
```

测试驱动修复后仅重新编译 `spatial-qa-driver.dll`，日志 `build-qa-bridgefix.log`。实际执行的重启验收与恢复：

```powershell
& work/v3.6.3/finish-obs.ps1 -Bridge work/v3.6.3/verified-bridge
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.6.3/verified-restart
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tests/v363_restart_probe.py work/v3.6.3/verified-restart work/v3.6.3/verified-restart work/v3.6.3/verified-presets
& work/v3.6.3/finish-obs.ps1 -Bridge work/v3.6.3/verified-restart
& work/v3.6.3/restore-environment.ps1
git -c core.whitespace=cr-at-eol diff --check
```

完整输出保存在 `work/v3.6.3/`。实际截图包括 `styles-sparse-rich-contact-sheet.jpg`、`append-normal-preview-controls.png`、`append-final-saved-frame.png` 和 `append-final-restarted-frame.png`，均收录于证据包；画面比较使用真实 OBS 输出。

实际打包命令：

```powershell
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tools/package-v363.py
```

21 项打包门槛通过。首轮包保留在任务工作目录，补齐本报告最终状态后重新生成正式包；正式包再次执行全部门槛和逐字节归档校验。`dist/v3.6.2/` 和 `dist/v3.6.2-shadowfix/` 的已记录交付哈希保持不变。

## 开发设备与性能

Windows 10 专业版 19045；AMD Ryzen 7 5800X（8 核 16 线程）；AMD Radeon RX 6900 XT（驱动 32.0.21045.20006）；约 31.92 GiB 内存。硬件记录：`hardware.json`。

性能场景为 1920×1080、60fps、3 个合成测试图层，FX 与舞台灯光关闭。以下是 OBS 统计均值；渲染耗时是 OBS 整帧统计，不是独立 GPU 基准。

| 场景 | 时长 | 平均 FPS | 平均渲染 ms | 平均 CPU | 新增渲染丢帧 | 内存变化 MiB |
|---|---:|---:|---:|---:|---:|---:|
| 原始场景 | 30.078s | 60.000 | 0.333 | 0.638% | 0 | +0.0039 |
| 温柔舒缓·丰富 | 30.094s | 60.000 | 0.380 | 0.585% | 0 | 0 |
| 节奏快剪·丰富 | 30.094s | 60.000 | 0.375 | 0.579% | 0 | 0 |

三组各记录 30 个样本、1805～1809 次舞台渲染；预热后的图形分配计数均保持 1，无新增分配或失败。CPU 小幅差异属于本次样本，不能据此推断丰富模式降低资源占用。

这些记录仅对应此硬件和合成场景；未进行功耗测量，不作能耗结论。

## 交付与环境恢复

安装位置：`C:\ProgramData\obs-studio\plugins\obs-spatial-fx`。安装器仅更新版本和载荷，原安装逻辑沿用。

隔离 OBS 均正常退出（退出码 0）；测试驱动已移出运行目录，4457 测试监听关闭；153 个隔离配置文件逐文件恢复原 SHA256，文件数一致。证据：`environment-restored.json` 与两次 `exit-status.json`。

交付目录：`dist/v3.6.3/`，包括安装器、对应源码、符号、证据、本报告、清单与 SHA256。安装器大小 6,390,272 字节。

| 文件 | SHA256 |
|---|---|
| `OBS_Spatial_FX_V3.6.3_Setup.exe` | `2c37c40df2af003a6aef1cb35b41242785dda174834eda2347102a32183138c4` |
| 实际安装并加载的 `obs-spatial-fx.dll` | `a2ad7add3e5325779d857c15b6b3d998f630ed9c36a569680427cbea6edd126d` |

源码、证据、符号与清单的最终 SHA256 由打包脚本输出至 `SHA256SUMS.txt`。已校验源码逐字节一致、ZIP CRC、最终安装载荷、版本号、实际执行证据和历史交付物哈希。

说明书未更新；未进行 Git 提交、推送或 GitHub Release 发布。
