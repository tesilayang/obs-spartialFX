# OBS Spatial FX v3.4.2 修复与验证报告

日期：2026-11-12。交付方式：本地 x64 安装包、完整源码 ZIP、符号 ZIP、测试证据 ZIP。原使用说明书保持原样。

## 修复结果

### 1. 受光与阴影无法恢复

旧代码 `resolveFxLighting()` 在灯光页主灯标记为已启用时直接借用该灯的坐标、强度和范围；这个判定没有考虑“启用舞台灯光”总开关。因此，主灯强度为零、距离超出范围等状态会让 FX05 的附加光照消失，关闭舞台灯光总开关仍继续使用同一盏无效主灯。

现在 6 个受光/阴影子功能始终使用随本图层位置、旋转与缩放运动的独立附加光源。渲染上下文已经移除灯光页主灯输入，避免之后再次意外借用；仍保留各子功能的颜色、中心、尺寸与增益控制。灯光页继续负责舞台本身的照明、空间投影和轮廓光。

裁切后的图层使用可见尺寸定位独立光源，采样仍使用原纹理 UV 基底及裁切中心。这样，“裁切图层”与“预先裁好的等尺寸素材”得到一致的默认受光位置。相关 GPU 坐标回归已经实际执行。

### 2. 辉光半径、高光阈值不明显

旧柔光辉光分支根本没有读取高光阈值；高光溢出只有中心加 12 个稀疏圆盘采样，大半径时容易漏掉小亮点或形成离散亮斑。

现在辉光先提取高光，再使用复用纹理的连续高斯卷积；大半径按 2 倍逐级降采样，以控制采样预算。高光溢出按光量叠加，柔光辉光采用柔和屏幕混合，两种模式都使用同一组半径、阈值与强度参数。

- 扩散半径保留 0～160 的原数值范围，单位是素材处理纹理像素；最长边最多 2048 像素的原有处理限制不变。
- 高光阈值保留 0～1：0 更宽松；1 排除 SDR 高光。辉光、星芒、放射光束和镜头鬼影按 RGB 最高通道提取亮部，避免饱和红色、蓝色等亮点因为亮度权重过低而被忽略。
- 辉光两种模式的强度 0 和 SDR 阈值 1 均经过原图像素一致性检查。连续小亮点光晕也经过实际像素检查。
- 保留素材 Alpha；没有高光的暗素材不会自动产生亮点，纯色素材也不会凭空出现有空间形状的光晕。

### 3. 调色响应

实际核验没有发现基础调色滑条发生数值错位。Gamma、RGB、曝光、饱和度的映射和范围保留，步进仍为 0.01；数值框、滑条和重启后的配置读取均经过真实 Qt 检查。

发现的实际问题是 FX 中间纹理为 8 位，第一次调色就会把大于 1 的高光截断，后面的调色再降曝光也无法还原。现在中间结果、时间效果的原始帧和历史纹理统一使用 RGBA16F，避免提前截断，并保持 D3D 复制所需的格式一致。

已验证先 +2 档曝光、再 -2 档曝光能恢复测试素材；六个色区的色相/饱和度/密度、三段明暗、白平衡和曲线节点均能独立改变实际画面。还执行了合法组合极值 Gamma 0.1、RGB 3、曝光 +5 后接亮度曲线的检查。浮点中间纹理不代表已经验证 HDR 工作流；最终 SDR 输出仍会截断超亮颜色。

## 真实复现与测试条件

- 修复前直接运行已交付的 3.4.1 安装版：50 项专项检查中 32 项失败，记录在 `work/v3.4.2/baseline-gpu33/fx342-tests.json`；没有把这些失败算作 PASS。
- 独立光源像素对比关闭测试图层的舞台受光，以隔离附加光源本身；另外的 17 项受光回归使用正常舞台受光图层，覆盖舞台总开关开/关时 FX 仍有贡献。主灯对最终画面的正常照明、全黑照明或过曝不等于 FX 被关闭。
- 真实 GPU 测试使用自行生成的灰阶、色块、小高光和透明轮廓素材；不是渲染示意图，也不代表用户已认可全部美术风格。
- 每次检查实际加载模块路径、DLL SHA256 和 19 个安装资源文件。OBS32 从 `bin/64bit` 加载，OBS33 便携验证从安装器提供的 `portable33` 路径加载。没有覆盖运行中的 OBS 文件。

