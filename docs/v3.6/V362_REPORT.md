# OBS Spatial FX v3.6.2 开发与交付报告

日期：2026-11-14。验收环境：OBS 33.0.1。状态：开发、真实编译、安装载荷验证和测试环境恢复已完成。

## 1. 本版修复

### 新增灯光的位置

点击“添加点光源”或“添加面光源”时，读取当前镜头、当前动画时刻和正在运行的 FX 时钟，复用 `evaluateSceneAt()` 求值。根据启用且可见的导入图层的裁剪边界，将新灯光放在构图中心、最近图层四角之前，并留出安全间隔。旋转镜头、倾斜和镜像图层、视差补偿、动画、动态源尺寸与边界框均纳入计算。

构图完全偏出画面时，灯光中心限制在画面内侧 80%；没有有效素材时放在镜头前 500 世界单位。已有灯光位置保持原值，配置格式不变。

### 软阴影

已复现的核心问题是环境补光绕过遮挡。旧版实际 Alpha 投影与软阴影半径能够工作，但较高的环境补光会显著冲淡阴影。修复后，每盏灯的补光、直射和轮廓光接受该灯自己的遮挡，其他灯的补光仍独立。环境补光总上限保留为 2；补光和直射分别调色，保留高饱和度下的未遮挡画面。

真实 GPU 复现使用环境补光 0.83、直射强度 0.12：相同 ROI 的 RGB 绝对差均值之和从旧版 **12.23** 提升到 **321.70**，阴影从难以辨认变为明确可见。这是图像差值，不能作为通用视觉评分。

仍按空间几何投影：投影素材必须位于光源与接收平面之间。只读诊断发现，截图场景中的人物 Z=900、桌面 Z=720；在前方光源下，人物不能向位于自己前方的桌面投影。需要这类人物落桌影时，应将接收平面放到人物后方，或调整灯光和素材的相对关系。

### 灯光页布局

参数行实测由约 **42px** 缩至 **30px**，减少约 **28.6%**。数值框仍支持 0.01 步进和手动输入；灯光颜色、阴影颜色保留完整色值预览。其他页面的参数行不受影响。

### 安装器复验发现的问题

故障回滚后的恢复目录只使用进程编号命名，编号重用可能阻塞下一次安装。本版增加运行计时后缀，避免与旧恢复目录重名，保留已有恢复文件。

## 2. 修改文件

| 文件 | 实际修改 |
|---|---|
| `src/scene_evaluator.hpp/.cpp` | 新灯光定位；复用当前时间、FX 时钟、实际源尺寸和最终空间位姿 |
| `src/dock_v2.cpp` | 新建点光、面光调用统一定位入口 |
| `src/stage.hpp/.cpp` | 发布 FX 时钟；逐灯补光与阴影合成，保留补光上限 |
| `data/effects/spatial.effect` | 遮挡本灯补光；补光与直射分别调色 |
| `src/dock.cpp` | 灯光页紧凑布局与版本展示 |
| `src/plugin.cpp`、`CMakeLists.txt` | v3.6.2 版本与输出名 |
| `installer/setup.cpp`、`installer/setup.rc.in` | 安装版本、资源版本、唯一暂存目录 |
| `tests/scene_evaluator_tests.cpp` | 镜头、动画、FX 时钟、裁剪、边界框及异常位置回归 |
| `tests/v361_ui_bridge.inc` | QA 控件记录增加位置，用于实测行距 |
| `tests/v362_light_probe.py` | 真实灯光图像、原生按钮、性能和连续修改验证 |
| `tests/v362_light_grade_probe.py` | 高饱和度未遮挡合成的 GPU 回归 |
| `tests/v362_restart_probe.py` | 保存、进程重启及图像一致性验证 |
| `tools/test-installer-v362.ps1`、`tools/package-v362.py` | 覆盖升级测试、证据门槛和交付校验 |
| `docs/architecture/SPATIAL_COORDINATE_CONTRACT.md` | 新灯光位置与逐灯补光规则 |
| `docs/v3.6/V362_IMPLEMENTATION.md`、本报告 | 开发记录与验收结果 |

沿用工作区已有的 v3.6.1 成果，未提交或推送 GitHub，未更新使用说明书。v3.6.1 已交付文件的六项 SHA256 全部保持一致。

## 3. 实际命令

以下均已实际执行，完整输出位于 `work/v3.6.2/`，随 Evidence.zip 提供。

```powershell
$env:OBS_ROOT=Join-Path $PWD 'runtime/obs33'
& 'third_party/cmake/cmake-4.4.5-windows-x86_64/bin/cmake.exe' --build build --target clean
tools/build.cmd 4
tools/test.cmd
& tools/test-installer-v362.ps1
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.6.2/verified-bridge
```

实际 Python 解释器为 `C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`，依次运行：

```text
tests/v362_light_probe.py work/v3.6.2/verified-light work/v3.6.2/verified-bridge
tests/v36_render_probe.py work/v3.6.2/verified-gpu work/v3.6.2/verified-bridge
tests/v34_render_probe.py work/v3.6.2/verified-fx work/v3.6.2/verified-bridge
tests/v34_coordinate_probe.py work/v3.6.2/verified-fx work/v3.6.2/verified-bridge
tests/v362_light_grade_probe.py work/v3.6.2/verified-grade work/v3.6.2/verified-bridge work/v3.6.2/verified-light
tests/v342patch_probe.py work/v3.6.2/verified-fx-lighting work/v3.6.2/verified-bridge
```

