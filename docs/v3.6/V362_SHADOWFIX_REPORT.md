# OBS Spatial FX v3.6.2 软阴影修复交付报告

日期：2026-11-14。版本号保持 **3.6.2**。交付目录：`dist/v3.6.2-shadowfix/`。原 `dist/v3.6.2/` 安装包、源码和证据保留原样；本次未更新说明书，未提交、推送或发布 GitHub。

## 修复结果与根因

旧算法将九个偏移光源的硬投影平均。增加半径只是拉开多个轮廓，产生清晰边缘和台阶。实际 OBS 33.0.1 失败证据保留于 `work/v3.6.2-shadowfix/red/`，没有覆盖原失败结果。

现在逐个生成素材的硬 Alpha 投影，再进行连续高斯卷积，最后合成阴影。按光源、投影物与接收面间距换算 XY 半影宽度；XY 分轴 padding、分轴降采样，防止细长接收层丢失细节。按 XY 分辨率组合缓存 GPU 资源，避免不同深度素材之间反复重新分配纹理。

| 实际画面指标 | 旧安装载荷 | 最终安装载荷 |
|---|---:|---:|
| 半径80：最大相邻像素亮度跳变 | 16 | 3 |
| 半径160：最大相邻像素亮度跳变 | 16 | 2 |
| 半径300：最大相邻像素亮度跳变 | 16 | 1 |
| 半径300：单条投影的分离轮廓 | 3 | 1 |

截图：修复前 `red/radius-300.png`，修复后 `verified-softness/radius-300.png`；完整亮度剖面见各自 `profiles.json`。均为真实 OBS 输出，使用合成测试素材。

缓存回归另有真实失败记录 `cache-red/stress-tests.json`：混合深度素材在3秒内从304增加至676次分配；最终相同检查在预热后分配数不再增长，见 `verified-stress/stress-tests.json`。

## 本次修改文件

- `src/stage.cpp`
- `src/fx_blur.cpp`
- `src/fx_blur.hpp`
- `src/shadow_blur.hpp`
- `data/effects/spatial.effect`
- `data/effects/fx_blur.body`
- `tests/scene_evaluator_tests.cpp`
- `tests/v362_shadow_softness_probe.py`
- `tests/v362_shadow_stress_probe.py`

另新增 `tools/test-installer-v362-shadowfix.ps1`、`tools/package-v362-shadowfix.py`、本报告及 `docs/v3.6/V362_SHADOWFIX_IMPLEMENTATION.md`。相对本轮初始源码的精确差异为证据包内 `task-changes.diff`。工作区保留此前未提交的版本开发内容；源码快照基于提交 `055034ce9380c80cf19460bd127aafea9f4551d3` 并包含实际工作区内容。

## 实际编译与验收命令

在仓库根目录执行；Python 使用本机 Codex runtime 的 `python.exe`。

```powershell
$env:OBS_ROOT = Join-Path $PWD 'runtime/obs33'
tools/build.cmd 4
tools/test.cmd
tools/test-installer-v362-shadowfix.ps1
tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.6.2-shadowfix/verified-bridge
python tests/v362_shadow_softness_probe.py work/v3.6.2-shadowfix/verified-softness work/v3.6.2-shadowfix/verified-bridge
python tests/v362_shadow_stress_probe.py work/v3.6.2-shadowfix/verified-stress work/v3.6.2-shadowfix/verified-bridge work/v3.6.2-shadowfix/verified-softness
python tests/v362_light_probe.py work/v3.6.2-shadowfix/verified-light work/v3.6.2-shadowfix/verified-bridge
python tests/v36_render_probe.py work/v3.6.2-shadowfix/verified-gpu work/v3.6.2-shadowfix/verified-bridge
python tests/v34_render_probe.py work/v3.6.2-shadowfix/verified-fx work/v3.6.2-shadowfix/verified-bridge
python tests/v34_coordinate_probe.py work/v3.6.2-shadowfix/verified-fx work/v3.6.2-shadowfix/verified-bridge
python tests/v362_light_grade_probe.py work/v3.6.2-shadowfix/verified-grade work/v3.6.2-shadowfix/verified-bridge work/v3.6.2-shadowfix/verified-light
python tests/v342patch_probe.py work/v3.6.2-shadowfix/verified-fx-lighting work/v3.6.2-shadowfix/verified-bridge
tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.6.2-shadowfix/verified-restart
python tests/v362_restart_probe.py work/v3.6.2-shadowfix/verified-restart work/v3.6.2-shadowfix/verified-restart work/v3.6.2-shadowfix/verified-light
work/v3.6.2-shadowfix/restore-environment.ps1
python tools/package-v362-shadowfix.py
```

