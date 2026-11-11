# 固定版说明书维护

交付给使用者的是 `index.html`：双击即可离线阅读，图片、CSS 与 JS 已全部内置。GitHub 文件预览显示 HTML 源码，普通使用者请到 Release 下载 `OBS_Spatial_FX_V3.4.1_Manual.html`。

本目录包含：

- `USER_GUIDE.md`：15 章中文操作说明。
- `catalog.json`：从原 v3.4.1 C++ 聚合目录读取的 95 项效果与 474 项参数。
- `screenshots/`：111 张原始截图；`SCREENSHOT_PROVENANCE.json` 记录原始证据路径、大小和 SHA256，图片没有改绘。
- `widgets.html`、`manual.css`、`manual.js`：五组明确标为 HTML 示意的本地交互，不连接 OBS。
- `page-base.html`：使用 Page/Swiss 渲染器生成的正文布局中间文件；最终构建时移除通用模板的外部图片、远程字体和文字编辑入口。
- `build_manual.py`：Python 标准库构建脚本；从仓库根目录运行 `python docs/v3.4.1/manual/build_manual.py`。
- `BUILD_MANIFEST.json`：正文与最终 HTML 哈希、覆盖数。
- `verify_manual.cjs`、`VALIDATION.json`：真实 Edge/Playwright 阅读与交互检查，以及本次结果。

## 重建

克隆仓库后，普通重建不需要 OBS、不需要联网，也不需要 Page 技能：截图和固定正文布局已纳入版本管理。Python 脚本读取当前 `src/fx_catalog_*.inc`；若本地同时保留原运行导出的 `build/fx-runtime-catalog.json`，会额外比对每项默认值与范围。没有该运行导出时，构建报告会如实显示未执行此交叉核对。

单独下载 Manual.zip 时可以直接阅读 HTML/Markdown；维护脚本仍需要完整插件仓库中的 `src/`。准备修改时，把说明书目录放回仓库的 `docs/v3.4.1/manual/`，不要在脱离工程的 ZIP 解压目录中运行构建。

固定版本正文被 `page-base.sha256` 约束，避免改了 Markdown 却悄悄发布旧正文。修改 USER_GUIDE 后，要先重新渲染 page-base。作者使用的外部工具是归藏 Page 技能的 `scripts/render.mjs`，调用方式：

```text
node <Page技能目录>/scripts/render.mjs USER_GUIDE.md annotations.json page-base.html --style swiss --theme ikb
```

更新 `page-base.sha256` 为 USER_GUIDE.md 的 SHA256 后再运行 Python。该外部排版工具不属于插件构建依赖；社区开发者也可使用自己的渲染器，只要保持文中 `@@...@@` 插槽、章节和 TOC 结构，并重新做浏览器验收。不要仅改哈希绕过正文同步。

## 浏览器验证

安装 Node.js、Playwright，并准备 Edge 或 Chromium。设置 `PLAYWRIGHT_EXECUTABLE_PATH` 为浏览器绝对路径，执行：

```text
node docs/v3.4.1/manual/verify_manual.cjs
```

结果与排版截图写入 `work/publish-v341/`，不会修改插件、OBS 或个人场景。检查包括 95 项/13 组/474 参数覆盖、111 张离线图片、有效锚点、搜索、展开折叠、放大图、五组交互、桌面/窄屏无整页横溢、零脚本错误和零 HTTP 请求。测试不代表新增 OBS 渲染或功耗测试。

若新增或更新截图，应从真实运行或测试输出取图，更新来源清单；不可把 HTML 示意截图标为插件运行截图。只在用户明确要求时更新其他版本说明书，本目录只针对 v3.4.1。
