# OBS Spatial FX v3.4.1 开发与实测报告

日期：2026-11-11。此文件是开发交付记录；用户说明书未更新。

## 交付内容

- 在“FX 特效 → 画质修正”下增加独立的“抗锯齿”和“清晰化”折叠子板块，保留原有锐化、降噪入口。
- SMAA 1x 提供四步省资源、八步标准搜索；FXAA 采用单遍边缘搜索及细线柔化。两者二选一，切换保留参数。
- 清晰化采用 AMD RCAS，固定五点采样、单遍处理，默认程度0.25，可开关噪点保护。与抗锯齿分开启用。
- 新功能默认关闭，滑条精度0.01，颜色/透明轮廓分别判断，关闭后不执行该效果的绘制。
- AMD Native AA 按用户确认延期，本版隐藏；单独研发范围见 NATIVE_AA_FUTURE.md。
- 保留全部旧效果ID及参数键；把读取上限从旧92项改为完整95项目录，防止末尾效果保存后被截断。
- 安装器更新为3.4.1，支持覆盖3.4，保留场景/预设与控制面板卸载功能。

## 实际修改文件

界面和目录：`src/fx_panel.cpp`、`src/fx_catalog.cpp`、`src/fx_catalog_a.inc`、`src/dock.cpp`。

渲染和持久化：`src/fx_aa.hpp/.cpp`、`src/fx_renderer.cpp`、`src/fx_model.cpp`、`data/effects/fx_smaa.body`、`data/effects/fx02.body`。

来源和授权：`src/third_party/smaa_area.hpp`、`src/third_party/smaa_search.hpp`、`data/effects/AA_NOTICES.txt`；原始参考的URL/大小/SHA256在`work/v3.4.1/references/sources.json`。

版本和安装：`CMakeLists.txt`、`src/plugin.cpp`、`installer/setup.cpp`、`installer/setup.rc.in`。

新增测试：`tests/v341_qa.inc`、`tests/v341_quality_probe.py`、`tests/v341_encode_probe.py`；扩展`tests/fx_tests.cpp`、`tests/obs_qa_driver.cpp`、`tests/v34_qa.inc`；颜色回归脚本增加Windows文件共享竞争的有界重试。

交付工具：`tools/test-installer-v341.ps1`、`tools/run_v341_regressions.ps1`、`tools/report_v341.py`、`tools/package_v341.py`。

## 真实编译与执行命令

工作目录：`E:/obs spartialFX develop`。

```powershell
& tools/build.cmd 4
& tools/test.cmd
& tools/test-installer-v341.ps1
& tools/run_v33_installed_qa.ps1 -Major 32 -Scenario v34-hold -Output work/v3.4.1/release/bridge32final
& tools/run_v341_regressions.ps1 -Major 32 -StartAt colors
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.4.1/release/bridge33final
& tools/run_v341_regressions.ps1 -Major 33
```

OBS32的quality测试已由同一runner的首次完整调用执行，通过后因测试通信竞争从colors恢复；14项最终退出码全部保存于`release/exits32.json`。该竞争的原始失败日志保留在`diagnostics/colors32-race.log`，不是插件着色器失败。

编译日志：`work/v3.4.1/logs/build-3.log`（重新链接DLL与安装器）、`build-4.log`（更新最终着色器安装资源）；最终单元测试：`test-final.log`。测试先失败记录：`red-tests.log`；最初新增效果未注册，随后实现并验证通过。

## 实测环境

Windows10 22H2 x64（build19045），AMD Ryzen7 5800X，Radeon RX6900XT，驱动32.0.21045.20006。OBS32.2.2与OBS33.0.1分别从隔离的便携测试目录启动，实际加载ProgramData中的最终DLL；每次启动的DLL和全部19个资源校验见`installed-path.json`。VS2022、C++17、MSVC `/utf-8`、Qt6、D3D11。

## 测试结果

| 测试 | OBS32.2.2 | OBS33.0.1 |
|---|---|---|
| 原有界面回归 | PASS · 79 项 | PASS · 79 项 |
| 原有配置重启恢复 | PASS · 18 项 | PASS · 18 项 |
| 新抗锯齿/清晰化界面 | PASS · 35 项 | PASS · 35 项 |
| 新参数重启恢复 | PASS · 16 项 | PASS · 16 项 |
| 真实像素/遍数/尺寸变化/资源复用 | PASS · 66 项 | PASS · 66 项 |
| 全部95效果与244模式变体 | PASS · 202 项 | PASS · 202 项 |
| 颜色实时预览/取消/确认 | PASS · 13 项 | PASS · 13 项 |
| 模糊算法差异 | PASS · 6 项 | PASS · 6 项 |
| 模糊质量 | PASS · 14 项 | PASS · 14 项 |
| 模糊透明边缘 | PASS · 10 项 | PASS · 10 项 |
| 灯光控制 | PASS · 17 项 | PASS · 17 项 |
| 既有功能修复回归 | PASS · 36 项 | PASS · 36 项 |
| 统一空间坐标 | PASS · 8 项 | PASS · 8 项 |
| 真实x264编码 | PASS · 7 项 | PASS · 7 项 |

