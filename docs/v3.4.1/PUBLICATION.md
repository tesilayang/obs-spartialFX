# v3.4.1 公开发布与文档补充记录

日期：2026-11-11。用户本次明确要求缓存回收、长期 README、固定 v3.4.1 图文与动画说明书，以及 GitHub 公开下载。本记录补充原 `V341_REPORT.md`；原报告中“说明书未更新”描述的是插件构建交付当时的状态，本次随后新增说明书。

## 本地整理

14 个可丢弃缓存目标、58,615 个文件、12,216,747,394 字节（约 11.38 GiB）移入 Windows 回收站，未永久删除，未清空回收站。因此不能将其说成磁盘空间已经释放。移除内容主要是重复 SDK 解压目录、已完成安装事务回滚缓存与 GitHub CLI 下载解压缓存；源码备份、历史 dist 交付、现有构建和当前测试证据保留。

执行脚本和每项记录保存在开发机 `work/publish-v341/recycle-caches.ps1`、`cleanup-result.json`、`recycle-manifest.json`、`recycle-bin-verification.json`。已核对回收站原路径，以及受保护源码/历史交付物哈希。公开仓库不上传本机缓存和回收站内容。

## 文档内容

重写 README，介绍项目能力、初次操作、安装、源码构建、实际工具链和边界。明确未测整机电功耗，其他硬件不保证相同能耗/性能；集显、NVIDIA 实机、互联网推流、长时间稳定性未执行。

新增 `manual/index.html`：15 章操作说明、13 组/95 子功能/474 FX 参数、111 张留存真实截图、5 组 HTML 交互示意。说明图和真实截图明确区分。参数默认值与范围逐项对照 C++ 源码及原运行导出的目录。HTML 可独立离线打开。

真实执行：

```text
python docs/v3.4.1/manual/build_manual.py
node docs/v3.4.1/manual/verify_manual.cjs
```

作者使用内置 Python 与 Node 的绝对路径执行同一脚本；浏览器为真实无头 Microsoft Edge。15 项检查通过，111 图离线解码，1440×1000 与 390×844 检查，无脚本错误、无 HTTP 请求。结果见 `manual/VALIDATION.json`，排版截图保留在开发机 `work/publish-v341/`。

本轮没有改动 `src/`、`data/`、`installer/` 或插件测试代码，不重新编译/安装/运行插件，不将文档测试充作新的插件运行测试。插件实测仍引用原 `V341_REPORT.md` 记录。

## 冻结交付物

| 文件 | SHA256 |
|---|---|
| OBS_Spatial_FX_V3.4.1_Setup.exe | 0552e62dbcb6c1ac8041cfcc7b12c619ded3e90d994b8cf6870bb9c3c37eece3 |
| OBS_Spatial_FX_V3.4.1_Source.zip | 387602f2c3523a780f992e23f0358955984ed31d78e754acb54f66005714b754 |

原 Source.zip 与 EXE 保持原始字节。原源码 ZIP 中的 README 等说明保留构建交付时状态；仓库 main、v3.4.1 标签及单独说明书提供本次新文档。源码 ZIP 内 `src/`、`data/`、`installer/`、`tests/`、`tools/` 与本次发布工程一致；源包、符号和原证据包 CRC 检查通过。没有借本次整理重做安装包。

公开仓库：`https://github.com/tesilayang/obs-spartialFX`。固定发布入口：`releases/tag/v3.4.1`。下载页提供安装器、原始源码 ZIP、独立 HTML、说明书维护包及下载校验文件。普通用户只需 EXE 与 HTML；社区开发者可克隆仓库或下载原源码包。
