# v3.4 模糊与局部光源修复记录（2026-11-11）

版本保持 **3.4.0**，覆盖同名 v3.4 安装包。最终插件 SHA256：`027c850943010602cf71d4f63a1e5f5afcb4ffca84aa1b057d06ca7be1d5d8ba`。
本文件是新增开发验证记录；用户说明书、选择 JSON、原实现映射和 v3.3 安装包不更新。此前交付的 v3.4 完整保存在 `work/v3.4-blurfix-20261111/baseline/dist`。

## 修复内容

1. **模糊**：旧 7×7 采样在半径 100 时，相邻采样间隔约 33 像素，造成轮廓重复、格子和磨砂感。改为连续相邻像素卷积。高斯采用归一化钟形权重（sigma=radius/3），方框采用归一化均匀权重；镜头散景采用圆形/多边形光圈覆盖，保留高光增强与光圈旋转。径向和移轴按位置改变模糊半径。方向拖线、运动模糊、残影拖尾、像素化保留独立路径，并验证非透明输出；静止运动模糊恢复原图。
2. **资源**：大半径先做带抗混叠的逐级缩小，再卷积；中间结果使用预乘 Alpha、FP16。纹理按图层和缩小级别缓存，避免不同模糊半径叠加时每帧改尺寸。计数包含实际后备纹理分配/调整尺寸。奇数宽高采用四点足迹平均，1 像素高素材有完整长轴卷积支持。镜像是边界取样规则，并非另一种卷积核；远离边缘时，与同核的钳制模式可相同。
3. **受光与阴影说明**：组内最上方加入可见、自动换行的操作提示。“启用舞台灯光”控制舞台照明和跨图层投影；材质 FX 可独立启用，读取已启用的主灯或第一盏已启用附加灯，无启用灯时使用随素材移动的局部效果灯。关闭该舞台总开关，不等于取消材质 FX；显式设为零强度的灯仍保持不发光。
4. **烛光/聚光灯**：均追加“中心 X 偏移”“中心 Y 偏移”“光照大小比例”，步进 0.01。XY 范围 -1～1，默认 0；1 对应素材完整宽/高，正 X 向右、正 Y 向下。大小 0.01～10，默认 1。偏移以原灯光投影中心为基准，随素材变换；原灯位置、颜色、强度、范围仍有作用。新参数追加在旧参数后，旧场景自动取零偏移/大小 1，保留原受光表面比例、裁切与旧灯光位置映射。
5. **安装资源**：新增 blur shader 资源 216，包含在部署及卸载清单中。继续安装到 ProgramData；同版本升级不改场景/预设，不携带 Qt DLL 或 QA driver。

## 实际构建与运行

Windows 10 22H2 x64，AMD Ryzen 7 5800X / Radeon RX 6900 XT，D3D11；实际运行 OBS 32.2.2 和 OBS 33.0.1。VS2022 MSVC C++17，Qt6，`/utf-8`。所有 `gs_*` 资源创建/渲染/释放沿用图形上下文；未增加渲染线程等待 UI 的锁。

最终生产代码真实编译命令为 `tools/build.cmd 4`，记录 `build-final4.log`：重新编译 fx_renderer、链接 DLL、嵌入安装资源并链接 Setup。`build-final5.log` 确认构建成功；`build-ready.log` 只重建 QA driver。`tools/test.cmd` 的 `ctest-final5.log` 与最终 `ctest-release.log` 均为 **8/8 PASS**。

以下都是最终安装 DLL 的实际运行结果，失败数均为 0；仅 `release/` 为本次最终验收目录：