C++自动测试：8/8 PASS。安装器：18/18 PASS（3.4→3.4.1、文件锁回滚逐字节一致、配置保留、卸载、非自有文件保留、重新安装、不带Qt DLL或QA驱动）。运行中OBS安装保护：退出码10、DLL哈希未变。

图像检查包括：已关闭效果原图逐像素一致；SMAA/FXAA斜线误差对比8×子像素覆盖参考；两种算法输出不同；常色区域不变；RCAS零程度恒等及alpha保持；透明轮廓不出现隐藏RGB色边；暗灰/红/蓝斜线；SMAA/FXAA互斥；两个实例独立；源尺寸变化后资源重新稳定。

## 1080p/60fps性能记录

下表是OBS GetStats的整帧平均渲染时间均值，单位ms，每项采样8秒。它包含OBS开销，不是单独GPU时间戳；小差值会受计时噪声影响，不能据此宣称某算法普遍更快。

| 场景 | OBS32 | OBS33 |
|---|---:|---:|
| 仅原图 | 0.2136 | 0.1886 |
| FXAA | 0.2324 | 0.2161 |
| SMAA省资源 | 0.2304 | 0.2356 |
| RCAS | 0.2184 | 0.2207 |
| FXAA＋RCAS | 0.2245 | 0.2308 |

两版各完成30秒代表性FX链稳定运行及30秒采样区间的1080p60、x264 veryfast、6000kbps本地编码（含图层旋转动画、SMAA＋RCAS）。采样区间未新增渲染丢帧或编码丢帧，FPS保持59以上，预热后纹理分配数不增长。启动和切换重负载效果时已有的累计丢帧数未被当作0报告；前后数值原样保存在证据中。

SMAA实测每帧3遍，FXAA每帧1遍，RCAS每帧1遍；SMAA＋RCAS共4遍。关闭效果后绘制遍数不再增加；关闭组或强度0时不激活。SMAA专用图像约19.8MiB/1080p图层，另有共享小型LUT；资源紧张时优先尝试单遍FXAA。RCAS默认0.25，不包含分辨率放大、历史帧或CPU逐像素处理。

RCAS选择理由与CAS差异见ALGORITHM_PROVENANCE.md。默认CAS也能编译为五点采样，因此不宣称RCAS一定比CAS快；本版使用RCAS的稳健裁剪限制与噪点保护。

## 真实截图和日志

- `work/v3.4.1/release/ui34132/quality-controls.png`、`ui34133/quality-controls.png`：真实OBS Dock。
- `release/quality32/`、`quality33/`：原图、SMAA、FXAA、RCAS、透明及暗色边缘截图；`quality-tests.json`为最终66项结果。
- `release/gpu32/render-tests.json`、`gpu33/render-tests.json`：202项/95效果/244模式结果。
- `release/bridge32final/obs-runtime.log`、`bridge33final/obs-runtime.log`：真实着色器编译、x264参数、运行及正常退出。
- `release/encode32/encode-tests.json`、`encode33/encode-tests.json`：真实编码文件、帧数及采样。编码视频仅为本地合成测试素材，保存在work中，不装入插件。
- `release/ui*/installed-path.json`：实际加载路径、DLL/资源SHA256；`logs/`记录所有测试输出。
- `diagnostics/`与早期build/test日志为迭代诊断；最终验收只使用上述规范路径，包装工具拒绝任何未通过的最终结果。

## 未执行与边界

| 项目 | 状态 | 原因 |
|---|---|---|
| 无独显/集显实机性能与CAS对照 | NOT RUN | 本机只有RX6900XT，不能把独显成绩当作集显成绩 |
| NVIDIA显卡实机 | NOT RUN | 无对应硬件 |
| 互联网实际推流 | NOT RUN | 使用真实本地x264编码验证，没有向平台发送直播 |
| 数小时持续运行 | NOT RUN | 本轮为短时功能、稳定及编码验收 |
| AMD Native AA | DEFERRED | 用户确认留待独立研发，本版不显示 |

本版为空间图层当前纹理做抗锯齿，无法恢复源视频缺失细节，也不消除后续舞台透视采样新产生的锯齿。未承诺所有集显、驱动或后续OBS版本均能达到同样性能。

## 最终文件与校验

插件DLL SHA256：`0c638fe93b1fd54a9d56e7cf9d014486b9cae67b3b2d42f326605d6fd8d13836`。

安装器SHA256：`0552e62dbcb6c1ac8041cfcc7b12c619ded3e90d994b8cf6870bb9c3c37eece3`。

交付目录`dist/`：`OBS_Spatial_FX_V3.4.1_Setup.exe`及通用名`OBS_Spatial_FX_Setup.exe`；配套`OBS_Spatial_FX_V3.4.1_Source.zip`、`OBS_Spatial_FX_V3.4.1_Symbols.zip`、`OBS_Spatial_FX_V3.4.1_Evidence.zip`。完整哈希及验收清单见`V341_RELEASE.json`、`SHA256SUMS.txt`和`work/v3.4.1/final-package-audit.json`。历史3.4安装器/资料及说明书保持原字节。
