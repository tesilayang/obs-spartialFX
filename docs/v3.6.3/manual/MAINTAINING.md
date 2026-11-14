# v3.6.3 说明书维护

正文为 `USER_GUIDE.md`，FX 参数从当前 `src/fx_catalog*.inc` 抽取，预设脚本从 `src/presets/composer.cpp` 抽取。截图使用留存真实证据，逐图版本、来源与 SHA256 见 `SCREENSHOT_PROVENANCE.json`。历史示例不能写成新版本重测结果。

## 重建

仅调整样式或动画时，用 Python 3.12+ 执行：

```text
python docs/v3.6.3/manual/build_manual.py
```

新克隆使用已入库截图；作者机器存在原证据时会核对并复制对应证据。正文修改后，先使用归藏 Page 的 `render.mjs` 刷新 `page-base.html`：

```text
node <guizang-page-skill>/scripts/render.mjs docs/v3.6.3/manual/USER_GUIDE.md docs/v3.6.3/manual/annotations.json docs/v3.6.3/manual/page-base.html --style swiss --theme ikb
```

将正文文件 SHA256 写入 `page-base.sha256`，再运行 builder。Page 底稿为外部排版工具的产物，已经入库；普通重建不依赖该技能安装位置。最终 HTML 去掉外部字体、编辑功能和通用设置按钮，截图/样式/动画均内嵌。

`catalog.json` 的默认值与范围来自源码。本轮未重新导出完整运行时参数目录，`runtime_defaults_and_ranges_crosschecked` 为 false，禁止改成 true。显式提供并核对同目录 `runtime-catalog.json` 才会通过运行目录交叉校验。

## 验证

安装 Playwright，或使用含 Playwright 的运行环境。执行：

```text
node docs/v3.6.3/manual/verify_manual.cjs
```

默认使用本机 Edge；可用 `PLAYWRIGHT_EXECUTABLE_PATH` 指定 Chromium。检查桌面与手机视窗、95 项目录、478 个参数、离线图片、目录锚点、搜索、图片放大及七个示意。结果在 `work/publish-v363/manual-qa.json`，固定验证摘要为 `VALIDATION.json`。

讲解动画不连接 OBS，不作为 GPU 效果或性能证据。发布前检查其说明与真实 UI 一致；更新截图必须更新来源记录，不能把合成示意标为真实截图。
