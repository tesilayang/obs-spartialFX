# v3.6.4 专项测试与交付报告

日期：2026-11-15。验收只使用 OBS **33.0.1**，测试本次透明来源、预设关键帧、统一对象动画及其直接关联功能。

## 结论

| 项目 | 状态 | 实际证据 |
|---|---|---|
| C++17 插件、Qt Dock、安装器真实编译 | PASS | `build-final2.log` |
| 10 个相关 CTest 套件 | PASS | `test-final2.log`，10/10，5.18 秒 |
| 最终安装器覆盖安装 | PASS | `install-final2.log`，退出 0，ProgramData 部署 |
| 最终安装载荷加载，单一 DLL，无重复版本 | PASS | `final2-bridge/installed-path.json` |
| 真实 Qt/OBS 交互：39 项检查 | PASS | `final2-acceptance/acceptance.json`、界面截图 |
| 直通/预乘 Alpha 合成来源，10 个取样点 | PASS | `final2-alpha/alpha-results.json` |
| 真实 VTube Studio Spout2 Alpha | PASS | `final2-spout/spout-results.json`及截图 |
| 真实 VTube Studio NDI | BLOCKED | `final-sender-discovery.json`：NDI 来源插件已加载，无发送端 |
| 灯光、像素 FX、舞台 FX 动画实际改变画面 | PASS | `final2-animation/results.json` |
| 几何 FX 确定时间、历史缓存定位清理、纹理复用 | PASS | `final2-clock/results.json` |
| 保存、正常退出、重新启动恢复动画与画面 | PASS | `final2-restart/results.json`，画面差值 0 |
| 新灯光/FX/图层轨 JSON、ZIP 工程往返 | PASS | `final2-acceptance/roundtrip-*.json` |
| 1080p60 四组专项性能 | PASS | `final2-performance/performance.json` |
| 正常退出、原测试配置和运行目录还原、关闭 4457 | PASS | `cleanup.json`：153 个配置文件、19 个原插件文件逐文件哈希一致 |
| 长时间推流、无关缺素材提示、卸载重装全回归 | NOT RUN | 本版没有修改这些流程 |
| 自动加载迁移的可见取消按钮 | NOT RUN | 核心取消回调有单测，自动加载没有独立 UI 入口 |

最终 DLL SHA256：

```text
FD8C6486421B49E5FB9341574CE8F9BEA05A9D21CF211F8A557B05457BA8DD83
```

实际加载位置：

```text
C:\ProgramData\obs-studio\plugins\obs-spatial-fx\portable33\obs-spatial-fx\obs-spatial-fx.dll
```

安装器与着色器逐项摘要见交付目录 `PAYLOAD_MANIFEST.json`；交付文件摘要见 `SHA256SUMS.txt`。证据包中的目录均相对于本地 `work/v3.6.4/`。

## 环境和实际命令

Windows 10 22H2 x64，Build 19045.6466；AMD Ryzen 7 5800X、16 逻辑处理器、32GB 内存；AMD Radeon RX 6900 XT。实际 OBS 日志与 CPU/GPU 信息随证据包保存。构建使用 VS2022/MSVC、Qt6、固定 CMake/Ninja 依赖。

在工程根目录执行：

```powershell
cmd /c "tools\build.cmd 4 > work\v3.6.4\build-final2.log 2>&1"

& third_party/cmake/cmake-4.4.5-windows-x86_64/bin/ctest.exe --test-dir build `
  -R '^spatial-(model|scene-evaluator|fx|fx-stage|presets|composer|preset-panel|timeline-object|editor-timeline|fx-clock)$' `
  --output-on-failure > work/v3.6.4/test-final2.log 2>&1

Start-Process build/OBS_Spatial_FX_V3.6.4_Setup.exe `
  -ArgumentList '/SILENT','/LOG="E:\obs spartialFX develop\work\v3.6.4\install-final2.log"' -Wait

& tools/run_v33_installed_qa.ps1 -Scenario v34-hold -Output work/v3.6.4/final2-bridge

$taskPython='C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
& $taskPython tests/v364_acceptance.py work/v3.6.4/final2-bridge work/v3.6.4/final2-acceptance
& $taskPython tests/v364_alpha_probe.py work/v3.6.4/final2-alpha
& $taskPython tests/v364_gpu_animation.py work/v3.6.4/final2-bridge work/v3.6.4/final2-animation
& $taskPython tests/v364_gpu_clock.py work/v3.6.4/final2-bridge work/v3.6.4/final2-clock
& $taskPython tests/v364_spout_probe.py work/v3.6.4/final2-spout work/v3.6.4/final2-bridge
& $taskPython tests/v364_performance.py work/v3.6.4/final2-bridge work/v3.6.4/final2-performance final364

# 正常退出第一实例后启动相同配置，验证持久化结果。
& tools/run_v33_installed_qa.ps1 -Scenario v34-hold -Output work/v3.6.4/final2-restart-bridge
& $taskPython tests/v364_restart.py work/v3.6.4/final2-restart-bridge work/v3.6.4/final2-acceptance work/v3.6.4/final2-restart
& $taskPython tests/v364_sender_discovery.py
& tests/v364_cleanup.ps1