| 检查 | OBS 32 | OBS 33 | 证据子目录/JSON |
| --- | --- | --- | --- |
| 全 FX 默认画面与模式 | 196/196，92 效果/237 变体 | 196/196，92 效果/237 变体 | gpu32、gpu33 / render-tests.json |
| 坐标/裁切/实际运动模糊 | 8/8 | 8/8 | gpu32、gpu33 / coordinates/coordinate-tests.json |
| 连续模糊质量 | 14/14 | 14/14 | quality32、quality33 / blur-quality.json |
| 奇数尺寸/窄素材/缓存复用 | 10/10 | 10/10 | edge32、edge33 / blur-edgecases.json |
| 烛光/聚光灯中心、大小、灯光兼容 | 17/17 | 17/17 | light32、light33 / lighting-controls.json |
| 非密集模糊兄弟与静止原图 | 6/6 | 6/6 | siblings32、siblings33 / blur-siblings.json |
| 渐变、LED、21:9、受光回归 | 36/36 | 36/36 | patch32、patch33 / patch-tests.json |
| 原生 Dock 操作与 .01 步进 | 69/69 | 69/69 | ui32、ui33 / dock-smoke.json |
| 重启保存/恢复 | 15/15 | 15/15 | persist32、persist33 / dock-smoke.json |

质量测试将半径 3/30/100 的高斯和方框与密集 CPU 卷积的 Alpha 参考比较，平均绝对误差均 <2/255；同时检查大半径连续覆盖、高频网格抑制和径向/移轴过渡不保留清晰重影。这些数值验证不代替用户对艺术效果的判断。

代表性 FX 链在 1080p/60 逐秒取样 30 秒，混合三种模糊半径另外取样 10 秒：

| 运行 | 30 秒最低 fps | 稳态纹理分配数 | 累积跳帧（前→后） | 10 秒混合模糊纹理分配数 |
| --- | --- | --- | --- | --- |
| OBS 32 | 60.00 | 29 → 29 | 29 → 29 | 17 → 17 |
| OBS 33 | 60.00 | 29 → 29 | 36 → 36 | 17 → 17 |

“跳帧前值”包含测试前创建场景及首次编译时的累积量，不能声称整次启动零跳帧；上述稳态窗口没有新增跳帧。每版本的 performance.json 保留完整样本。

实际加载路径及其 DLL 哈希、17 个 shader 文件哈希保存在 bridge32/33 与 ui/persist 的 installed-path.json；分别验证 OBS32 的 bin/64bit 和 OBS33 的 portable33 路径。所有六份完整运行/退出日志检查 shader 参数缺失、编译失败、插件错误，并有正常 module unloaded 记录。

安装测试 `tools/test-installer-v34-blurfix.ps1` **18/18 PASS**：从此前 v3.4 升级、锁文件失败回滚、配置保留、卸载/保留非所属文件和预设、重装、Windows 控制面板登记。运行中保护实际返回 **10**，DLL 哈希未变，记录 install-running-guard.json。最终系统安装内容与本次 DLL/Setup 一致。

## 重现命令

```powershell
& tools/build.cmd 4
& tools/test.cmd
& tools/test-installer-v34-blurfix.ps1
# 在所属隔离测试实例配置好临时测试 WebSocket 后，逐版本运行：
& tools/run_v33_installed_qa.ps1 -Major 32 -Scenario v34-hold -Output work/v3.4-blurfix-20261111/release/bridge32
& tools/run_v34_blurfix_release.ps1 -Major 32
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.4-blurfix-20261111/release/bridge33
& tools/run_v34_blurfix_release.ps1 -Major 33
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tests/fx05_light_ray_probe.py
# 恢复两份测试 WebSocket 原始配置，补齐本记录/实际执行清单后：
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tools/package_v34_blurfix.py --check
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tools/package_v34_blurfix.py --package
```

GPU 专项的真实 Python 命令逐条保存在 release/commands32.log、commands33.log；每条退出码见 release-run.json。独立光线数学探针真实编译并执行，17 项断言通过，见 lighting-ray-test/result.log。

## 失败与修正记录

