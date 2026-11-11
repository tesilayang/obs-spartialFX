# v3.4 修复验证报告（2026-11-11）

版本仍为 3.4.0。此报告与 `V34_DEBUG_IMPLEMENTATION_MAP.json` 是本次修复的新增记录；原说明书、原实现映射、原始用户选择 JSON 和 v3.3 安装包保留。

## 当前交付门禁

全部打包门禁通过；本报告本身不代表 dist 已替换。

最终插件 SHA256：`b0d9cebee34cdd499480eac0e874eea5f9f34e2669df96bfe1cfad597477de40`。`tools/package_v34_debug.py` 默认只检查门禁并刷新报告/映射/审计；只有显式 `--package` 且所有门禁通过后，才在 work 暂存并替换 dist 中同名 v3.4 安装包、源码包、符号包、证据包、发布清单和校验和。原 v3.4 安装包及三个压缩包保存在本次 work 的 baseline 目录，并按原发布清单校验。

| 验证目录 | 实际结果 |
| --- | --- |
| installed-ui32-final | 62 项，失败 0 |
| installed-persist32 | 13 项，失败 0 |
| installed-ui33-final | 62 项，失败 0 |
| installed-persist33 | 13 项，失败 0 |
| patch32 | 36 项，失败 0 |
| patch33-final2 | 36 项，失败 0 |
| gpu32-final | 196 项，失败 0 |
| gpu32-final/coordinates | 8 项，失败 0 |
| gpu33-final | 196 项，失败 0 |
| gpu33-final/coordinates | 8 项，失败 0 |

完整门禁结果见 `work/v3.4-debug-20261111/package-checks.json`。构建证据为 `build-final.log` 与仅重建 QA driver 的 `build-qa-final.log`；`ctest-final.log` 记录 8/8 通过。安装测试需 18/18；运行中保护需退出码 10 且原 DLL 哈希不变。UI 主测试每版本 62 项，重启持久化每版本 13 项；针对性像素/舞台回归每版本 36 项；最终广泛 GPU 回归每版本 92 个效果、237 个默认/模式变体、196 项检查；坐标回归每版本 8 项。

每个最终 GPU 目录还必须有至少 30 个逐秒样本：预热后分配数不增加、无 shader 失败、每个样本 >59fps、无新增 renderSkippedFrames。较早 `gpu33` 运行使用中间构建，仅留作诊断；交付门禁要求 `gpu33-final` 最终安装版本重新运行。

## 行为修改与边界

- 模糊：高斯采用中心加权的固定 7×7 核；方框采用同范围均匀权重；镜头散景采用等面积光圈采样，支持圆形和多边形。镜像是边界延伸规则，不是独立模糊核；画面内部相同核应相同。固定采样预算避免半径增大时无界增长，最大半径仍是近似。
- 受光：没有启用的空间灯时，只为已启用材质 FX 提供随图层位置/朝向/范围变化的局部暖色效果灯；不改模型中的舞台灯状态。存在明确启用的空间灯时沿用其绑定、颜色、强度和位置，包括用户指定的零强度。环境辅助源是可选输入；背光增加可见内侧轮廓，镜面反射采用有限介电 Fresnel。外侧阴影受素材矩形裁切，需源图内留透明边；新建阴影默认内侧，旧外侧模式仍按原键值读取。
- 移除 FX11.1 分层镜头视差，保留兄弟 ID。新增 FX12.9 渐变叠加，紧邻混合模式之前；纯色、线性和径向模式，两端颜色、中心、范围、柔度、反向复选框、线性角度和径向椭圆比例。模式无关参数隐藏，数值步进 0.01；只改变颜色、保留素材 Alpha。
- LED：改为按真实素材单元采样，再以 RGB 灯珠或圆形灯珠重建画面，带暗缝与发光。使用当前素材的颜色与画面结构。
- 宽银幕：FX13.1 在完整舞台合成后统一处理固定居中的 21:9。黑边模式外围不透明；裁切模式外围透明。它不修改 OBS 全局分辨率。多个图层同时启用时，以时间轴中第一个实际有效项决定最终舞台配置；禁用后恢复。

