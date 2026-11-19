# v3.6.5 窗口捕捉可见性修复

日期：2026-11-16。版本保持 3.6.5；本次交付保存到 `dist/v3.6.5-window-capture`，原已发布包保持原样。

## 原因

窗口捕捉采集正常，导入后的深度分配错误使它被不透明背景遮挡。

旧导入使用 `(当前场景素材总数 - 索引 - 1) × 180`，每次导入重新计算。分批导入之间增加或删除其他素材，会让新背景位于旧前景之前。实际保存的场景中，窗口 Z=1260，背景 Z=1080，桌子 Z=720；Z 越大越远，因此角色完全被背景遮挡。

修复前在隔离 OBS 中稳定复现：先导入窗口，移除两条未导入素材，再导入背景，得到窗口 Z=360、背景 Z=180。窗口仍为 `videoShowing=true`，输出像素完全等于背景色。

## 修改

- `src/import_depth.hpp`：按 OBS 场景项层级与已有导入实例确定新对象深度；支持相邻层插值、单边锚定、重复实例与近裁剪余量。
- `src/dock_actions.cpp`：使用统一场景求值获取已有层世界位置，按场景 UUID、场景项 ID、来源 UUID 匹配；新图层继续通过 `importPlane()` 保持屏幕位置和尺寸。
- `tests/core_tests.cpp`：加入七项深度分配边界与排序检查。
- `tests/v365_import_order.py`：加入真实 OBS 导入、遮挡、重复实例和采集生命周期回归。
- `tests/v365_window_capture.py`：加入 BitBlt 与 Windows Graphics Capture 的真实窗口捕捉检查。
- `docs/architecture/SPATIAL_COORDINATE_CONTRACT.md`：固定分批导入深度规则。

本机受影响窗口没有动画、环绕或手动坐标修改，其位置与保存的导入默认值完全一致。备份场景后，将这条窗口记录移至 Z=900，位置和缩放按 `2400/2760` 补偿，并同步导入默认值。语义比对确认仅修改该记录的 X/Y/Z、缩放及五个对应默认值。原配置备份为 `work/v3.6.5-window-capture/before/personal-scene.json`；用户配置与素材不打入交付包。

## 实际命令

以下命令均已执行。Python 使用本机 Codex runtime 的 Python，CTest 使用仓库固定的 CMake 4.4.5。

```powershell
cmd /c "tools\build.cmd 4 > work\v3.6.5-window-capture\build.log 2>&1"
third_party/cmake/cmake-4.4.5-windows-x86_64/bin/ctest.exe --test-dir build -R '^(spatial-core|spatial-scene-evaluator)$' --output-on-failure
tools/run_v33_installed_qa.ps1 -Scenario v34-hold -Output work/v3.6.5-window-capture/final-bridge
python tests/v365_import_order.py work/v3.6.5-window-capture/final-bridge work/v3.6.5-window-capture/order-delivery
python tests/v365_window_capture.py work/v3.6.5-window-capture/capture-final
python work/v3.6.5-window-capture/verify_saved_layout.py
python work/v3.6.5-window-capture/repair_saved_layout.py
tools/run_v33_installed_qa.ps1 -Scenario v34-hold -Output work/v3.6.5-window-capture/restart-bridge
python work/v3.6.5-window-capture/restart_check.py after
work/v3.6.5-window-capture/cleanup.ps1
```

窗口捕捉脚本使用输出目录中的 `bridge` 目录链接连接 QA 桥；链接目标为 `final-bridge`。安装后验证通过系统安装器的启动入口加载 ProgramData 中的最终 DLL。

## 测试结果

环境：Windows 10 22H2，Ryzen 7 5800X、RX 6900 XT、32GB；OBS 33.0.1 隔离便携环境，1920×1080、60fps。测试范围为导入深度、窗口采集及其直接关联的重复实例、保存与重启。

| 状态 | 项目 | 证据 |
|---|---|---|
| FAIL | 原载荷分批导入复现：窗口被背景覆盖 | `work/v3.6.5-window-capture/order-red/checks.json`、`imported-stage.png` |
| PASS | 真实编译插件与安装器 | `build.log` |
| PASS | core 与统一场景求值两项 CTest | `ctest.log` |
| PASS | 安装载荷路径、DLL 与着色器校验 | `final-bridge/installed-path.json` |
| PASS | 十一项导入与采集生命周期回归 | `order-delivery/checks.json` |
| PASS | 修复后窗口屏幕位置与颜色：三个采样点误差均为 0 | `order-delivery/checks.json`、`captured-window.png`、`imported-stage.png` |
| PASS | BitBlt 与 WGC：隐藏原项后仍采集并显示 | `capture-final/results.json`、对应源与舞台截图 |
| PASS | 实际 LAPLACE Persona：按原始错误深度不可见，修正后可见 | `saved-layout/verification.json`、前后截图 |
| PASS | 保存模型、重启后实际窗口可见、采集显示状态、GPU 错误计数四项 | `saved-layout/restart-checks.json`、`after-restart.png` |
| PASS | 当前场景记录备份、修复及语义差异校验 | `saved-layout/repair.json` |
| PASS | 正常退出测试 OBS，恢复 153 个配置文件与 19 个原插件文件，移出 QA 驱动，关闭 4457 端口 | `cleanup.json` |
| NOT RUN | 专项性能与功耗测量 | 本次修改仅发生在导入操作，渲染管线保持原样 |

路径未带前缀时均相对 `work/v3.6.5-window-capture/`。测试工具初次把金色采样区误判成蓝色，随后改为与真实来源同位置像素对照；初次重启检查引用了未导出的 `missing` 指标，随后改为实际来源显示状态与已导出的 GPU 错误计数。原失败记录分别保留在 `order-green/checks.json`、`saved-layout/restart-checks-first.json`。

实际应用截图与个人配置仅保留在本机 work 目录。公开交付证据仅包含自制窗口、背景及日志。未运行与本次修改无关的功能回归，未更新使用说明书，未发布或替换 GitHub Release。

## 最终载荷 SHA256

```text
obs-spatial-fx.dll
8EBC2C60A0A954ED7381812B09965CC7D3F7F2CD9DD8C55C34D030632E71BA7C

OBS_Spatial_FX_V3.6.5_Setup.exe
E12F41C768F92522D0265246D99EF5EA906933347D2D3D1C6BBDBBE27B8AC474
```

完整文件及着色器哈希见交付目录的 `PAYLOAD_MANIFEST.json` 和 `SHA256SUMS.txt`。