初始复现、奇数尺寸混叠、不同半径缓存反复改尺寸、1 像素高素材半径截断、过渡保留锐利重影等失败均保留日志。中途发现 OBS 编译器保留无效分支的 shader 参数，非密集模糊四个兄弟变透明；已对每个 child 初始化 blur_image 和 blur_minimum_radius，再用最终 DLL 重跑全部分支。

一次 OBS32 隔离启动死锁是测试在 FINISHED_LOADING 前发出 WebSocket CreateScene：主线程启动 AudioMixer 等 signal mutex，WebSocket 线程持该锁等 UI。线程栈在 hang2700/stacks.txt。仅结束所属挂起 QA 进程；新增 PID 校验的原子就绪标记后再允许创建场景。恢复对话框正常启动后的该次就绪超时不计 PASS，随后完整重启重测。UI32 首次正常退出时测试程序未持有进程 Handle，ExitCode 为 null，严格门禁拒绝；保留 Handle 后重新运行得到明确正常退出与新报告。

QA 同时清除旧报告/桥命令，核对报告场景、时间、非空断言、真实加载模块与 shader 哈希，并扫描正常退出后的完整日志。收紧日志检查时曾把 QA 自身“0 failures”提示误判为产品错误，已将前缀限制为产品及数字版本，并重跑；OBS33 的模块列表缓存也已改为每轮 Refresh，重新取得实际加载路径。旧目录及名字含 final 的中间失败尝试不参与最终通过判断；证据包把这些归入 diagnostic-history。

## 参考与边界

对照本地 obs-shaderfilter 2.6.1 的 Gaussian/box 示例及 Xaymar/obs-StreamFX **0.12.0b299** 的 Gaussian/box effect 与 C++ render 方法。参考快照在 work 的 references/。采用标准归一化卷积思想独立实现，没有复制外部项目代码或将其二进制打包。shaderfilter 示例中随强度增大取样步距的方式未照搬，以避免再次稀疏采样。

- NOT RUN：NVIDIA 实机、OBS34+、数小时压力测试、92 项同时开启。已测性能仅代表明确记录的特效链及窗口。
- 大半径通过抗混叠降采样近似全分辨率卷积；径向/移轴的空间变化可分离卷积也是近似。高斯和方框对某些平色素材视觉差异可能不大。
- 像素效果限于源素材矩形；外侧投影需要源内透明留边。镜像仅影响触及边界的取样。

## 实际修改文件

以下相对此前交付 v3.4 Source.zip 按字节比较生成；另新增本记录文件。

- `CMakeLists.txt`
- `data/effects/fx01.body`
- `data/effects/fx05.body`
- `data/effects/fx_blur.body`
- `installer/setup.cpp`
- `installer/setup.rc.in`
- `src/fx_blur.cpp`
- `src/fx_blur.hpp`
- `src/fx_catalog_a.inc`
- `src/fx_panel.cpp`
- `src/fx_renderer.cpp`
- `tests/fx05_light_ray_probe.py`
- `tests/fx_tests.cpp`
- `tests/obs_qa_driver.cpp`
- `tests/v34_blur_edgecases.py`
- `tests/v34_blur_quality.py`
- `tests/v34_blur_siblings.py`
- `tests/v34_coordinate_probe.py`
- `tests/v34_lighting_controls_probe.py`
- `tests/v34_qa.inc`
- `tools/package_v34_blurfix.py`
- `tools/run_v33_installed_qa.ps1`
- `tools/run_v34_blurfix_release.ps1`
- `tools/test-installer-v34-blurfix.ps1`

## 实际截图

![真实 OBS 截图合成：原图、旧高斯、新高斯、新方框](E:/obs spartialFX develop/work/v3.4-blurfix-20261111/blur-before-after.png)

![最终 OBS33 原生 Dock 的受光说明和烛光控制](E:/obs spartialFX develop/work/v3.4-blurfix-20261111/release/ui33/lighting-controls.png)

原始截图、运行日志、构建日志和测试 JSON 均保留本地工作目录；分发证据包剔除场景配置、桥命令、WebSocket 凭据和用户素材。
