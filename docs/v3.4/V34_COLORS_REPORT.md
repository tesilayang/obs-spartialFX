# v3.4 FX 颜色与界面微调交付记录

日期：2026-11-11。版本保持 **3.4.0**，覆盖现有 v3.4；新增抗锯齿/清晰化只做调研，等待用户决定。已有说明书未更新。

## 本次行为

1. FX 页开头只显示一次：`"受光与阴影"特效组效果独立，子功能需要的附加光源会独立于灯光页主灯单独生效。` 已移除素材卡片中原来的三句说明。
2. 背后柔光、烛光、聚光灯各有独立“效果颜色”，由各自 FX 实例保存，着色器读取该实例颜色。烛光的“暖色比例”仍参与最终颜色，设为 0 可保留所选色相。
3. FX 所有既有颜色控件统一为“实心色块 + ARGB 色值 + 选择颜色”一行。渐变起始/结束颜色遵循相同方式。调整时实时提交到舞台；确定保留，取消或关闭恢复原颜色。当前效果颜色为 RGB，ARGB 中 Alpha 显示 FF，素材透明度仍由原有透明度参数控制。
4. 参数行、复选子功能行、恢复按钮和分组间距压紧，按约 30% 的目标收紧，并以主题字体能完整显示为下限。Qt 主题完成字体应用后才设置行高，避免 OBS 32 的较大字号被固定高度裁切。OBS32/33 实测相邻数值参数行均 23/23 像素；复选子功能行距分别 20/20 像素，随主题字号适配。

本次三项灯光颜色使用现有 tint Uniform，没有增加渲染 Pass 或额外颜色纹理。其位置、范围和强度的既有计算保持本补丁前的方式。既有 13 组 / 92 个子功能的稳定 ID 保持。

## 实际修改文件

| 文件 | 用途 |
|---|---|
| `src/fx_panel.cpp`、`src/fx_panel.hpp` | 顶部单次说明、统一色块、异步实时选色、取消恢复、主题适配和紧凑行距 |
| `src/fx_catalog_a.inc` | 为 FX05.1 / FX05.2 / FX05.3 声明颜色控件 |
| `data/effects/fx05.body` | 背光/烛光/聚光灯使用各自颜色 |
| `tests/fx_tests.cpp` | 三项独立颜色的模型保存恢复回归 |
| `tests/v34_qa.inc`、`tests/obs_qa_driver.cpp` | 逐个 FX 色控件实时提交/取消、确认保存、字号可读性与原生 UI 桥接 |
| `tests/v34_live_colors_probe.py` | 原生选色窗口打开时捕获真实 D3D11 舞台像素，验证变色、取消和确定 |
| `tools/test-installer-v34-colors.ps1`、`tools/run_v34_colors_release.ps1` | 本补丁安装/卸载和双版本运行测试入口 |
| `tools/package_v34_colors.py` | 绑定本次最终 DLL 哈希，检查新鲜运行证据后再覆盖发布 |
| `docs/v3.4/V34_COLORS_REPORT.md`、`docs/v3.4/V34_AA_CLARITY_RESEARCH.md` | 本次研发记录与待决策调研，不是说明书更新 |

与上一交付包的实际差异：`work/v3.4-colors-20261111/source-changes.diff`。旧版交付保存在 `work/v3.4-colors-20261111/baseline/dist/`。

## 真实执行命令

以下入口已在本地真实执行；构建工具使用 VS2022/MSVC、Qt6、UTF-8 `/utf-8`：

```powershell
& tools/build.cmd 4
& tools/test.cmd
& tools/test-installer-v34-colors.ps1
& tools/run_v33_installed_qa.ps1 -Major 32 -Scenario v34-hold -Output work/v3.4-colors-20261111/release/bridge32
& tools/run_v34_colors_release.ps1 -Major 32
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.4-colors-20261111/release/bridge33
& tools/run_v34_colors_release.ps1 -Major 33
```

运行脚本逐一执行 `tests/v34_live_colors_probe.py`、`v34_blur_siblings.py`、`v34_blur_quality.py`、`v34_blur_edgecases.py`、`v34_lighting_controls_probe.py`、`v34_debug_probe.py`、`v34_render_probe.py`、`v34_coordinate_probe.py`，随后执行原生界面和重启恢复场景。完整命令、退出码和结果留在本次 work 目录。

## 验证结果

设备：Windows 10 x64，AMD Ryzen 7 5800X / Radeon RX 6900 XT，D3D11。运行环境为隔离的 OBS 32.2.2 与 OBS 33.0.1，真实加载 ProgramData 中最终安装的插件；QA 驱动只放在隔离运行目录，没有打入安装包。