安装器暂存目录修复后再次实际执行 `tools/build.cmd 4` 和安装器全套测试；重启隔离 OBS 后运行：

```text
tests/v362_restart_probe.py work/v3.6.2/verified-restart work/v3.6.2/verified-restart work/v3.6.2/verified-light
```

最后验证运行中安装返回 10 且所有安装文件哈希不变；正常退出 OBS，再执行 `work/v3.6.2/restore-environment.ps1`。

## 4. 验收结果

| 检查 | 状态 | 证据 |
|---|---|---|
| MSVC 全量编译、链接 DLL 与 EXE | PASS | `build-final.log`；安装器最终重编译 `build-installer-final.log` |
| 全部 13 组 CTest | PASS | `test-final.log`，0 失败 |
| 灯光专项 31 项 | PASS | `verified-light/light-tests.json` |
| 灯光保存与重启 14 项 | PASS | `verified-restart/restart-tests.json` |
| 95 项 FX、202 项真实图像检查 | PASS | `verified-fx/render-tests.json` |
| 图形与遮罩 24 项 | PASS | `verified-gpu/geometry-tests.json` |
| 坐标 GPU 检查 8 项 | PASS | `verified-fx/coordinates/coordinate-tests.json` |
| 高饱和度彩色直射与补光合成 1 项 | PASS | `verified-grade/grade-tests.json` |
| 独立 FX 环境色与背光 25 项 | PASS | `verified-fx-lighting/patch-tests.json` |
| 覆盖升级、锁文件失败回滚、配置保留、卸载重装 18 项 | PASS | `installer-final/installer-tests.json` |
| OBS 运行中拦截安装 | PASS | `verified-running-guard.json`，退出码 10、安装文件未变 |
| 最终载荷来源、Shader、正常退出日志 | PASS | 两次 `installed-path.json`、`obs-runtime.log` |
| 隔离配置与 QA 环境恢复 | PASS | `environment-restored.json`，153 个配置文件恢复、QA 已移除、4457 无监听 |
| Windows 11、NVIDIA、集成显卡 | NOT RUN | 本轮未具备对应设备 |
| 长时间推流、实体功耗测量 | NOT RUN | 本轮仅完成下述短时性能测试 |
| 用户实际场景的艺术效果验收 | NOT RUN | 只读诊断其空间信息；图像验收使用合成测试素材 |

共完成 323 项专项、回归与安装检查，另有运行拦截及环境恢复检查。未将旧版测试结果作为本版 PASS。

失败记录保留：`red-repro` 为旧版真实复现；`build-first.log` 为新增代码首次编译时的未声明函数错误，已更正；`installer-first-failure` 为暂存目录重名，已修复并重跑最终 18 项安装测试。编译仍存在既有第三方常量条件和编辑器变量遮蔽警告，本版新增灯光代码无新增编译警告。

## 5. 性能实测

Windows 10 专业版 19045、Ryzen 7 5800X、Radeon RX 6900 XT、31.92 GiB RAM。OBS 33.0.1、D3D11、1920×1080/60fps；测试场景含两张合成图层，一个带孔洞的透明前景和一个接收层。每种组合实际运行不少于 30 秒。

| 组合 | OBS 平均 CPU | OBS 平均渲染时间 | 平均 FPS | 新增渲染跳帧 |
|---|---:|---:|---:|---:|
| 关闭舞台灯光 | 0.267% | 0.3361ms | 60.00 | 0 |
| 单灯软阴影 | 0.325% | 0.3363ms | 60.00 | 0 |
| 四灯软阴影 | 0.332% | 0.3518ms | 60.00 | 0 |

三个组合均实际渲染超过 1500 帧，预热后 FX 缓存分配数稳定，渲染失败为 0。另执行 80 次灯光位置与软阴影参数变更，未出现渲染失败。公共 FX 链和独立背光各完成另一轮 30 秒缓存稳定验证。

以上为该开发设备上整个 OBS 进程的短时采样，不是独立插件 CPU、GPU 能耗或其他硬件性能承诺。原始逐秒样本和硬件信息随证据提供。

## 6. 实际截图

灯光页紧凑布局：

![灯光页](../../work/v3.6.2/verified-bridge/light-after.png)

高环境光下的软阴影、透明孔洞与接收平面：

![软阴影](../../work/v3.6.2/verified-light/bright-ambient-shadow-on.png)

正常重启后的画面：

![重启恢复](../../work/v3.6.2/verified-restart/shadow-restart.png)

## 7. 交付与校验

交付目录 `dist/v3.6.2/`：安装器、对应源码 ZIP、符号 ZIP、测试证据 ZIP、本报告、交付清单和 SHA256SUMS。

- DLL SHA256：`296e50a4472b30a38def3a2908fccc73a36710836bc0d7a357770fbbf2114a1f`
- 主 Shader SHA256：`b21ac11f8c0df71c31e8e3686596093787de60f6d7b13f36199924002f1a14e6`
- Setup SHA256：`5e11dd5602617c952648d2fd6399c947d438d3f5a1c0d9afc06413ced81c7d71`

安装到 ProgramData，未将 Qt、OBS 或 QA DLL 放进安装载荷。正常关闭 OBS 后双击安装器即可覆盖升级。源码包含本地未提交变更；GitHub 未发布。
