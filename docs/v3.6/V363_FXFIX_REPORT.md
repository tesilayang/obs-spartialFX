# v3.6.3 FX 光影修复测试与交付报告

日期：2026-11-14。保留版本号 3.6.3。

## 交付结果

- 独立内外阴影：角度、距离、连续模糊、扩散、阴影强度、混合强度、辅助强度及两种颜色。
- 辉光两种模式支持独立颜色，柔和扩散可越出透明轮廓和素材矩形。
- 星芒支持 2／4／5／6 射线、光效颜色、目标颜色和颜色容差。仅匹配目标色且达到亮度阈值的区域生成星芒。
- 旧工程参数迁移及新增参数保存／重启恢复已验证。本机已通过安装器部署最终载荷。

## 范围与环境

只验证本次光效、参数、颜色交互和直接受影响的公共 UV、卷积、预乘合成、裁切、投影及几何遮罩。没有运行工程导入导出、缺素材告警、预设随机组合等无关全量回归。

实际环境：Windows 10 Pro 19045，AMD Ryzen 7 5800X（8 核／16 线程），AMD Radeon RX 6900 XT，驱动 32.0.21045.20006，约 31.92 GiB 内存。OBS 33.0.1，D3D11，1920×1080／60fps。WMI 的 AdapterRAM 字段不能用于认定该显卡实际显存容量。

## 结果

| 项目 | 状态 | 实际证据 |
|---|---|---|
| 原问题复现 | PASS | `red/optics-tests.json`：旧载荷中 13 项预期失败，截图留存 |
| MSVC 插件、安装器编译 | PASS | `build-final.log`：真实链接 DLL 与 Setup EXE |
| FX 模型及舞台模型单测 | PASS | `test-final.log`：2／2 CTest 通过，含新增迁移及参数往返断言 |
| 最终安装载荷光效、UI、参数检查 | PASS | `verified-optics/optics-tests.json`：70／70；均来自真实 OBS 输出或原生 Qt 控件 |
| 全局阴影、扩展接收层／遮挡层、几何遮罩 | PASS | `verified-projection/projection-tests.json`：6／6 |
| 退出重启、参数／颜色／实例关联恢复 | PASS | `verified-restart-results/restart-tests.json`：5／5；恢复后画面差异为 0 |
| 安装载荷及配置保留 | PASS | `verified-install/install-tests.json`：9／9；三个布局 DLL 和全部着色器与构建一致 |
| 实际加载来源与最终载荷哈希 | PASS | `verified-bridge/installed-path.json`、`verified-restart/installed-path.json` |
| 正常退出、隔离环境恢复 | PASS | 两次 `exit-status.json` 均为 0；`environment-restored.json`：153 个原配置文件原样恢复，QA 驱动已移除，无 4457 监听 |
| 安装包、源码、证据及 SHA256 校验 | PASS | `package-audit.json`、交付目录 `SHA256SUMS.txt` |
| Windows 11、NVIDIA、核显、长时间真实推流、功耗测量 | NOT RUN | 本次未执行 |
| OBS 32 及无关功能全量回归 | NOT RUN | 不属于本次验收范围 |

70 项检查包含：辉光两种模式的外溢和颜色；零强度／阈值端点；阴影全部控制在内外两种模式的可感知变化；舞台灯切换独立性；连续柔影；细小亮点的四档实际射线数量；目标色排除与容差范围；原生 Dock 入口；颜色实时预览、取消及确认；旋转、镜像、非对称裁切、几何遮罩；公共处理路径及 1080p60 性能。

大半径外阴影剖面最大相邻 Alpha 变化为 3；不透明源矩形辉光的外扩剖面最大变化为 3。实际图像与采样数列保存在证据包，未以生成示意图替代画面验收。

## 性能记录

测试使用本次生成的合成测试素材，不使用用户未授权素材。对 1920×1080 素材分别运行无光效基线及极限参数组合；组合为辉光半径 160、星芒长度 300／六射线、外投影距离 200／模糊 100／扩散 60。

| 场景 | 时长 | 最低 FPS | 平均帧渲染耗时 | 最高帧渲染耗时 | 新增渲染丢帧 | 预热后资源分配 |
|---|---:|---:|---:|---:|---:|---|
| 基线 | 30.14 秒 | 60.00 | 0.339 ms | 0.398 ms | 0 | 382 → 382 |
| 极限组合 | 30.09 秒 | 60.00 | 0.417 ms | 0.492 ms | 0 | 403 → 403 |

OBS 报告的平均 CPU 占用为 0.322%／0.358%，平均内存为 946.04／946.07 MiB。完整逐秒数据见 `verified-optics/performance.json`。这些数据只描述上述机器、素材和时长，未进行功耗测试。

## 实际命令

在 `E:\obs spartialFX develop` 执行：

```powershell
$env:OBS_ROOT=Join-Path $PWD 'runtime/obs33'
& tools/build.cmd 4 *>&1 | Tee-Object work/v3.6.3-fxfix/build-final.log
& third_party/cmake/cmake-4.4.5-windows-x86_64/bin/ctest.exe --test-dir build --output-on-failure -R '^spatial-fx$|^spatial-fx-stage$'
& work/v3.6.3-fxfix/install-final.ps1
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.6.3-fxfix/verified-bridge
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tests/v363_fx_optics_probe.py work/v3.6.3-fxfix/verified-optics work/v3.6.3-fxfix/verified-bridge --full
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tests/v363_optics_global_probe.py work/v3.6.3-fxfix/verified-projection work/v3.6.3-fxfix/verified-bridge
& work/v3.6.3-fxfix/finish-obs.ps1 -Bridge work/v3.6.3-fxfix/verified-bridge
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.6.3-fxfix/verified-restart
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tests/v363_optics_restart_probe.py work/v3.6.3-fxfix/verified-restart-results work/v3.6.3-fxfix/verified-restart work/v3.6.3-fxfix/verified-optics
& work/v3.6.3-fxfix/finish-obs.ps1 -Bridge work/v3.6.3-fxfix/verified-restart
& work/v3.6.3-fxfix/restore-environment.ps1
git -c core.whitespace=cr-at-eol diff --check
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tools/package-v363-fxfix.py
```

## 载荷

- 插件 DLL SHA256：`46a80f750b91236301fd2063b1cf1eb01ce9a3b0428d2154803c145ac702bb94`
- 安装包 SHA256：`2ea32a78606b0c471bb50549f35cd87089e30093621f3b7435c1ebcebf8d8e62`
- 详细修改文件、算法、坐标换算和迁移规则见 `V363_FXFIX_IMPLEMENTATION.md`。
- 真实 OBS 截图总览：`verified-optics/optics-contact-sheet.png`；界面截图见 `verified-bridge/fx-panel-all.png`、`verified-restart/fx-restarted.png`。
- 原始交付目录不覆盖；说明书不更新；本次未提交、推送或发布 GitHub。