## 失败、修正与重试记录

首次真实运行画面全零，原因是新舞台 shader 常量未初始化；已补齐初始化并在后续构建/实际像素测试重新验证。UI32 首次测试把 Qt deleteLater 留下的隐藏旧 QObject 当作活动控件；修正的是 QA 查找活动控件的断言，没有为测试改变产品行为。第一次 GPU32 全目录运行在性能采样完成前被主代理误关 OBS，该次不能计作通过；只接受 `gpu32-final` 完整重试证据。早期不完整/失败日志保留供追溯。

## 可复现命令

```powershell
cmd /c tools\build.cmd 4
cmd /c tools\test.cmd
powershell -File tools/run_v33_installed_qa.ps1 -Major 32 -Scenario v34 -Output work/v3.4-debug-20261111/installed-ui32-final -Wait
powershell -File tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34 -Output work/v3.4-debug-20261111/installed-ui33-final -Wait
powershell -File tools/test-installer-v34-debug.ps1
python tools/package_v34_debug.py --check
# 主任务最终确认完整结果后才执行：
python tools/package_v34_debug.py --package
```

重启持久化使用 `-Scenario v34-persist`，输出到相应 `installed-persist32/33`。GPU 与像素脚本依赖已通过安装启动器打开的隔离 OBS 和临时 websocket；具体本次启动/运行输出见 work 下相应 `.log`。包装脚本不启动、不关闭 OBS，不替代真实运行测试。

## 修改文件

以下由源码与本次 baseline 逐文件 SHA256 比较生成（新增报告/映射另列）：

- `CMakeLists.txt`
- `data/effects/fx01.body`
- `data/effects/fx05.body`
- `data/effects/fx12.body`
- `data/effects/fx13.body`
- `data/effects/spatial.effect`
- `src/fx_catalog.cpp`
- `src/fx_catalog.hpp`
- `src/fx_catalog_a.inc`
- `src/fx_catalog_c.inc`
- `src/fx_geometry.inc`
- `src/fx_lighting.hpp`
- `src/fx_panel.cpp`
- `src/fx_renderer.cpp`
- `src/fx_stage.hpp`
- `src/stage.cpp`
- `tests/fx_stage_tests.cpp`
- `tests/fx_tests.cpp`
- `tests/v34_debug_probe.py`
- `tests/v34_qa.inc`
- `tests/v34_render_probe.py`
- `tools/package_v34_debug.py`
- `tools/test-installer-v34-debug.ps1`

新增交付记录：`docs/v3.4/V34_DEBUG_REPORT.md`、`docs/v3.4/V34_DEBUG_IMPLEMENTATION_MAP.json`。源码压缩包逐字节校验，符号包包含最终插件与安装器 PDB，证据包包含相关日志、截图、公开检查结果和审计。QA 场景 JSON、内嵌 stageSettings、websocket 备份、control/native command/answer、baseline 和已安装备份不进入公开证据包。

## 已知验证范围

- NOT RUN: NVIDIA hardware verification (no NVIDIA device available).
- NOT RUN: OBS34+ and future hardware/runtime combinations (no runtime available).
- NOT RUN: hours-long or all-92-simultaneous stress test; completed performance coverage is a 30-second representative FX chain per OBS version.
- Pixel effects remain bounded by the source rectangle; outer shadows need transparent source margins.
- Large fixed-budget blur radii approximate a dense convolution and may undersample tiny details.

原用户选择 SHA256：`909de115de4fcdfa4ae5280fadfc7e6febd3e8a3bb31f06185df7cc252210e11`。保留 v3.3 安装包 SHA256：`1e704adb6fd4967b6674d3c4cf33deed65b03813c2a3dc27366141e54ac94f72`。原说明书逐项哈希以 `work/v3.3-ui-20261110/manual-hashes.json` 为准。
