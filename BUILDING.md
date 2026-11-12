# OBS Spatial FX 构建说明（当前 v3.4.2）

开发、测试和发布必须执行 [项目开发与交付标准](DEVELOPMENT_STANDARDS.md)。

本源码包包含完整插件、shader、安装器及 QA 源码。OBS/Qt/MSVC 是外部依赖，不在安装包中分发。

实测工具链：VS2022 MSVC 19.44.35229.0 x64、CMake 4.4.5、Ninja、Qt6 开发依赖。OBS API 基于官方 OBS 32.0.0 头文件，兼容性验收只运行 OBS 33.0.1，运行时 Qt 为 6.11.1。

在 VS2022 x64 Native Tools Command Prompt 中：

    python tools/bootstrap.py
    set OBS_ROOT=F:\Program Files (x86)\obs-studio
    python tools/prepare_sdk.py "%OBS_ROOT%"
    tools\build.cmd 4
    tools\test.cmd

bootstrap 使用 tools/dependencies.json 中固定 URL 与 SHA256，下载到 third_party；需要 Python3.12+、Git 和网络。已有本地官方依赖时也可配置 -DOBS_SOURCE=官方OBS源码路径、-DCMAKE_PREFIX_PATH=Qt6开发目录，工具链使用外部 VS2022/CMake/Ninja。不要把 OBS 自带的 Qt DLL 复制到插件安装目录。

构建产物 build/OBS_Spatial_FX_V3.4.2_Setup.exe；插件 build/obs-spatial-fx.dll；PDB 同目录。

当前工作区可直接在 PowerShell 执行：

    & (Join-Path $PWD 'tools/build.cmd') 4
    & (Join-Path $PWD 'tools/test.cmd')

隔离 UI 回归（必须先准备 runtime/obs33 的合法官方 OBS 副本和独立 portable 配置）：

    ./tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v342 -Output work/v3.4.2/ui-contributor

此入口核对已经安装的插件与本次构建 DLL/资源是否一致，需要先将自己的构建部署到独立测试环境。QA 驱动通过真实 Qt/OBS API 发事件和保存实际截图，不能部署到日常 OBS。GPU 回归需要仅在隔离 OBS 开放 4457 测试端口，v34-hold 场景供测试桥接；完整命令见 docs/v3.4.2/V342_REPORT.md。测试结束发送 native-command.json 的 finish action 正常关闭测试实例，并清理测试驱动与端口。

源码分发依赖与安装载荷分离；GPL2 LICENSE 见根目录。

旧 tools/package_v32.py、tools/verify_v32.py 仅用于历史版本复现。当前安装器测试入口为 tools/test-installer-v342.ps1；该脚本会在开发测试机执行真实安装/升级/卸载，运行前阅读源码，不要在日常直播系统直接执行。音频 C++ 探针为 build/spatial-audio-probe.exe，夹具说明见 docs/audio-v32.md。

## 公开仓库与固定版本交付

当前 GitHub v3.4.1 Release 的原始 Source.zip 保留发布时的工程快照，安装器与该快照均保持原 SHA256。仓库 main 可以继续更新文档；固定版本 HTML 说明书见 docs/v3.4.1/manual/。仓库中的测试证据链接指向开发工作区时，请改从 Release 下载 Evidence.zip 查阅；这些缓存不存入 Git 历史。新克隆工程的依赖准备受上游下载可用性和本机工具链影响，本轮验证针对现有开发机，不等于已在所有干净系统复现。

当前打包入口是 tools/package_v342.py，它要求本机完整验收证据，不是一次普通编译后就能无条件运行的通用发行脚本。普通开发先使用 tools/build.cmd 4 与 tools/test.cmd。
