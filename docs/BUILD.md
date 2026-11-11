# Build / 真实复现

基准：Windows x64，VS2022 14.44，Windows SDK 10.0.22621.0，Python3.12+，OBS32+。源码及 CMake 使用 UTF-8；编译选项包含 /utf-8、/W4、/permissive-、/we4996。SDK 头文件固定 OBS32.0.0，实际链接库由本机 OBS32+ 导出表生成；不会复制或修改 OBS DLL。

## 新设备
1. 安装 VS2022 的“使用 C++ 的桌面开发”、Windows SDK、Python3.12+、Git。
2. 在 VS2022 x64 Native Tools 命令行进入工程目录。
3. 执行 python tools/bootstrap.py。它只从官方 GitHub/API/codeload 获取固定哈希的 OBS、Qt6 开发包、CMake、Ninja 和固定提交的 obs-plugintemplate。锁文件 tools/dependencies.json。
4. 设置 OBS_ROOT 为 OBS 安装根目录（包含 bin/64bit/obs64.exe）。不设置时 tools/build.cmd 会自动检测。
5. 执行 tools\build.cmd 4；执行 tools\test.cmd。
6. 产物 build/obs-spatial-fx.dll、build/OBS_Spatial_FX_V2.0_Setup.exe。QA 驱动虽独立构建，但绝不进入安装器资源或安装规则。

当前工作区已包含独立 MSVC 工具链；tools/build.cmd 优先使用 msvc/setup_x64.bat。在其他设备也可直接使用已安装 VS2022 的开发者命令行。若 Python 不在 PATH，可设置 PYTHON 环境变量为解释器完整路径。

## 等效 CMake
在 VS2022 x64 Native Tools 命令行：

    python tools\prepare_sdk.py "D:\Apps\obs-studio"
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DSPATIAL_PHASE=4 -DOBS_ROOT="D:\Apps\obs-studio"
    cmake --build build --parallel 6
    ctest --test-dir build --output-on-failure

Qt6 默认从 third_party/qt6 查找；其他 SDK 可设置 CMAKE_PREFIX_PATH。禁止改为 Qt5，禁止将 Qt DLL 或 OBS 自带文件打入包。安装器使用 /MT，只依赖 Windows 系统库；插件使用 OBS 已有的 Qt6 和 MSVC 运行库。

## 分阶段命令与日志
- Phase0：tools\build.cmd 0，真实官方 template 模块，logs/phase0-build.log。
- Phase1：tools\build.cmd 1，logs/phase1-build.log。
- Phase2：tools\build.cmd 2，logs/phase2-build.log、phase2-unit-tests.log。
- Phase3：tools\build.cmd 3，logs/phase3-build.log、phase3-unit-tests.log。
- Phase4：tools\build.cmd 4，logs/phase4-build.log、phase4-unit-tests.log。
- Phase5：tools\build.cmd 4，logs/phase5-final-build.log。
- Phase6 / 发布：tools\build.cmd 4，logs/release-build.log、release-unit-tests.log、release-binary-inspection.log。

阶段宏只用于开发隔离，发布固定 Phase4（所有功能启用）。不要把早期阶段 DLL 当作交付物。

图像对比测试另需 Pillow（python -m pip install Pillow）；C++ 构建和标准库 WebSocket 客户端不依赖 Pillow。

## 测试工具
- tests/core_tests.cpp、model_tests.cpp：投影/旋转/缓动/插值/阴影/边界及 OBS settings 序列化。
- tests/obs_qa_driver.cpp：独立测试模块，在真实 OBS 内操作真实 Qt 控件，截图与断言，正常关闭 OBS。不会替代产品逻辑。
- tests/obs_client.py：仅依赖标准库的本地 OBS WebSocket v5 测试客户端。
- tests/runtime_probe.py、feature_probe.py、performance_probe.py：实际 GPU 输出、视差、动画、灯光、FX、60秒采样。
- tools/test-installer.ps1：需要管理员权限；仅对本插件系统目录验证回滚/卸载/重装。
- tools/test-installed-normal.ps1：本机验收工具，使用专用 QA 场景，先备份相关常规版配置，退出后恢复并核对哈希。
- tools/test-installed-portable.ps1：启动实际生成的便携版快捷方式；临时测试驱动在结束后删除。
- 上述集成脚本中的本地 OBS 路径是本设备测试夹具，迁移设备时按实际环境调整。它们不是安装器依赖。

测试 WebSocket 端口4457只用于隔离 QA；交付结束时测试进程关闭，测试配置的服务禁用。用户正常 OBS 配置不会被改为无认证。
