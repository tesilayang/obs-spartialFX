# v3.6.4 仓库发布记录

用户于 2026-11-15 授权更新仓库并发布 v3.6.4。

- 发布仓库：`tesilayang/obs-spartialFX`，默认分支 `main`，标签 `v3.6.4`。
- 提交只包含 v3.6.4 代码、专项测试、开发记录、发布脚本及 README 版本入口。工作区已有 v3.6.3 说明书重命名和演示页保持原状。
- 沿用已实际安装、加载并验收的安装器，SHA256 为 `95BD1173A942111AEEC70DDECD596C97618EF58F990438CC17F48DAB1BCF34E2`；插件 DLL 为 `FD8C6486421B49E5FB9341574CE8F9BEA05A9D21CF211F8A557B05457BA8DD83`。
- 原交付目录 `dist/v3.6.4/` 保留原样；发布目录为 `dist/v3.6.4-release/`。发布源码由标签对应的 Git 提交归档，安装载荷的源码与已验收快照逐文件核对。
- 发布源码包含当前开发标准。说明书继续引用既有 v3.6.3 版本，不制作新版说明书。
- 公开证据包选取专项日志、合成测试图和结果；真实角色画面保留本地，公开包只保留 Spout 数值结果。
- 真实 NDI 仍为 BLOCKED，自动迁移没有可见取消按钮；状态沿用专项测试报告。

发布前实际重跑命令：

```powershell
& third_party/cmake/cmake-4.4.5-windows-x86_64/bin/ctest.exe --test-dir build `
  -R '^spatial-(model|scene-evaluator|fx|fx-stage|presets|composer|preset-panel|timeline-object|editor-timeline|fx-clock)$' `
  --output-on-failure > work/publish-v364/release-tests.log 2>&1
```

结果：10/10 PASS，4.93 秒。未改变产品代码或重建安装载荷。

发布脚本：`tools/publish_v364.py`。使用 `--package --commit HEAD` 生成对应提交的发布文件；使用 `--verify` 核对在线 main、标签、源码树、开发标准与所有资产的大小及 SHA256，并实际下载资产再核验。线上验证结果保存至 `work/publish-v364/online-verification.json`。提交号及发布资产摘要以 Release 的 `RELEASE_MANIFEST.json` 为准。
