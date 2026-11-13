# OBS Spatial FX v3.5.0 开发与交付记录

日期：2026-11-13。用户版本名 v3.5；插件、安装器与交付文件统一使用 3.5.0。验收环境：Windows 10 22H2 x64（19045）、AMD Ryzen 7 5800X、AMD Radeon RX 6900 XT、OBS 33.0.1、MSVC x64、Qt6。

## 本版功能

保留“预设”页，新增“工程管理”页，提供四个操作按钮。

1. **导出工程 JSON**：保存当前场景和依赖来源的设置、原素材路径、舞台、实例、关键帧、灯光、FX、滤镜、音频与私有设置，不复制素材。
2. **导出完整工程包 ZIP**：扫描依赖，允许核对和补充文件/目录，复制原文件并记录 SHA256，使用 ZIP64 保存大文件。
3. **导入工程 JSON**：按存档内的原场景名称查找。已有同名场景时，确认并保存恢复 JSON 后覆盖；不存在时按存档名称新建。缺失素材列出，允许选择替代文件或文件夹，也可保留失效路径继续恢复配置。
4. **导入完整工程包**：始终新建场景，重名自动编号。可直接指定已解压的根文件夹，也可指定 ZIP 与解压父目录。ZIP 在父目录内创建唯一的持久文件夹，完成文件校验和路径转换后重建场景。导入后必须保留这个素材文件夹。

导入会复制来源、舞台和实例标识并重写引用，保留同名目标场景 UUID，保护其他场景共享的原来源。隐藏项、分组、嵌套场景、离场依赖和压缩器侧链来源都在依赖闭包内。

没有原生场景项持有的依赖，会保存在导入场景中自动创建的隐藏、锁定“Spatial FX 工程依赖”组，确保 OBS 保存与重启后仍保留这些来源。请保留这个组。

文件扫描、复制、散列与解包在工作线程执行；OBS 快照、确认及场景提交在 UI 线程执行。文件任务支持取消，扫描取消不会发布部分清单。ZIP/目录校验包含路径穿越、文件名冲突、NTFS 重解析点、大小、CRC 与 SHA256；失败保留已有目标文件。

## 导入边界

- 捕捉设备、联网服务、字体和第三方来源/滤镜需要目标电脑具备对应环境；界面显示运行要求。缺少来源/滤镜实现，或实例创建失败，必须在覆盖前停止。
- 动态脚本生成的文件路径无法全部自动推断，可在打包核对窗口补充。跳过的链接依赖会明确列出。
- 同名目标与存档的自定义尺寸模式不一致时停止覆盖。OBS 公共接口无法安全清除旧模式；先重命名目标，再按存档名称新建。
- 根场景的上级场景也被共享舞台关联时，保留它会形成循环引用，导入前停止。请从最上级场景导出或解除该关联。
- 覆盖时先校验临时场景和实际来源/滤镜，再提交；提交后检查原生场景项数量，失败恢复原设置及来源引用。

## 实际修改

- `src/project/project_obs.*`：OBS 来源闭包、名称/UUID 映射、滤镜顺序、侧链、隐藏依赖持有与安全提交。
- `src/project/project_document.cpp`：素材发现、设备路径排除、网页目录放置、重新链接和格式校验。
- `src/project/project_archive.*`、`src/project/vendor/miniz/`：归档与校验；miniz 3.1.0 固定提交、MIT 许可证和逐文件 SHA256。
- `src/project/project_panel.*`：四按钮、打包核对、两种完整包导入、缺失素材处理、恢复快照与后台任务。
- `src/dock.cpp`、`src/plugin.cpp`、`CMakeLists.txt`、`installer/`、`data/PROJECT_NOTICES.txt`：页面接入、版本、构建与安装载荷。
- `tests/project_*_tests.cpp`、`tests/v35*_qa.inc`、`tests/obs_qa_driver.cpp`、`tools/test-installer-v35.ps1`：文件、真实 OBS 界面与安装验证。
- `README.md`：新增本版能力；固定 HTML 说明书未修改。

## 执行命令

在项目根目录 PowerShell 执行；完整日志在交付 Evidence.zip。