## 验证结果

| 项目 | 状态 | 本轮实际证据，路径相对 work/v3.6.2-shadowfix |
|---|---|---|
| C++ DLL 和安装器真实编译 | PASS | `build-final.log`，退出0 |
| CTest | PASS | 13/13；`test-final.log` |
| 软阴影真实图像专项 | PASS | 20 项；`verified-softness/softness-tests.json` |
| 裁剪、边界、混合深度缓存及多灯多素材 | PASS | 6 项；`verified-stress/stress-tests.json` |
| 灯光操作和渲染 | PASS | 31 项；`verified-light/light-tests.json` |
| 保存后全新进程重启 | PASS | 14 项；`verified-restart/restart-tests.json` |
| 95 个 FX 与公共模糊管线 | PASS | 202 项；`verified-fx/render-tests.json` |
| 装饰图形和遮罩 | PASS | 24 项；`verified-gpu/geometry-tests.json` |
| 坐标对照 | PASS | 8 项；`verified-fx/coordinates/coordinate-tests.json` |
| 高饱和度灯光调色 | PASS | 1 项；`verified-grade/grade-tests.json` |
| 独立 FX 环境染色和背光 | PASS | 25 项；`verified-fx-lighting/patch-tests.json` |
| 安装升级、回滚、卸载重装 | PASS | 18 项；`installer-final/installer-tests.json` |
| OBS运行中安装拦截 | PASS | 返回10、文件哈希不变；`verified-running-guard.json` |
| 退出与环境恢复 | PASS | 两个最终 OBS 进程正常退出；恢复153份配置，移除QA，无4457监听；`environment-restored.json` |

独立代码复审提出的各向异性细节损失、缓存反复resize均已修复；最终复审未发现新的重要问题。复审本身只读，运行结论来自上述实际测试。

## 性能

实测设备：Windows10专业版19045、AMD Ryzen7 5800X、AMD Radeon RX6900XT、31.92GiB RAM。D3D11、画布1920×1080、60fps，合成静态Alpha素材；单灯/四灯场景为1个投影物，压力场景为4灯×4个投影物、半径300、1个接收层。所有场景预热后纹理分配稳定、渲染失败数为0。

| 场景 | 时长秒 | FPS均值 | OBS CPU均值 | 总帧渲染毫秒均值 | 新增渲染丢帧 | 内存变化MiB |
|---|---:|---:|---:|---:|---:|---:|
| disabled | 30.09 | 60.000 | 0.380% | 0.302 | 0 | -0.62 |
| one-soft-light | 30.09 | 60.000 | 0.351% | 0.323 | 0 | -0.07 |
| four-soft-lights | 30.08 | 60.000 | 0.387% | 0.385 | 0 | 0.00 |
| four-lights-four-casters-radius300 | 30.06 | 60.000 | 0.758% | 0.737 | 0 | -0.03 |

原始采样、资源计数与设置保存在 `verified-light/light-tests.json`、`verified-stress/stress-tests.json`、`hardware.json`。FX三效果组合另完成一次30秒回归。

独立背光首轮性能检查为 **FAIL**：开始采样后的短时段累计8帧渲染丢帧，最低52.615fps；资源分配稳定，渲染器失败数为0，未发现着色器错误。该次失败原样保留在 `verified-fx-lighting-first-failure/`。瞬时停顿的来源尚未定位。相同源码、安装载荷和测试条件下，随后连续两轮各25项复测均为 PASS，各包含30秒性能采样、无新增渲染丢帧；见 `verified-fx-lighting-repeat1/` 和 `verified-fx-lighting/`。没有用重测结果改写首轮失败记录。

未进行物理功耗测量；核显、NVIDIA、Windows11、长时推流均为 NOT RUN。

## 兼容与交付校验

世界坐标、旋转、关键帧及JSON字段保持原规则，无存档迁移。阴影图为512×512；每个投影物以中心深度估计高斯半影宽度，倾斜素材不执行逐像素物理面积光源积分。硬投影保持射线和平面求交。

最终 DLL SHA256：`0367f423bc6ed7e84bb164ab24ddbdf0510ffbcd9cd1b3f85e4451cd63e0c0c0`。

最终安装器 SHA256：`3856c944c48093689246cd7ac37c3558241603c3ba82df000197f880923976f2`。

最终加载路径与全部着色器哈希见 `verified-bridge/installed-path.json`、`verified-restart/installed-path.json`；正常卸载日志见同目录 `obs-runtime.log`。安装包、源码、PDB、证据ZIP的校验值见交付目录 `SHA256SUMS.txt` 和 `DELIVERY_MANIFEST.json`。关闭 OBS 后运行新修复安装包即可覆盖升级。
