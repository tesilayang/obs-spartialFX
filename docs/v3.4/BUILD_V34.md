# v3.4 构建与验证记录入口

本文件用于开发交付，不改动旧版使用说明书。

工程根目录：`E:\obs spartialFX develop`。使用 VS2022 MSVC x64、Qt6、OBS32 官方头文件，源码与 MSVC 编译均使用 UTF-8。依赖固定版本及下载校验保存在 `tools/dependencies.json`，准备方式仍为 `tools/bootstrap.py` 与 `tools/prepare_sdk.py`。

PowerShell 实际构建命令：

```powershell
& tools/build.cmd 4
& tools/test.cmd
```

输出：`build/obs-spatial-fx.dll`、`build/obs-spatial-fx.pdb`、`build/OBS_Spatial_FX_V3.4_Setup.exe`。完整编译日志为 `work/v3.4-development/build14.log`，CTest 最终日志为 `work/v3.4-development/ctest-final.log`。

真实安装测试：

```powershell
& tools/test-installer-v34.ps1
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34 -Output work/v3.4-development/installed-ui33 -Wait
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-persist -Output work/v3.4-development/installed-persist33 -Wait
& tools/run_v33_installed_qa.ps1 -Major 32 -Scenario v34 -Output work/v3.4-development/installed-ui32 -Wait
& tools/run_v33_installed_qa.ps1 -Major 32 -Scenario v34-persist -Output work/v3.4-development/installed-persist32 -Wait
```

GPU 测试只对隔离 OBS 运行。先用 `tools/launch-v3-qa.ps1` 的 `v34-hold` 场景启动，再在隔离配置中临时开启 4457 测试端口。执行：

```powershell
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tests/v34_render_probe.py work/v3.4-development/gpu32-final work/v3.4-development/gpu32-final-bridge
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tests/v34_coordinate_probe.py work/v3.4-development/gpu32-final work/v3.4-development/gpu32-final-bridge
```

OBS33 使用对应的 `gpu33-final` 和 `gpu33-final-bridge`。截图缩小至640×360传输；真实源纹理与舞台均为1920×1080，运行60fps。测试桥接、QA模块与测试端口均不属于安装载荷。

渲染按子效果延迟编译，每图层复用两个处理表面；历史效果单独保留原始历史与显示混合结果。禁用的组/子项不进入处理链。素材引用取原始输入，避免效果之间递归引用。所有 GPU 创建/销毁在图形上下文内，渲染不等待UI线程。

确认目录与实现对应关系：`docs/v3.4/V34_IMPLEMENTATION_MAP.json`。原始选择文件、旧版说明书与v3.3安装包保持原样。