| 验证项 | OBS 32.2.2 | OBS 33.0.1 |
|---|---|---|
| 原生界面 | 79/79 PASS | 79/79 PASS |
| 重启后保存恢复 | 18/18 PASS | 18/18 PASS |
| 选色窗口打开时真实 GPU 预览 | 13/13 PASS | 13/13 PASS |
| 全部 92 特效 / 237 模式 | 196/196 PASS | 196/196 PASS |
| 模糊算法独立性 | 6/6 PASS | 6/6 PASS |
| 模糊画质 | 14/14 PASS | 14/14 PASS |
| 模糊边缘与资源复用 | 10/10 PASS | 10/10 PASS |
| 光照中心、尺寸及开关联动回归 | 17/17 PASS | 17/17 PASS |
| 上一补丁的功能回归 | 36/36 PASS | 36/36 PASS |
| 坐标与实例一致性 | 8/8 PASS | 8/8 PASS |

- CTest：8/8 PASS。
- 安装器：18/18 PASS，覆盖同版本升级、锁文件失败回滚、配置保留、控制面板版本、卸载、保留用户文件和最终重装。
- OBS 运行中保护：安装器退出码 10，最终插件 DLL 哈希不变。
- 两个运行版本的实际加载路径、DLL SHA256、17 个着色器资源哈希均记录在 `release/*/installed-path.json`。
- 独立光效的实时选色测试中，取消前后真实 GPU 像素差为 0；红/蓝预览及确认后的像素明显不同。

- OBS 32：30 秒，最低 60.000 FPS；测量窗口新增渲染丢帧 0，预热后纹理分配 29 → 29，GPU Shader 失败 0。
- OBS 33：30 秒，最低 60.000 FPS；测量窗口新增渲染丢帧 0，预热后纹理分配 29 → 29，GPU Shader 失败 0。

以上性能是代表性特效链的短时间测量，不是 92 个特效同时运行的性能承诺。OBS 启动和测试切换时已有的丢帧不计入稳定窗口的新增帧数。

## 复现中遇到的测试问题

- 最早的原生选色检查误取 Qt `deleteLater` 尚未销毁的隐藏旧窗口；改为操作当前可见窗口。日志保留在 `ui33-red/`、`ui33-diag/` 等诊断记录。没有把这些失败记录标为 PASS。
- OBS32 的主题在控件显示前应用较大字号和尺寸约束。新增真实字号/行距检查发现该问题后，改为主题 polish 后计算紧凑高度，最终双版本按实际文字高度检查。
- 这些诊断记录与 `release/` 中针对最终 DLL 的正式验收记录分开保留。

## 截图、日志和调研

- [最终编译日志](<E:/obs spartialFX develop/work/v3.4-colors-20261111/build-final4.log>)
- [CTest 日志](<E:/obs spartialFX develop/work/v3.4-colors-20261111/ctest-final.log>)
- [OBS32 完整运行日志](<E:/obs spartialFX develop/work/v3.4-colors-20261111/release32-final.log>)
- [OBS33 完整运行日志](<E:/obs spartialFX develop/work/v3.4-colors-20261111/release33-final.log>)
- [紧凑 UI 前后对照](<E:/obs spartialFX develop/work/v3.4-colors-20261111/fx-ui-before-after.png>)
- [真实 GPU 选色预览对照](<E:/obs spartialFX develop/work/v3.4-colors-20261111/color-live-preview.png>)
- [抗锯齿与低开销清晰化调研](<E:/obs spartialFX develop/docs/v3.4/V34_AA_CLARITY_RESEARCH.md>)

## 交付与边界

安装器：`dist/OBS_Spatial_FX_V3.4_Setup.exe`；通用入口 `dist/OBS_Spatial_FX_Setup.exe` 为同一文件。源码、PDB 符号和经过脱敏的测试证据分别打包。安装到 `C:/ProgramData/obs-studio/plugins/obs-spatial-fx/`，不包含 OBS 的 Qt DLL。双版本 WebSocket 配置已经恢复到测试前的原始字节。

最终 DLL SHA256：`a6c5111abe23d28019b088c7987e96bb4d117a2202c753e01c9203a284e3daca`。

最终 Setup SHA256：`6187050971b03d72fc560b0b0ce5e62e54b9a41a776cefe57697787d0f3e12d2`。

- NOT RUN：NVIDIA 实机验证，本机无 NVIDIA 设备。
- NOT RUN：OBS34+、未来运行库、其他 Windows/DPI/主题组合及数小时压力测试。
- NOT RUN：SMAA、FXAA、CAS、RCAS 在本插件内的耗时/画质测试；这些新功能本次尚未接入，等待用户决定是否另做小版本。