```powershell
$env:OBS_ROOT = Join-Path $PWD 'runtime/obs33'
& tools/build.cmd 4
& tools/test.cmd
& tools/test-installer-v35.ps1
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v35 -Output work/v3.5/verified-roundtrip -Wait
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v35-persist -Output work/v3.5/verified-restart -Wait
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v35-ui -Output work/v3.5/verified-ui -Wait
$env:PATH = (Join-Path $PWD 'third_party/qt6/bin') + ';' + $env:PATH
& build/spatial-project-archive-tests.exe --large
& tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/v3.5/verified-bridge
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tests/v34_render_probe.py work/v3.5/verified-gpu work/v3.5/verified-bridge
& 'C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' tests/v34_coordinate_probe.py work/v3.5/verified-gpu work/v3.5/verified-bridge
```

测试用隔离 OBS 的 WebSocket 临时使用本机 4457 端口；测试结束恢复原配置并关闭监听。正式安装包不包含 QA 驱动、OBS 或 Qt DLL。

## 验收状态

最终状态、哈希和性能数据由交付清单及最终测试报告列出。旧失败记录保留，最终证据必须与安装载荷 DLL 哈希相符。

| 项目 | 状态 | 证据 |
|---|---|---|
| C++/安装器真实编译 | PASS | `build-final.log` |
| 10 组 CTest | PASS | `test-final.log` |
| 场景闭包、同名覆盖、共享隔离、离线包恢复、边界拦截 | PASS | `verified-roundtrip/dock-smoke.json`，56 项 |
| 保存重启 | PASS | `verified-restart/dock-smoke.json`，14 项 |
| 四按钮、确认、恢复 JSON、手动重新链接 | PASS | `verified-ui/dock-smoke.json`，90 项，含真实界面截图 |
| ZIP64 实际超过 4GB 文件 | PASS | `archive-final-large.log`，4,296,015,872 字节原文件往返 |
| 升级/回滚/配置保留/卸载重装 | PASS | `installer/installer-tests.json`，18 项；`verified-running-guard.json`，退出码 10 且载荷不变 |
| 原 FX、坐标、1080p60 | PASS | `verified-gpu/render-tests.json`，95 个效果、202 项；`coordinates/coordinate-tests.json`，8 项 |
| 其他 OBS 版本、核显/NVIDIA、Windows 11、长时间网络推流 | NOT RUN | 本版未执行 |
| 物理功耗测量 | NOT RUN | 不作功耗结论 |
| 实际跨电脑迁移 | NOT RUN | 已验证原路径不可用时离线包恢复，未使用第二台物理电脑 |

## 性能采样

1080p/60fps，实际 FX 链为基础模糊（FX01.0）、基础颜色（FX03.0）、暗角（FX04.4），连续采样 30 秒。OBS 帧率均为 60.0000；CPU 0.20%～0.68%，内存 432.04～432.06 MiB，平均帧渲染时间 0.244～0.387 ms。采样开始已有 32 个累计渲染跳帧，采样期间未增加；没有进行编码或网络推流。插件纹理分配数保持 46，着色器失败计数为 0。数值仅对应这台开发机及该测试场景。详细组合与原始数据见 `verified-gpu/performance.json` 和 `render-tests.json`。

测试结束正常卸载模块，OBS 日志记录内存泄漏数为 0；`environment-restored.json` 记录配置逐文件 SHA256 恢复、无 OBS 进程、无 QA 驱动与无 4457 监听。

## 研发中发现并处理的问题

- Qt 测试代码保存了临时 JSON 对象的引用，导致测试驱动崩溃；改为值拷贝。崩溃记录保留，修复后重新执行。
- OBS 原生场景不接受仅音频滤镜；修正测试夹具，在媒体来源验证侧链，在根场景验证视频滤镜顺序。
- 原生滤镜枚举已按保存顺序反向枚举；移动已验证滤镜时使用枚举顺序，避免再次反转。
- 仅 UUID/名称关联不足以持有离场来源；新增隐藏依赖组，并验证重启后的关联。
- 进度窗口程序关闭触发取消信号；完成后先断开取消连接，再关闭窗口并提交后续操作。
- 文件选择器测试初始名称被默认值覆盖；等待目录模型就绪，明确设置并核对完整路径，再确认。
- UTF-8 文件名字节数与字符数限制不一致；补充 180 个汉字文件名往返测试并统一检查。
- GPU 首次启动时测试端口未启用；保留失败日志，启用隔离实例后重新执行。

## 交付与后续

本地交付目录 `dist/v3.5`：安装器、源码 ZIP、符号 ZIP、证据 ZIP、报告和 SHA256。未自动推送或发布 GitHub；历史 Release 保留。v3.6 快捷预设计划见 `V36_PRESET_ROADMAP.md`。
