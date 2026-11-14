# v3.6.3 阴影轮廓隔离交付报告

日期：2026-11-14。版本保持 3.6.3。

## 交付结果

- 舞台轮廓光仅从局部光效合成前的素材表面提取轮廓；FX 新增阴影、已裁掉的原图轮廓均不再形成舞台亮边。
- FX 投影／内阴影移除辅助强度和辅助颜色；原有阴影色、角度、偏移、模糊、扩散、强度和混合控制保留。
- 旧存档的辅助数据不读取、不写出，保存重启后保持移除。
- 已用最终安装器升级本机系统插件目录，并在隔离 OBS 33.0.1 验证最终安装载荷。
- 历史交付物保留；使用说明书未修改；GitHub 未提交、推送或发布。

## 实际修改文件

生产代码：`src/stage.cpp`、`src/fx_renderer.hpp`、`src/fx_catalog_a.inc`、`src/fx_panel.cpp`、`src/fx_model.cpp`、`data/effects/spatial.effect`、`data/effects/fx05.body`。

测试：`tests/fx_tests.cpp`、`tests/v363_fx_optics_probe.py`、`tests/v363_shadow_isolation_probe.py`、`tests/v363_shadow_isolation_restart.py`。

记录和打包：`docs/architecture/SPATIAL_COORDINATE_CONTRACT.md`、`docs/v3.6/V363_SHADOW_ISOLATION_IMPLEMENTATION.md`、本报告及 `tools/package-v363-shadow-isolation.py`。

详细公式、资源复用及存档规则见实施记录。

## 真实编译与命令

工作目录：`E:\obs spartialFX develop`。

```powershell
$env:OBS_ROOT=Join-Path $PWD 'runtime/obs33'
& tools/build.cmd 4 *>&1 | Tee-Object work/v3.6.3-shadow-isolation/build-final.log
& third_party/cmake/cmake-4.4.5-windows-x86_64/bin/ctest.exe --test-dir build --output-on-failure -R '^spatial-fx$|^spatial-fx-stage$'
& work/v3.6.3-shadow-isolation/install-final-v2.ps1
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.6.3-shadow-isolation/final-bridge
```

真实 MSVC 编译成功，最终日志含插件 DLL 与安装器 EXE 链接。首次完整增量编译见 `build-first.log`；裁切边界修正后的最终编译见 `build-final.log`。最终 CTest 日志为 `test-delivery.log`，2/2 PASS。

以下命令使用 Python：
`C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`。

```text
tests/v363_shadow_isolation_probe.py work/v3.6.3-shadow-isolation/final-isolation work/v3.6.3-shadow-isolation/final-bridge --extended --performance
tests/v363_fx_optics_probe.py work/v3.6.3-shadow-isolation/final-optics work/v3.6.3-shadow-isolation/final-bridge
tests/v363_optics_global_probe.py work/v3.6.3-shadow-isolation/final-projection work/v3.6.3-shadow-isolation/final-bridge
```

隔离 OBS 正常退出后，通过同一启动脚本以 `final-restart` 目录启动新进程，再执行：

```text
tests/v363_shadow_isolation_restart.py work/v3.6.3-shadow-isolation/final-restart-results work/v3.6.3-shadow-isolation/final-restart work/v3.6.3-shadow-isolation/final-isolation
```

最后正常退出隔离 OBS，执行 `restore-environment-final.ps1`；153 个原配置文件逐一哈希匹配，QA DLL 已移除，测试端口无监听。

## 复现与最终验收

