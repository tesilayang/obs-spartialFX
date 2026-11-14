# OBS Spatial FX v3.6.3 说明书与发布记录

日期：2026-11-14。范围：更新说明书、README、构建说明，归档此前已完成的 v3.6.0～v3.6.3 源码与专项记录，制作首次公开 v3.6.3 Release。

## 本次文件

- `docs/v3.6.3/manual/`：中文正文、单文件 HTML、样式、交互动画、构建与验证脚本、目录元数据及截图来源清单。
- `README.md`、`BUILDING.md`：当前能力、下载入口、编译与测试范围。
- `tools/publish-v363.py`：固定提交源码归档、载荷来源检查、历史包保护、Release 下载及线上源码校验。
- `docs/v3.6.3/RELEASE_NOTES.md`：仅列本版新增与修复。
- 本地已完成的 `src/presets/`、FX、灯光、安装器、测试和 `docs/v3.6/` 记录一并进入 v3.6.3 标签；开发标准同步到公开仓库。

本次没有修改插件算法。README 嵌入安装器资源，故重新链接安装器。插件 DLL 与最新实际 GPU 验收载荷相同。

## 说明书

包括安装、导入、多实例、空间坐标、时间轴、关键帧曲线、灯光、95 项 FX 的 478 个参数、节拍、卡片式预设、完整脚本库、四种工程管理操作、故障检查和开发环境。

截图目录含 120 个真实留存图像文件；正文会重复引用部分图像。v3.6.3 界面与最新光影使用本版证据，其余历史示例标为 v3.4.1。具体来源和 SHA256 在 `SCREENSHOT_PROVENANCE.json`。未声明本轮重新执行 95 项 GPU 全量回归。

七个交互示意：空间视差、关键帧与贝塞尔、绑定灯光、FX 启用关系、拍点、预设预览与补充、工程导入去向。示意不连接 OBS，不作为真实效果或性能证据。

## 实际执行命令

```powershell
& 'F:/Program Files (x86)/Node/node.exe' 'C:/Users/admin/.agents/skills/guizang-page-skill/scripts/render.mjs' docs/v3.6.3/manual/USER_GUIDE.md docs/v3.6.3/manual/annotations.json docs/v3.6.3/manual/page-base.html --style swiss --theme ikb
python docs/v3.6.3/manual/build_manual.py
node docs/v3.6.3/manual/verify_manual.cjs
$env:OBS_ROOT=Join-Path $PWD 'runtime/obs33'
& ./tools/build.cmd 4
& ./work/publish-v363/install-final.ps1
& ./tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34-hold -Output work/publish-v363/bridge-release
python work/publish-v363/smoke.py
& ./work/publish-v363/finish.ps1 -Bridge work/publish-v363/bridge-release
& ./work/publish-v363/restore-release.ps1
git -c core.whitespace=cr-at-eol diff --check
python tools/publish-v363.py --commit HEAD
python tools/publish-v363.py --verify
```

实际 Python 使用 `C:/Users/admin/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe`，Node 使用 `F:/Program Files (x86)/Node/node.exe`。浏览器验证使用本机 Playwright/Edge，`NODE_PATH` 指向依赖运行目录。GitHub 操作用已授权 `gh`，不读取或输出令牌。

## 本轮测试状态

| 项目 | 状态 | 证据 |
|---|---|---|
| 离线说明书 18 项浏览器检查 | PASS | `manual/VALIDATION.json`；桌面/手机截图与 `manual-qa.json` |
| 95 FX、13 组、478 参数及全部图片解码 | PASS | 浏览器检查与 `BUILD_MANIFEST.json` |
| 七个讲解示意、搜索、导航、放大、窄屏 | PASS | 浏览器检查；零脚本错误、零 HTTP 请求 |
| 当前参数目录与完整运行时导出再次交叉核对 | NOT RUN | 本轮从当前源码抽取，构建清单中交叉核对标志为 false |
| MSVC 安装器资源编译与链接 | PASS | `work/publish-v363/build-final.log` |
| 最终安装、版本、三种布局及配置保留 | PASS | `final-install-publish/install-tests.json`，9 项 |
| 最终载荷 OBS 33.0.1 加载及五页真实 UI | PASS | `bridge-release/installed-path.json`、`publish-tab-*.png`、`runtime-smoke.json` |
| 正常退出、清理 QA、恢复 153 个配置文件 | PASS | `environment-restored.json` 与 `exit-status.json` |
| 无关产品功能全量回归 | NOT RUN | 本轮仅文档和重打包，按开发标准限定范围 |
| 新的功耗、其他硬件和长时间推流 | NOT RUN | 无本次改动，不增加这些验收 |

UI 冒烟辅助脚本首次读取了错误的配置文件，随后发现场景集合文件名自带 `.json`；已修正脚本并在新隔离进程完成五页检查。两次提前结束均正常退出并恢复配置，不作为产品功能失败或通过证据。

## 载荷、归档与线上核对

插件 DLL SHA256：`374b284c6509d1e36a7a2fb21abf9da86d2cb3c5567b42d21351edb72e924b5e`。

本次安装器 SHA256：`a707d741164b6156d9f75e30e64b36efae9cb6bb976876ad49963ab1033397e8`。

本次安装器包含更新后的 README。此前同版本交付目录及 SHA256 保留原样。新发布目录为 `dist/v3.6.3-release/`；提供 Setup、固定提交 Source.zip、离线 HTML、Manual.zip、Symbols.zip、Evidence.zip、发布记录、清单和 SHA256SUMS。

源码 ZIP 由 `git archive` 从确定提交生成，逐文件核对 Git 内容；提交号写入 `RELEASE_MANIFEST.json`。发布前确认公开仓库默认分支为 main，推送后由 `--verify` 核对 main、标签、完整文件树、开发标准原文、Latest 状态和全部下载文件大小/SHA256。实际线上校验结果随后保存在本机 `work/publish-v363/online-verification.json`；预先生成的 Evidence.zip 不包含这份上传后的检查结果。本记录不以预期结果替代该检查。

公开清单排除缓存、SDK、配置、凭据和无授权媒体。开发标准采用当前本地 `DEVELOPMENT_STANDARDS.md`，仅验收 OBS 33.0.1；小版本按本次变更执行专项测试。