## 环境

Windows 10 Pro 22H2，10.0.19045 x64；AMD Ryzen 7 5800X，约 32 GiB 内存；AMD Radeon RX 6900 XT，驱动 32.0.21045.20006，实际 D3D11 渲染设备为 RX 6900 XT。构建使用 VS2022/MSVC、CMake/Ninja、Qt6.11.1 和现有 OBS32 SDK 头文件；源码 UTF-8，MSVC `/utf-8`。实测 OBS 32.2.2、33.0.1，运行时 Qt6.11.1。

## 检查结果

真实构建成功；CTest 为 8/8 PASS。安装器 18/18 PASS，覆盖 3.4.1 → 3.4.2、文件锁导致的升级回滚、配置保留、控制面板版本、卸载、重装、没有打包 Qt DLL/QA 驱动。运行中升级保护实测返回 10，原 DLL 哈希不变。

### OBS 32.2.2

| 检查 | 实测结果 |
|---|---|
| v3.4.2 专项（含组合极值） | 101/101 PASS |
| 95 项效果真实 GPU 渲染 | 202/202 PASS |
| SMAA / FXAA / RCAS | 66/66 PASS |
| 颜色实时预览 | 13/13 PASS |
| 模糊同组独立效果 | 6/6 PASS |
| 模糊质量 | 14/14 PASS |
| 模糊边界 | 10/10 PASS |
| 受光控制 | 17/17 PASS |
| 既有 FX 修复回归 | 36/36 PASS |
| 坐标、裁切与运动 | 8/8 PASS |
| 实际本地 x264 编码 | 7/7 PASS |
| 全部 FX 原生界面 | 79/79 PASS |
| 全部 FX 重启保存 | 18/18 PASS |
| 抗锯齿原生界面 | 35/35 PASS |
| 抗锯齿重启保存 | 16/16 PASS |
| 调色/辉光滑条原生界面 | 47/47 PASS |
| 调色/辉光重启保存 | 15/15 PASS |

### OBS 33.0.1

| 检查 | 实测结果 |
|---|---|
| v3.4.2 专项（含组合极值） | 101/101 PASS |
| 95 项效果真实 GPU 渲染 | 202/202 PASS |
| SMAA / FXAA / RCAS | 66/66 PASS |
| 颜色实时预览 | 13/13 PASS |
| 模糊同组独立效果 | 6/6 PASS |
| 模糊质量 | 14/14 PASS |
| 模糊边界 | 10/10 PASS |
| 受光控制 | 17/17 PASS |
| 既有 FX 修复回归 | 36/36 PASS |
| 坐标、裁切与运动 | 8/8 PASS |
| 实际本地 x264 编码 | 7/7 PASS |
| 全部 FX 原生界面 | 79/79 PASS |
| 全部 FX 重启保存 | 18/18 PASS |
| 抗锯齿原生界面 | 35/35 PASS |
| 抗锯齿重启保存 | 16/16 PASS |
| 调色/辉光滑条原生界面 | 47/47 PASS |
| 调色/辉光重启保存 | 15/15 PASS |

OBS33 首轮整套专项为 100/100；之后补充组合极值，使用同一最终安装 DLL 在 `boundary-gpu33` 完整执行 101/101，并额外保留其模块加载和正常退出日志。其余完整结果位于 `release`。两种 OBS 均完成 6 轮原生界面/重启检查，每种共 210 项；日志检查未发现着色器编译、参数缺失或 D3D11 错误，均正常退出。

### 性能范围

下表是 160 像素最大半径辉光，在实际 1920×1080、60fps 场景中预热后的连续 30 秒。渲染时间是 OBS 的整帧统计，不是 GPU 功耗或光晕独占 GPU 时间。

| 运行版本 | 最低帧率 | 最高 OBS 平均帧渲染时间 | 新增渲染丢帧 | 分配计数（前→后） |
|---|---|---|---|---|
| OBS 32 | 60.00 fps | 0.356 ms | 0 | 27 → 27 |
| OBS 33 | 60.00 fps | 0.413 ms | 0 | 27 → 27 |

