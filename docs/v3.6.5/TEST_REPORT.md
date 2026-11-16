# v3.6.5 专项测试报告

日期：2026-11-16。系统：Windows 10 22H2 x64；CPU Ryzen 7 5800X，GPU Radeon RX 6900 XT，内存 32GB。运行验证：隔离 OBS 33.0.1，1920×1080 / 60fps。构建：VS2022/MSVC、C++17、Qt6。

## 范围与结果

只检查 FX 页刷新、FX 控件关联、共用悬停说明及退出清理。共享提示代码涉及镜头、灯光、预设，因此检查这三页的提示。未改动渲染算法、工程格式和安装布局。

| 检查 | 状态 | 证据 |
|---|---|---|
| 真实 MSVC 构建 | PASS | `build-delivery.log`、`build-package.log` |
| 滚动 / 控件身份 / 外部 FX 删除 / AA 互斥 | PASS | `tests-delivery.log`，`spatial-fx-panel-scroll` |
| 一秒提示 / 窗口边界 / 应用失活取消 / 末根销毁 | PASS | `tests-delivery.log`，`spatial-hover-help`，Windows Qt 平台 |
| 预设面板及编辑器关联回归 | PASS | 同一日志；共四组 CTest，0 失败 |
| 最终安装 DLL 加载 | PASS | `delivery-bridge/installed-path.json`，加载路径位于 ProgramData 的 portable33 布局 |
| 真实 OBS 交互 22 项 | PASS | `delivery-acceptance/acceptance.json` |
| 底部 FX 连续开关 8 次 | PASS | 滚动位置每次均为 1889，点击项保持在视窗中 |
| 恢复效果默认值后的重建 | PASS | 滚动位置 1889→1889，`after-effect-reset.png` |
| FX 数值实际改变 GPU 输出 | PASS | 原图 / 调色图；RGB 平均差合计 21.5255 |
| FX、镜头、灯光、预设悬停 | PASS | `hover-help.png`、`timeline-help.png`、`light-help.png`、`preset-help.png`；普通子 QLabel，位于所属 OBS 窗口内 |
| 原生提示窗口枚举 | PASS | 最终检查仅 OBSBasic 主窗口可见，无 Qt::ToolTip 窗口 |
| 正常退出及配置恢复 | PASS | `cleanup.json`；153 个配置文件、19 个运行目录插件文件按 SHA256 恢复，QA 移除，4457 监听关闭 |
| 原始“Codex 输入时整个桌面白蒙版”的确定性复现 | BLOCKED | 本轮未稳定复现原始症状。旧代码原生 tooltip 的创建已捕获；不能将其等同于已捕获桌面蒙版 |
| 直接自动操作 Codex 编辑区重现输入 | NOT RUN | 未执行此项；实际外部文字输入使用记事本替代，记录在 `external-typing.json`，提示隐藏、无原生提示窗口 |
| 工程导入导出、缺素材、OBS32、全量性能及编码压力 | NOT RUN | 不属于本次 UI 修复范围 |

## 基线与中间失败

`tests-baseline.log` 两项均失败：本地切换 FX 后原控件被删除；Windows 创建原生提示窗口。基线最小 Qt 场景滚动值没有跳到零，实际记录为 16018→16018、widgetAlive=0。

`tests-final.log`、`tests-trace.log` 保留早期控制器退出崩溃，真实栈指向 QApplication 销毁时的 removeEventFilter。随后修复并增加销毁断言。最终 `tests-delivery.log` 四组通过。

早期 OBS 检查包含测试场景错误：渐变覆盖尚未关闭，遮住了前序调色效果；在合成应用失活事件后继续检查悬停，且镜头按钮在视窗外。脚本改为隔离测试条件、先检查真实可见按钮、最后测试失活，再在新目录运行。失败文件未改写；最终结果采用 `delivery-acceptance`。

桌面抓图接口曾返回 `FrameArrived timed out`；实际界面证据使用 OBS 内 Qt 控件截图和源截图。未将该接口故障当作通过。原始桌面蒙版未捕获，修复作用为消除插件的原生提示窗口路径、重复控制器及后台延迟提示。

## 实际命令

```powershell
cmd /c "tools\build.cmd 4 > work\v3.6.5\build-delivery.log 2>&1"
& third_party/cmake/cmake-4.4.5-windows-x86_64/bin/ctest.exe --test-dir build -R 'spatial-fx-panel-scroll|spatial-hover-help|spatial-preset-panel|spatial-editor-timeline' --output-on-failure
Start-Process build/OBS_Spatial_FX_V3.6.5_Setup.exe -ArgumentList '/SILENT','/LOG="E:\obs spartialFX develop\work\v3.6.5\install-delivery.log"' -Wait
& tools/run_v33_installed_qa.ps1 -Scenario v34-hold -Output work/v3.6.5/delivery-bridge
& 'C:\Users\admin\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' tests/v365_acceptance.py work/v3.6.5/delivery-bridge work/v3.6.5/delivery-acceptance
& tests/v365_cleanup.ps1
cmd /c "tools\build.cmd 4 > work\v3.6.5\build-package.log 2>&1"
Start-Process build/OBS_Spatial_FX_V3.6.5_Setup.exe -ArgumentList '/SILENT','/LOG="E:\obs spartialFX develop\work\v3.6.5\install-package.log"' -Wait
```

最后一次打包仅因 README 的本版报告入口更新而重编译安装器资源；DLL 和着色器与最终 OBS 验证时完全一致。再次安装后核对载荷哈希一致。合成测试图已随源码保存为 `tests/fixtures/fx-ui.png`，与实际测试所用图逐字节相同。

## 最终载荷

- DLL SHA256：`63E7C3E26BC5794FF235FF48FFB71B23BBFAC5009C8F0A5F9582AF911116A0CF`
- 安装包 SHA256：`6087BEE2E4B7CA9CA7E4A4B10A5A21CFD77549D418C2B5853A8C436D8992944F`
- 源码、着色器和全部交付文件哈希由 `RELEASE_MANIFEST.json`、`PAYLOAD_MANIFEST.json` 和 `SHA256SUMS.txt` 关联。