| 检查 | 状态 | 实际证据 |
| --- | --- | --- |
| 原安装版复现用户问题 | FAIL（预期失败） | `red/isolation-tests.json`：13 项中 7 项失败；外阴影区域轮廓光差值峰 156，辅助控件和存档字段仍存在 |
| 第一修正版的实体裁切边界复现 | FAIL（预期失败） | `crop-red/isolation-tests.json`：27 项中 1 项失败；crop_left=180、投影向左时差值峰 225 |
| 最终阴影／轮廓、UI、保存与性能 | PASS | `final-isolation/isolation-tests.json`：33/33，包括 4 项性能检查 |
| 辉光、星芒关联短探针 | PASS | `final-optics/optics-tests.json`：14/14 |
| 全局投影、接收面及几何遮罩 | PASS | `final-projection/projection-tests.json`：6/6 |
| 真实进程重启与像素一致性 | PASS | `final-restart-results/restart-tests.json`：5/5，保存前后像素差 0 |
| 相关 CTest | PASS | `test-delivery.log`：2/2 |
| 安装载荷与配置保留校验 | PASS | `final-install/install-tests.json`：9/9 |
| 最终 DLL、着色器来源及正常退出 | PASS | `final-bridge`、`final-restart` 中 `installed-path.json`、`obs-runtime.log`、`exit-status.json` |
| 测试环境恢复 | PASS | `environment-restored.json` |
| 无关功能全量回归、工程导入导出、缺素材报警 | NOT RUN | 按本次光影改动范围排除 |
| 其他 OBS 版本、Windows 11、核显、NVIDIA、长期推流及实体功耗 | NOT RUN | 本轮未执行 |

最终实际 OBS 专项合计 58 项：33＋14＋6＋5。失败记录原样保留，不计为最终 PASS。

最终验证含内外阴影、轮廓光开关、旧辅助数值、裁切切入实体、旋转、镜像、非等比缩放、几何遮罩、Alpha128 原素材、50% 图层透明度、阴影颜色实时预览与取消恢复。阴影区域轮廓光差值峰为 0；原素材轮廓仍有可见受光。50% 图层透明度的实际覆盖比例为 0.49816。

## 性能记录

Windows 10 19045，AMD Ryzen 7 5800X，AMD Radeon RX 6900 XT。OBS 33.0.1，D3D11，1920×1080、60fps；单个 1920×1080 RGBA 输入，开启舞台灯光与轮廓光。硬件详情见 `hardware.json`。

| 场景 | 时长 | OBS 平均渲染时间 | 最高采样渲染时间 | 纹理分配计数 | 新增渲染丢帧 |
| --- | --- | --- | --- | --- | --- |
| 舞台轮廓光 | 30.078 秒 | 0.28646 ms | 0.31843 ms | 70→70 | 0 |
| 舞台轮廓光＋FX 外投影 | 30.093 秒 | 0.36229 ms | 0.40233 ms | 78→78 | 0 |

以上时间取自实际 OBS GetStats 采样；插件 GPU 错误计数为 0。完整采样见 `final-isolation/performance.json`。未测实体功耗。

## 交付文件与校验

交付目录：`dist/v3.6.3-shadow-isolation/`。

- `OBS_Spatial_FX_V3.6.3_ShadowFix_Setup.exe`：可覆盖升级的安装器。
- `OBS_Spatial_FX_V3.6.3_ShadowFix_Source.zip`：对应当前工作区源码快照。
- `OBS_Spatial_FX_V3.6.3_ShadowFix_Symbols.zip`：对应调试符号。
- `OBS_Spatial_FX_V3.6.3_ShadowFix_Evidence.zip`：实际截图、编译、复现、运行和校验日志。
- 两份开发记录、`DELIVERY_MANIFEST.json`、`SHA256SUMS.txt`。

插件 DLL SHA256：
`374b284c6509d1e36a7a2fb21abf9da86d2cb3c5567b42d21351edb72e924b5e`

安装器 SHA256：
`786d2d3ec720bee624638705e6da081c791f88c653417cffc7f1da1203187b7a`

最终日志扫描无着色器编译错误、未设置参数或 Spatial FX GPU 失败。安装器及插件版本均为 3.6.3；安装位置为 `C:\ProgramData\obs-studio\plugins\obs-spatial-fx`。
