from pathlib import Path
import json,re,hashlib
root=Path('.')
j=json.loads((root/'logs/release-preset-apply/dock-smoke.json').read_text(encoding='utf-8'));assert j['failed']==0
last=max((root/'runtime/obs32/config/obs-studio/logs').glob('*.txt'),key=lambda p:p.stat().st_mtime);(root/'logs/release-preset-apply-obs-runtime.log').write_bytes(last.read_bytes())
p=Path('tests/audit_release.py');s=p.read_text(encoding='utf-8-sig');s=s.replace("'logs/phase3/dock-smoke.json',","'logs/release-preset-apply/dock-smoke.json','logs/phase3/dock-smoke.json',");p.write_text(s,encoding='utf-8')
perf=json.loads((root/'logs/release-installed-performance/performance.json').read_text(encoding='utf-8'))
native=json.loads((root/'logs/release-installed-performance/dock-smoke.json').read_text(encoding='utf-8'))
sha=hashlib.sha256((root/'build/OBS_Spatial_FX_V1.0_Setup.exe').read_bytes()).hexdigest()
text=f'''# OBS Spatial FX 1.0 验收报告

日期：2026-10-30。本报告只对实际执行的项目标记 PASS。发布安装器 SHA256：

    {sha}

## 交付与实测环境
- 产品：Windows x64 C++ / Qt6 OBS 内嵌 Dock + 自定义视频合成源。
- 实测：Windows10 22H2 build19045、AMD Ryzen7 5800X、Radeon RX6900XT、驱动32.0.21045.5002。
- 宿主：OBS32.2.2，实际 Qt runtime6.11.1。头文件 API 基线 OBS32.0.0。
- 工具链：VS2022 MSVC14.44.35207 / 编译器19.44.35229、SDK10.0.22621.0、CMake4.4.5、Ninja。
- 安装：C:/ProgramData/obs-studio/plugins/obs-spatial-fx；真实系统安装和卸载已执行，最终保留正常安装状态。
- 安装器未签名。没有将签名、Win11 或 NVIDIA 未执行项标为通过。

## 分阶段结果
| 阶段 | 状态 | 真实证据 |
|---|---|---|
| Phase0 环境/官方空模块 | PASS | logs/phase0-build.log、phase0-obs-runtime.log；官方 template 固定提交 |
| Phase1 原生五标签 Dock | PASS | phase1/dock-smoke.json，9/9；真实 QWidget/QDockWidget 截图 |
| Phase2 空间/摄像机 | PASS | phase2/render-probe.json，6/6；数学32断言、模型11断言；实际纹理与视差截图 |
| Phase3 动画/命名预设/重启 | PASS | phase3 原生25/25、phase3-restart 原生14/14；键帧和 camera_x=177 重启后保留 |
| Phase4 灯光/Alpha阴影/FX | PASS | phase4 原生27/27、features.json 6/6；灯光、阴影、单色、动画像素对比 |
| Phase5 生命周期/性能 | PASS | phase5-lifecycle-fixed 28/28；最终已安装版本60秒1080p60计数 |
| Phase6 原生 EXE/部署 | PASS | installer-tests.json 11/11；常规版、便携快捷方式实际加载；release-audit.json |

## 最终已安装版本的性能
- 测量窗口 {perf['durationSeconds']:.3f} 秒，实际1920×1080 / 60fps。
- OBS renderTotalFrames 增量 {perf['frames']}；renderSkippedFrames 增量 {perf['renderLagFrames']}。
- 60次采样 activeFps 均值 {perf['meanFps']:.6f}。
- 各次 OBS“平均帧渲染耗时”采样中的最大值 {perf['maxReportedAverageRenderMs']:.6f}ms。这是平均值采样的最大值，不是单帧峰值，也不是 GPU 时间戳测得的 GPU 执行时间。
- 场景：3张透明纹理平面，循环摄像机关键帧、点光/阴影/暗角，并反复发布设置快照。
- 完整该次生命周期累计5296次 stage render，捕获表面创建计数固定3；正常卸载 effect/纹理、正常退出。
- Dock 原始统计：{native['performanceLabel'].replace(chr(10),' / ')}。
- 证据：logs/release-installed-performance/performance.json、performance-render.png、dock-smoke.json，以及 release-installed-performance-obs-runtime.log。

## 安装和配置验收
- 原生安装器真实执行 SILENT 安装，部署到 ProgramData、注册 Windows 卸载项、生成便携启动快捷方式。
- 锁定已拥有的 effect 文件诱发更新失败，核对全部原有文件字节恢复，实际错误与回滚日志保留。
- 实际运行安装后的自卸载器及其临时辅助进程；确认 DLL/卸载注册项删除，未知文件与 OBS 配置保留；随后成功重装。
- OBS运行时，安装与卸载均返回10，DLL哈希不变。自动识别运行中的深层便携目录，并正确将 OBS29 标为不支持。
- 常规版：F:/Program Files (x86)/obs-studio new 的真实 OBS32.2.2，从系统插件目录加载；真实面板14/14、实际渲染6/6，正常退出。
- 常规版只使用专用 QA 场景/配置夹具。原 user.ini/global.ini 已恢复，前后哈希一致。证据 normal-config-restoration.json。
- 便携版：实际启动安装器生成的开始菜单快捷方式；加载安装目录中的同一发布 DLL，原生14/14及性能3/3检查通过；正常退出。
- 命名预设取用追加验证：release-preset-apply/dock-smoke.json，{len(j['checks'])}/{len(j['checks'])}；先把摄像机改为999，再点击真实“应用预设”按钮还原177，确认图层/场景 UUID 不变。
- 安装资源与 build DLL/effect/README/LICENSE 逐字节相等；最终安装目录没有 QA 模块，没有 Qt DLL。
- 安装器自身仅依赖 Windows DLL；插件依赖宿主 OBS、Qt6 和 MSVC 运行库。二进制导入表见 release-binary-inspection.log。

## 真实命令
在工作区 E:/obs spartialFX develop 执行：

    cmd.exe /d /c tools\\build.cmd 0
    cmd.exe /d /c tools\\build.cmd 1
    cmd.exe /d /c tools\\build.cmd 2
    cmd.exe /d /c tools\\build.cmd 3
    cmd.exe /d /c tools\\build.cmd 4
    cmd.exe /d /c tools\\test.cmd
    python tests/runtime_probe.py logs/phase6-installed-normal
    python tests/feature_probe.py logs/phase4
    python tests/performance_probe.py logs/release-installed-performance
    powershell -File tools/test-installer.ps1
    powershell -File tools/test-installed-normal.ps1
    powershell -File tools/test-installed-portable.ps1
    python tools/bootstrap.py --verify-only
    python tests/audit_release.py

实际工作区 Python 来自本机打包运行时；脚本/工具调用记录和编译器完整命令在日志中。阶段脚本最后会调用 cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSPATIAL_PHASE=对应阶段，以及 cmake --build build --parallel 6。

## 本次修改的核心文件
CMakeLists.txt；src/plugin.cpp、dock.cpp、dock_actions.cpp、dock.hpp、core.cpp/.hpp、model.cpp/.hpp、stage.cpp/.hpp、obsconfig.h.in；data/effects/spatial.effect；installer/setup.cpp、setup.rc.in、Detect-OBS.ps1；tests/；tools/ 构建/验收脚本；README、架构/构建/测试文档。

## 失败记录与修复
- 数学测试先有11条预期失败，随后32/32通过；红/绿日志均保留。
- 修正 OBS32 已移除的旧 scene transform API，统一 info2；严格废弃警告作为编译错误处理。
- 删除源时即使还有强引用，也应视为已删除：UI和 weak-source render 路径补上 obs_source_removed，生命周期复测28/28。初次失败日志未改成通过。
- 接收背景首次绘制前初始化全部 uniform，最终运行不再出现“Not all shader parameters were set”。
- 常规 QA 第一次迁移便携场景时素材相对路径无效；该次纹理/视差检查失败。修正测试夹具绝对路径后重跑全部通过；失败目录 phase6-installed-normal-failed 保留。
- 卸载器最初用不同分隔符的路径字符串比较自身，导致自删除失败；改用 fs::equivalent 后重新真实自卸载/重装通过。installer-tests-failed.log 保留。
- 早期测试驱动在 OBS启动后才复制，存在枚举竞态；改为启动前复制。有两次不含自动退出驱动的隔离测试进程在记录证据后终止，未将其标为正常退出通过。见 harness-race-note 和 installed-portable-first-run-note。后续正式验收均正常退出。

## NOT RUN / 已知边界
- NVIDIA 实机、Win11、Win10 1909、所有 OBS32 小版本、所有采集设备：NOT RUN；没有在缺少硬件时声称通过。
- 多小时持续压力、完整4K性能、复杂嵌套场景与设备热拔插：NOT RUN。
- 安装器 GUI 的鼠标逐步点击：NOT RUN。本次系统安装/卸载以同一真实 Win32 EXE 的 SILENT 路径完成；没有将未执行的鼠标操作写作 PASS。
- OBS allocator 退出计数报告1条泄漏；完全不加载本插件的同一 OBS基线也报告1条。不能据此宣称“零泄漏”，本次未定位该基线分配的来源。证据 phase5-no-product-baseline.log 对照最终运行日志。
- V1为最多32平面的中心深度排序和单点 Alpha 轮廓阴影；无相交网格深度缓冲、柔影、空间音频。嵌套/裁剪/边界/镜像源导入会明确跳过；直接删除舞台前须先用还原按钮恢复原项。

## 证据真实性
所有截图来自真实运行：Dock/main 图由 OBS内真实 QWidget::grab 保存。该方法不能捕获 D3D 预览，因此 main 图预览可能为黑；GPU输出单独通过真实 OBS GetSourceScreenshot 保存，未合成或伪造窗口预览。QA是独立测试模块，不在交付安装器中。
'''
(root/'docs/TEST_REPORT.md').write_text(text,encoding='utf-8')
print('Report generated; preset checks',len(j['checks']))