git -c core.whitespace=cr-at-eol diff --check
& $taskPython tools/package_v364.py
```

首个实例通过 QA `finish` 正常关闭；恢复脚本同样通过 `finish` 关闭最终重启实例。两份最终 OBS 日志均记录完整卸载及 `Number of memory leaks: 0`。安装器测试在所有 OBS 进程退出后执行。

## 透明边缘与 GPU 结果

基线真实 Spout ALPHA 输出为直通数据，原缓存强制使用预乘混合导致半透明像素 RGB 大于 Alpha：旧载荷 14054 个半透明像素中，5902 个超过 A+3。修复后真实来源仍保留发丝和半透明度，不再出现该错误界。

最终安装载荷重新测试真实 `VTubeStudioSpout`，分辨率 1326×978。默认、0.73 倍、1.37 倍、旋转 17°、透明度 0.5 均保留半透明像素，未受光条件下异常 RGB>A+3 数为 0。来源为实时角色，两次采样姿态有差异，不进行跨时刻像素相等判断。黑、白、蓝背景对照由真实截图按预乘公式合成，原始截图也保留。

打开灯光的截图用于检查可渲染性，灯光可以有意增加 RGB，不以 RGB≤A 判断照明是否正确。没有宣称覆盖所有灯光组合。

灯光强度动画在真实 GPU 输出上的平均像素差为 35.89446；FX03.0 红色轨将 `(204,102,51,255)` 变为 `(0,102,51,255)`；宽银幕强度轨将外围透明像素变成黑边。几何 FX 定位回 0.25 秒后像素差为 0；历史帧跳转后的输出与无历史参考差为 0.00005787，定位前后分配计数保持 8，渲染失败计数 0。

## 动画和交互覆盖

- 对象 ID、图层/FX 重排、参数类型、范围、颜色和关闭效果后保留轨道。
- 灯光页与时间轴同步；自动关键帧、基础值与预览求值分离；颜色实时预览、取消恢复、一次撤销。
- 对象右键复制、播放头对齐粘贴、独立实例 ID、来源共享；删除取消、确认、撤销和重做。
- 旧 3.6.3 运动块备份与迁移；新一键预设显示普通关键帧，可以删除和撤销；预算、异常输入和取消回调的原子失败。
- FX 页初始化后从时间轴删除该 FX 的安全性；图层、效果和参数锁对 FX 卡片删除的限制；灯光/参数锁对灯光页删除的限制；FX 卡片删除撤销恢复完整动画。
- 新轨道保存和重启，以及工程 JSON/ZIP 往返。工程测试只覆盖本版新增动画数据，没有扩展为全部导入导出测试。

界面截图包括 `final2-bridge/timeline-native-keys.png`、`timeline-context-menu.png`、`preset-native-keys.png`、`fx-removed-safe.png`。

## 1080p60 性能

固定单张 800×600 RGBA 测试图、1920×1080/60fps，图层 6 秒横移动画；每组预热 3 秒，正式采样约 30 秒。灯光组增加强度/位置动画，FX 组增加颜色动画和历史帧效果，组合组同时开启。帧率/CPU/耗时通过 OBS WebSocket 记录，Windows 计数器记录目标进程 GPU 引擎与显存。

| 场景 | 3.6.3 CPU % | 3.6.4 CPU % | 3.6.3 渲染 ms | 3.6.4 渲染 ms | 3.6.4 GPU 最忙引擎均值 % | 3.6.4 渲染丢帧 |
|---|---:|---:|---:|---:|---:|---:|
| 基线图层动画 | 0.289 | 0.458 | 0.333 | 0.617 | 3.288 | 0 |
| 灯光动画 | 0.325 | 0.429 | 0.379 | 0.607 | 3.060 | 0 |
| FX 动画 | 0.312 | 0.618 | 0.464 | 0.733 | 3.366 | 0 |
| 组合 | 0.293 | 0.621 | 0.484 | 0.794 | 2.815 | 0 |

所有组约 60fps，每组约 1802～1803 帧，纹理分配在预热后稳定，失败 0。3.6.4 目标 OBS 进程专用 GPU 内存约 564.5～569.0MiB，共享 GPU 内存约 127.1MiB；进程私有内存约 1460～1465MiB。这些是整个隔离 OBS 进程的数值，包含已加载模块与测试场景，不是插件的独占开销。

3.6.3 不识别新增灯光/FX 轨；比较展示新增求值工作量，动态图像并不完全相同。两次进程的模块和保留测试场景有差异，GPU/内存数值不能归因为单一版本改动。Windows GPU 数值为各采样点最忙引擎的平均值，不将引擎使用率相加；每组有效计数器样本 9 个。没有测量瓦数，不作节能比例结论。本轮未运行录制或推流编码压力测试。

3.6.3 基线 DLL：`374B284C6509D1E36A7A2FB21ABF9DA86D2CB3C5567B42D21351EDB72E924B5E`。旧版原件保留。早期发现运行目录含 3.4.0 后关闭了该探索实例，其结果没有用于上表。

## 失败记录与范围限制

开发阶段保留了预设/模型的红测，以及无效 FX 访问的真实失败：`test-review-red.log`，随后修复，最终套件通过。早期编辑器测试曾因测试目标头文件、链接和 OBS 初始化缺失失败；已修正测试环境后重跑。

早期验收脚本对原始 JSON 的直接比较忽略了默认曲线手柄字段和通道排序，出现两项错误断言；重启脚本首次使用系统 GBK 读取 UTF-8 也失败。失败日志均保留，后续脚本明确编码并规范化比较，最终使用新目录重新验证。没有改写旧失败日志。

真实 NDI 发送端缺失，协议验收保持 BLOCKED；自动迁移未提供独立可见取消入口。源码包、安装载荷和证据包由实际当前工作区生成，未创建新 Git 提交、标签或 Release。使用说明书保持不变。
