# OBS Spatial FX v3.2 构建说明

本源码包包含完整插件、shader、安装器及 QA 源码。OBS/Qt/MSVC 是外部依赖，不在安装包中分发。

实测工具链：VS2022 MSVC 19.44.35229.0 x64、CMake 4.4.5、Ninja、Qt6 开发依赖。OBS API 基于官方 OBS 32.0.0 头文件，实测加载 OBS 32.2.2 / 33.0.1；两套测试运行时 Qt 均为 6.11.1。

在 VS2022 x64 Native Tools Command Prompt 中：

    python tools/bootstrap.py
    set OBS_ROOT=F:\Program Files (x86)\obs-studio
    python tools/prepare_sdk.py "%OBS_ROOT%"
    tools\build.cmd 4
    tools\test.cmd

bootstrap 使用 tools/dependencies.json 中固定 URL 与 SHA256，下载到 third_party；需要 Python3.12+、Git 和网络。已有本地官方依赖时也可配置 -DOBS_SOURCE=官方OBS源码路径、-DCMAKE_PREFIX_PATH=Qt6开发目录，工具链使用外部 VS2022/CMake/Ninja。不要把 OBS 自带的 Qt DLL 复制到插件安装目录。

构建产物 build/OBS_Spatial_FX_V3.2_Setup.exe；插件 build/obs-spatial-fx.dll；PDB 同目录。

当前工作区可直接在 PowerShell 执行：

    & (Join-Path $PWD 'tools/build.cmd') 4
    & (Join-Path $PWD 'tools/test.cmd')

隔离 UI 回归（必须先准备 runtime/obs32 或 runtime/obs33 的合法官方 OBS 副本和独立 portable 配置）：

    ./tools/launch-v3-qa.ps1 -Major 33 -Scenario v31 -Output work/v3.2/ui

QA 驱动通过真实 Qt/OBS API 发事件和保存实际截图，不能部署到日常 OBS。GPU 回归需要仅在隔离 OBS 开放 4457 测试端口，camera-remake-hold 场景供测试桥接；运行 tests/v31_parameter_probe.py（本次参数对照）或 tests/v3_render_probe.py 和 tests/camera_remake_probe.py / tests/camera_stability_probe.py 后发送 native-command.json 的 finish action 关闭 QA 实例。测试结束关闭测试端口。

源码分发依赖与安装载荷分离；GPL2 LICENSE 见根目录。

安装包脚本：python tools/package_v32.py；安装/最终清理后运行 python tools/verify_v32.py。音频C++探针build/spatial-audio-probe.exe，夹具与详细重现脚本见docs/audio-v32.md。请阅读每个测试所需的隔离OBS配置，不能向日常运行的OBS开放QA桥接。