另有原有混合 FX 链的 30 秒资源复用检查，以及 SMAA + RCAS 动画场景的实际本地 x264 编码检查。无新增预热后渲染丢帧、编码丢帧；按需编译全部效果的启动过程不包含在稳态性能结论内。

NOT RUN：核显、NVIDIA、Windows11、Win10 1909、HDR 工作流、互联网推流、数小时连续运行、电功耗对比。RGBA16F 每张中间纹理的字节数比原 RGBA8 增加；只为实际启用的 FX 分配并复用。不能承诺其他设备与本开发机有相同的资源占用或能耗。

## 修改文件

- `src/fx_lighting.hpp`：独立附加光源；`src/stage.cpp`、`src/fx_renderer.hpp`：按裁切后的可见尺寸提供统一空间基底。
- `src/fx_renderer.cpp`：高光提取/复用高斯扩散、浮点中间纹理与一致的历史帧格式。
- `data/effects/fx04.body`、`data/effects/fx_blur.body`：连续辉光、两种混合方式和正确使用阈值。
- `src/fx_catalog.cpp`、`src/fx_catalog_a.inc`：更新真实生效条件、单位和范围的悬停说明。
- `CMakeLists.txt`、`src/plugin.cpp`、`src/dock.cpp`、`installer/setup.cpp`、`installer/setup.rc.in`：3.4.2 版本与安装器。
- `tests/v342_fx_probe.py`、`tests/v342_qa.inc`、`tests/obs_qa_driver.cpp`：新增 GPU/滑条/重启检查；相关既有受光、坐标和版本检查同步新的独立光源约定。
- `tools/run_v342_regressions.ps1`、`tools/test-installer-v342.ps1`、`tools/package_v342.py`：可重跑的构建外验证与证据门禁打包。

## 实际命令与证据

以下均已经实际执行，详细输出在 `work/v3.4.2/logs`：

```powershell
& tools/build.cmd 4
& tools/test.cmd
& tools/test-installer-v342.ps1
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.4.2/release/bridge33final
& tools/run_v342_regressions.ps1 -Major 33
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.4.2/boundary-bridge33
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tests/v342_fx_probe.py work/v3.4.2/boundary-gpu33 work/v3.4.2/boundary-bridge33
& tools/run_v33_installed_qa.ps1 -Major 32 -Scenario v34-hold -Output work/v3.4.2/release/bridge32final
& tools/run_v342_regressions.ps1 -Major 32
```

- 最终完整构建：`logs/build-3.log`；测试：`logs/test-final.log`。
- 真实原生界面：`release/ui34232/v342-glow-controls.png`、`release/ui34233/v342-glow-controls.png`，以及对应 `obs-main.png`。
- [修复前后实际渲染对比](../../work/v3.4.2/review/before-after.png)（第二行为两张实拍输出的像素差异图，已标明，不是原画面）。
- [OBS33 全量效果输出记录](../../work/v3.4.2/release/gpu33/render-tests.json)。
- 安装、环境恢复和最终打包结果均保留 JSON 与 SHA256 记录；安装包与源码 ZIP 的校验值见 `dist/SHA256SUMS.txt`。

## 交付校验

- DLL SHA256：`d8ae84d7dede97595c651359e217d483d648cfcd5e175c90f8444b4b22f770a9`。
- 安装器 SHA256：`4d46ccfc17d463d5c0f909165ad1d702a583f5e36f6f993b4a99b2e3bfa175dd`。
- 旧版安装器、旧版源码和已有说明书均按原哈希保留；额外发现的用户手册 HTML 没有修改或并入新源码包。

## 发布与验收标准更新

2026-11-12，按用户指令发布 v3.4.2。后续兼容性验收仅使用 OBS 33.0.1，回归入口、安装器检查和打包门槛已同步。上文 OBS32 结果保留为历史实测记录。开发与交付统一执行根目录 `DEVELOPMENT_STANDARDS.md`，代理执行入口为 `AGENTS.md`。安装包、插件和着色器保持本轮已验证字节；本次更新标准、测试入口和源码快照，使用说明书保持原样。
