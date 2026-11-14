# v3.6 原生实时预设调研：目录 B

核查日期：2026-11-13（Asia/Shanghai）。本文件仅记录调研、证据和候选方向，没有修改产品代码。

## 结论与证据范围

- 六个指定地址中，五个仓库可公开读取；`0xpratzyy/specimen-and-signal` 的网页及 GitHub REST API 均返回 404。404 无法区分删除、私有、改名或地址错误，因此不据此断言项目不存在。
- `lemomo-ai/lemo-opuscar` 有真实的 Canvas 2D / Three.js 演示源码、时间函数和离线帧渲染工具，值得研究其构图、分层和确定性求值。其现有导出管线仍是浏览器逐帧截图 → ffmpeg → MP4，不能称为已适配 OBS 的原生实时渲染器。
- 其余四个可访问项目主要是提示词、来源链接、预览和目录生成工具。目录内标注 Three.js / Canvas / C++ 不等于仓库提供了这些示例的可执行源码。
- 多个目录收录同一条作者帖子，不能把重复收录当作独立复现或多次质量验证。标题、收藏数、README 中的模型声明均不作为画质、帧率或原生可移植性的实测证据。
- 本轮**实际阅读**了固定提交的 README、许可证、目录数据和选定演示源文件；**实际看过静态图**为 Lemo 的 Swiss、Bauhaus、HUD、Paper-cut 四张仓库 poster；**没有播放完整视频，没有运行这些演示、编译移植代码或测量 OBS 性能**。下文的运动描述分别标明来自源码、提示词或目录作者声明。
- `web.run` 已尝试打开/搜索指定仓库，但本环境未返回可用正文；实际证据来自 HTTPS 访问 GitHub REST API、固定 SHA 的 raw 源文件与原作者仓库文件。原始响应及抽取资料保存在 `work/v3.6/catalog-b/`，便于复核。未安装第三方依赖。

## 公开状态、固定提交与项目性质

GitHub API 查询的是核查时 `default_branch=main` 的提交；以下 SHA 是本轮观测值，不声称未来仍为最新。提交时间使用 GitHub committer 的 UTC 时间。[GitHub 仓库 API](https://docs.github.com/en/rest/repos/repos#get-a-repository) / [提交 API](https://docs.github.com/en/rest/commits/commits#get-a-commit)。

| 指定仓库 | 公开状态 / main HEAD | 提交时间 UTC | 仓库实际内容 / 栈 | 可供本项目直接研究的内容 |
|---|---|---|---|---|
| [TripoGrowthLab/awesome-opus-5-5-prompts](https://github.com/TripoGrowthLab/awesome-opus-5-5-prompts) | 可读；`d083bc4d56564a26cfd2e590a34376362f8c8b4b` | 2026-10-02 06:18:04 | CMS 同步生成的 Markdown 目录、WebP 预览、Node `.mjs` 同步/校验工具。README 标示 63 个案例、1 个有源码链接。不是 63 个渲染项目源码集合。 | 尤其是 AI orb 的周期时间、阻尼弹簧、内容独立进退场窗口等明确构建约束；案例须另查原作者源码与许可证。 |
| [BeatAPI/awesome-opus-5-5-videos](https://github.com/BeatAPI/awesome-opus-5-5-videos) | 可读；`a6af6772454710aab3ff8ee8a819f066701300e8` | 2026-09-28 04:28:12 | 20 条 JSON 案例、Markdown 画廊、Node 校验/README 生成脚本。数据明确写 `source-listed`、外链提示词/媒体。 | 作为原帖索引和重复案例识别；不作为实时渲染器、原始素材或已验证的效果库。 |
| [lemomo-ai/lemo-opuscar](https://github.com/lemomo-ai/lemo-opuscar) | 可读；`3a7132b996be0241c1ea0ab180d519f06e6f10b6` | 2026-11-12 11:29:01 | 真实代码工具包：ES modules、Canvas 2D、Three.js、WebGL2、Playwright、Python 音频/字幕、ffmpeg；99 个画廊风格，另有 `_template`（树中共 100 个 `style.json`，不能误报为 100 个成品风格）。 | 最强的源码研究对象；拆出几何、裁切、角色层级、时间函数与色彩系统，再原生重写。完整离线制作工具包不适合作为插件运行依赖。 |
| [X-RayLuan/awesome-opus-5-5-video-prompts](https://github.com/X-RayLuan/awesome-opus-5-5-video-prompts) | 可读；`33fbd3712817cf056056b70a7c71d8313917e0ea` | 2026-11-05 17:07:16 | 84 条结构化案例、提示词文本、预览、来源采集证据；工具主要维护目录。`prompt.type`：40 verbatim，40 not_shared，另外 4 条是作者描述/摘录/仓库引用。 | 查找具体原作者和技巧；一半案例没有共享提示词，更不能默认有源码。 |
| [0xpratzyy/specimen-and-signal](https://github.com/0xpratzyy/specimen-and-signal) | 网页 404；[REST API](https://api.github.com/repos/0xpratzyy/specimen-and-signal) 404；HEAD 未取得 | 未知 | 内容、栈、许可证、示例、演示均未知。 | 暂不能作为选型或复用依据。需要可访问的准确地址或用户提供的合法源码快照。 |
| [Li-Evan/awesome-opus-5.5-video-prompts](https://github.com/Li-Evan/awesome-opus-5.5-video-prompts) | 可读；`7f731215e5efbce772caf49d8dd016f310a97377` | 2026-09-27 15:38:34 | YAML 提示词/原帖/媒体元数据、Markdown 提示词页、静态 HTML 画廊、Python 构建和媒体检查。README 声明 334 条提示词、302 名作者。不是 334 个可运行项目。 | UI 单形状变形、像素循环等提示词的可执行约束很具体；仍需自己实现和验证。 |

对应固定提交入口：[Tripo 树](https://github.com/TripoGrowthLab/awesome-opus-5-5-prompts/tree/d083bc4d56564a26cfd2e590a34376362f8c8b4b)、[Beat 树](https://github.com/BeatAPI/awesome-opus-5-5-videos/tree/a6af6772454710aab3ff8ee8a819f066701300e8)、[Lemo 树](https://github.com/lemomo-ai/lemo-opuscar/tree/3a7132b996be0241c1ea0ab180d519f06e6f10b6)、[X-Ray 树](https://github.com/X-RayLuan/awesome-opus-5-5-video-prompts/tree/33fbd3712817cf056056b70a7c71d8313917e0ea)、[Li-Evan 树](https://github.com/Li-Evan/awesome-opus-5.5-video-prompts/tree/7f731215e5efbce772caf49d8dd016f310a97377)。

## 逐仓库具体案例、来源与可用原则

### 1. TripoGrowthLab：提示词比笼统的作品标题更有用

证据：[README](https://github.com/TripoGrowthLab/awesome-opus-5-5-prompts/blob/d083bc4d56564a26cfd2e590a34376362f8c8b4b/README.md)、[完整英语目录第 1 页](https://github.com/TripoGrowthLab/awesome-opus-5-5-prompts/blob/d083bc4d56564a26cfd2e590a34376362f8c8b4b/docs/catalog.en.1.md)、[有源码的条目索引](https://github.com/TripoGrowthLab/awesome-opus-5-5-prompts/blob/d083bc4d56564a26cfd2e590a34376362f8c8b4b/docs/with-code.md)。

- **AI orb UI motion**：目录保留的详细提示词要求暖灰背景、黑白 UI，珠光蓝紫桃色球体作为连续主角，经历 idle/listening/thinking/completion 等角色；同一白色容器改变尺寸/圆角，内容层单独安排退出和进入。要求 `seek(t)` 纯时间求值，闭式弹簧，上一周期的弹簧尾部补偿，球体 shader 使用整数频率的 `sin/cos(2πkt/L)`；给出 offscreen 640×640、每像素不超过 5 次 noise 调用等离线构建约束。**这是读到的提示词要求，不是本轮确认实现满足了这些要求**。[原作者帖](https://x.com/listudio/status/2104162483888062945) / [目录详情页](https://www.tripo3d.ai/3d-prompts/claude-opus-5-5-2104162483888062945)。
- **Beat-Synced 3D Sphere Drop**：提示词明确要求 Three.js、落球碰撞不同场景对象、对象按拍点亮；没有附可读取的该条演示实现源码，不能据此认定真实物理或稳态 60fps。[原作者帖](https://x.com/Gorden_Sun/status/2105302007896797351)。
- **对 OBS 的推论**：球体/容器/标题/辅助标签应成为各自的语义角色；连续性角色的位姿与 shader 相位共用模板时钟。球体是一个可参数化几何/程序材质候选，不能把整段视频当作前景透明素材。角色转换的节奏和遮挡关系比“加一个发光滤镜”更接近该案例的设计价值。
- **源码可得性边界**：当前 `docs/with-code.md` 仅列出 [WinterArc21/Battle-of-Austerlitz-Film](https://github.com/WinterArc21/Battle-of-Austerlitz-Film)。该外部仓库不在本轮六仓库审查范围，未检查其实现或许可；不能把 Tripo 的 MIT 当作它的许可。

### 2. BeatAPI：20 个来源线索，且明确声明未独立复现

证据：[README](https://github.com/BeatAPI/awesome-opus-5-5-videos/blob/a6af6772454710aab3ff8ee8a819f066701300e8/README.md)、[cases/catalog.json](https://github.com/BeatAPI/awesome-opus-5-5-videos/blob/a6af6772454710aab3ff8ee8a819f066701300e8/cases/catalog.json)。

- **Morphing UI states / twoclipping**：摘要说一个形状在多种界面状态之间变化；`techTags` 为 SVG。来源是 [原作者帖](https://x.com/twoclipping/status/2103273003555402193)，提示词外链去 yihui-dev 的固定提交文件。Beat 自身没有该动画的 renderer 源码。
- **Infinite zoom collage / koldo2k**：摘要说连接风景和复古物件的循环缩放、层叠图像和声音；[原作者帖](https://x.com/koldo2k/status/2103129343253778767)。X-Ray 目录对同一条原帖进一步注明外部生成模型与深度分层，不应将其宣传为纯程序几何示例。
- **网站作品集 / SaaS launch** 等条目只提供 Canvas、SVG、GSAP 标签和摘要。此类标题不足以支持“可直接用于直播”“比原插件画面更好”等判断。
- README 明确写：20 个条目来自 `yihui-dev/awesome-opus5-5-videos` 的 `1c092195b7bda246455827bc0be820be6d6a7b97`；Beat 未独立复现作品或验证全部模型声明。18 个 Skillry 预览可能展示 remake；这与原作者原片应分开判断。
- **对 OBS 的推论**：适合提取“一个绑定内容窗连续变形”“主体 + 边框 + 标签 + 遮罩协调”等模板方向；不应依赖此仓库校验脚本搭建原生动画引擎。任意用户图层更换尺寸时要重算 cover/contain、裁切范围和可读区域。

公开画廊 [beatapi.io/opus-5-5-videos](https://beatapi.io/opus-5-5-videos) 本轮 HEAD 请求返回 200；只验证链接可达，没有播放其视频。

### 3. Lemo：真实源码最丰富，但要拆开离线制作和实时内核

核心证据：[package.json](https://github.com/lemomo-ai/lemo-opuscar/blob/3a7132b996be0241c1ea0ab180d519f06e6f10b6/package.json)、[package-lock.json](https://github.com/lemomo-ai/lemo-opuscar/blob/3a7132b996be0241c1ea0ab180d519f06e6f10b6/package-lock.json)、[TECHNIQUE.md](https://github.com/lemomo-ai/lemo-opuscar/blob/3a7132b996be0241c1ea0ab180d519f06e6f10b6/TECHNIQUE.md)、[core/lib.js](https://github.com/lemomo-ai/lemo-opuscar/blob/3a7132b996be0241c1ea0ab180d519f06e6f10b6/core/lib.js)、[core/render/video.mjs](https://github.com/lemomo-ai/lemo-opuscar/blob/3a7132b996be0241c1ea0ab180d519f06e6f10b6/core/render/video.mjs)、[99 条画廊目录](https://github.com/lemomo-ai/lemo-opuscar/blob/3a7132b996be0241c1ea0ab180d519f06e6f10b6/styleboard/catalog.json)。

**确认在源码中存在的机制：**

- `core/lib.js` 提供 clamp / lerp / smoothstep 类插值、cubic in/out、back、spring、Mulberry seeded RNG、hash、value noise、单调三次插值、向量 track 和 envelope。它们是小而明确的数学工具，适合独立重写或在 MIT 范围内注明来源后移植。`spring` 默认阻尼低，不能机械套给所有风格；例如 Swiss 的说明明确拒绝弹跳。
- `TECHNIQUE.md` 要求 `window.render(t)` 确定性、禁止无种子随机和前帧状态依赖；这是契合定位/重播/多实例的设计原则。源码默认支持 Canvas 2D；3D 用 Three.js；部分后期为 WebGL2。
- `video.mjs` 每 worker 启动浏览器，调用 `window.render(f/FPS)`，JPEG screenshot 写入 ffmpeg `libx264`、`yuv420p`，最终连接 MP4。此路径既非 OBS `gs_*`，也不提供透明视频输出，更不证明渲染达到直播实时帧率。
- 锁文件观察到 Three.js **0.170.0（MIT）**、playwright-core **1.63.0（Apache-2.0）**；package.json 使用 `^` 范围。插件若引入任何依赖，应另按本仓库交付标准固定版本/校验值；无需把离线制作全套引入插件。
- README / TECHNIQUE 的安装估计：Node 20+、Python 3.11+、ffmpeg 7+；core 约 350 MB；voice 约 0.55 GB 再加 Whisper 模型；music numba 约 140 MB；采样包每个 70–400 MB；HDRI 约 12 MB。**这些是上游文档估计，本轮没有测量安装体积。** 其中大部分是离线音频、TTS、导出用途，和 OBS 原生几何模板无直接运行依赖关系。

| 实际读源码 / 看静态图的风格 | 已观察的画面与机制 | 本项目可独立设计的多层角色模板 | 边界 |
|---|---|---|---|
| **Swiss Motion / Five Rules for a Poster** | 静态 poster：大黑字、暖灰底、右侧海报、黑色矩形、单一红圆、细线信息层。`demo/film.js` 有 `revealText` 的矩形 clip、网格单元、circleState、posterMatrix、beat/bar；文字和海报共享构图时钟。 | 主内容窗 + 巨型标题/编号 + 信号色几何 + 基线/分隔线；入场由遮罩/位移完成，主体内容可绑定任意 OBS 实例。 | 原 demo 是有叙事进程的影片，不自动等于无缝 loop；背景、字样和历史海报构图不应照搬。 |
| **Bauhaus Shapes / COMPOUND** | 静态 poster：白底，蓝色斜坡，黄块、绿球、套叠圆和极端字号对比。`demo/film.js` 有相机矩阵、对数尺度插值、切口裁切、滚动圆的多尺度几何和逐区间求值。 | 圆形/矩形/楔形遮罩内容窗，强调色滚动/扩张几何，独立计数/标题，背景构图联动；可设计稳定循环的齿轮/圆环/轨道节奏。 | 看过 poster 和代码，未检查完整运动的视觉连贯性；不能引用该示例证明无限循环或 GPU 成本。 |
| **Hologram HUD / Volt · Spec Scan** | 静态 poster：青色线框自行车、暗背景网格、编号锚点、左侧注释、右侧参数板。`engine/hud.js` 是参数化确定性 Canvas：target boxes、leader lines、rolling numbers、plate、corners、parallax dot grid；内容和主题参数分离。 | 主体绑定角色 + 焦点框/环形扫描 + 跟随锚点的引线 + 独立参数/标题 + 可关背景网格。 | 应将装饰锚点绑定求值后的主体位姿，不能只让装饰在屏幕上循环而主体不参与；真实自行车模型与纯图层场景适用性不同。 |
| **Paper-cut Lightbox / The Last Night Train** | 静态 poster：多层蓝色山体/森林/桥梁，暖窗列车、月亮；前后遮挡清晰。`src/paper.js` 实际将 Canvas 轮廓做成 Three plane；有 seeded 纸纹、云纹、缓存纹理，以及 source-atop / destination-out / source-in 生成纸边明暗。 | 背景层 + 中景内容窗 + 前景洞口/纸边 + 主体 + 纹理/光色；不同深度的平移/相机运动创造视差，用户来源可进中景窗。 | 纸面纹理和轮廓可在编辑/加载时缓存；不能将原 Canvas 贴图创建或复杂 3D 后期直接搬到每帧。原示例有列车与故事，不是现成直播边框。 |

逐例固定源码/静态图/演示链接：

- Swiss：[film.js](https://github.com/lemomo-ai/lemo-opuscar/blob/3a7132b996be0241c1ea0ab180d519f06e6f10b6/styles/swiss-motion/demo/film.js) / [poster](https://raw.githubusercontent.com/lemomo-ai/lemo-opuscar/3a7132b996be0241c1ea0ab180d519f06e6f10b6/styles/swiss-motion/poster.jpg) / [MP4](https://github.com/lemomo-ai/lemo-opuscar/releases/download/films/swiss-motion.mp4)。
- Bauhaus：[film.js](https://github.com/lemomo-ai/lemo-opuscar/blob/3a7132b996be0241c1ea0ab180d519f06e6f10b6/styles/bauhaus-shapes/demo/film.js) / [poster](https://raw.githubusercontent.com/lemomo-ai/lemo-opuscar/3a7132b996be0241c1ea0ab180d519f06e6f10b6/styles/bauhaus-shapes/poster.jpg) / [MP4](https://github.com/lemomo-ai/lemo-opuscar/releases/download/films/bauhaus-shapes.mp4)。
- HUD：[hud.js](https://github.com/lemomo-ai/lemo-opuscar/blob/3a7132b996be0241c1ea0ab180d519f06e6f10b6/styles/hologram-hud/demo/engine/hud.js) / [poster](https://raw.githubusercontent.com/lemomo-ai/lemo-opuscar/3a7132b996be0241c1ea0ab180d519f06e6f10b6/styles/hologram-hud/poster.jpg) / [MP4](https://github.com/lemomo-ai/lemo-opuscar/releases/download/films/hologram-hud.mp4)。
- Paper-cut：[paper.js](https://github.com/lemomo-ai/lemo-opuscar/blob/3a7132b996be0241c1ea0ab180d519f06e6f10b6/styles/paper-lantern/demo/src/paper.js) / [poster](https://raw.githubusercontent.com/lemomo-ai/lemo-opuscar/3a7132b996be0241c1ea0ab180d519f06e6f10b6/styles/paper-lantern/poster.jpg) / [MP4](https://github.com/lemomo-ai/lemo-opuscar/releases/download/films/paper-lantern.mp4)。

[Release API](https://api.github.com/repos/lemomo-ai/lemo-opuscar/releases/tags/films) 本轮确实列有上述四个 MP4，大小分别为 Swiss 3,833,086 bytes、Bauhaus 10,149,311、HUD 37,095,352、Paper-cut 23,075,029；未下载/播放 MP4。Release 标签与其 asset 内容可能随后更新，不等同于固定源码提交的可复现构建证明。画廊 [lemomo-ai.github.io/lemo-opuscar/](https://lemomo-ai.github.io/lemo-opuscar/) HEAD 返回 200。

### 4. X-RayLuan：区分“公开案例”“公开提示词”“公开源码”

证据：[README](https://github.com/X-RayLuan/awesome-opus-5-5-video-prompts/blob/33fbd3712817cf056056b70a7c71d8313917e0ea/README.md)、[data/entries.json](https://github.com/X-RayLuan/awesome-opus-5-5-video-prompts/blob/33fbd3712817cf056056b70a7c71d8313917e0ea/data/entries.json)、[data/media.json](https://github.com/X-RayLuan/awesome-opus-5-5-video-prompts/blob/33fbd3712817cf056056b70a7c71d8313917e0ea/data/media.json)。

- **Glass & gold-leaf mosaic / LCSlates**：目录作者描述单 HTML、WebGL2、纯 JS、无图片/字体/音频文件，瓷砖离墙聚成鱼、鹤、鸽子，并有昼夜变化。条目 `prompt.type=verbatim`，但 `links=[]`，没有在该条目中提供可核查的演示源码链接。本轮只读到目录描述和提示词分类，没有看视频。视觉价值候选是“同一组实例几何在多个轮廓目标间重排”，不是一个给任意用户图层加马赛克的普通滤镜。[原帖](https://x.com/LCSlates/status/2102503027340988559)。
- **Infinite zoom collage / koldo2k**：目录描述明确依赖 Magnific MCP、外部图片/剪影/视频模型，以及 depth-layer parallax；不是纯代码绘出全部素材。可借鉴递归视窗、遮罩和深度层级，不应引入生成视频作为本项目运行基础。[原帖](https://x.com/koldo2k/status/2103129343253778767)。
- **90s C/C++ OpenGL demoscene / gandamu**：目录只写“作者报告使用 C/C++ 和 OpenGL”，该条 `not_shared`、`links=[]`，音乐来自 Second Reality。不能称此仓库提供可移植的 C++ 几何引擎，音乐也不随目录许可一起开放。[原帖](https://x.com/gandamu_ml/status/2102919394775220530)。
- **PDoomVideo** 具有明确外部源码链接 [JohnHeibel/PDoomVideo](https://github.com/JohnHeibel/PDoomVideo)，目录描述 p5.js / p5.brush + headless Chrome + ffmpeg；本轮未审查外部源码与许可证。只有这类明确 code link 才能作为继续审查的入口，不能将模型/工具标签等同于源码可用。

### 5. specimen-and-signal：访问受阻，保持未知

核查 [GitHub 页面](https://github.com/0xpratzyy/specimen-and-signal) 与 [API](https://api.github.com/repos/0xpratzyy/specimen-and-signal) 都是 404。没有可靠依据补写其 React/Three/Canvas 栈、许可证、视觉风格或 demo 地址。检索未返回可用的替代仓库页面。状态应记为 **BLOCKED（源资料无法访问）**，不是渲染质量 FAIL。

### 6. Li-Evan：有很细的循环规格，但仍是提示词目录

证据：[README](https://github.com/Li-Evan/awesome-opus-5.5-video-prompts/blob/7f731215e5efbce772caf49d8dd016f310a97377/README.md)、[data/showreel.yaml](https://github.com/Li-Evan/awesome-opus-5.5-video-prompts/blob/7f731215e5efbce772caf49d8dd016f310a97377/data/showreel.yaml)、[data/worlds.yaml](https://github.com/Li-Evan/awesome-opus-5.5-video-prompts/blob/7f731215e5efbce772caf49d8dd016f310a97377/data/worlds.yaml)。

- **One-shape UI morph / twoclipping**：YAML 中有完整长提示词：一个形状按 120 BPM 在 button / loader / player / slider / tabs / chart / command palette / toast 间连续变换；浅暖灰底、黑白部件、统一字体、微小过冲。`seek(t)` 不依赖上一帧；leading / trailing 两条边用不同弹簧造成拉伸；文字内容有各自的进退场窗口；循环末尾要求光标位置及速度也连续。该条 media 包含 320²/720² MP4 URL 和缩略图，但本轮未播放，因此不宣称这些要求都已完成。[原帖](https://x.com/twoclipping/status/2103273003555402193)。
- **Pixel-art wizard**：README 保留的详细提示词要求 128×96 offscreen 逻辑画布、整数缩放、约 24 色固定色板；idle → charge → cast → recover 循环；粒子池预分配、循环内零对象分配、姿态量化。这里的“固定 60Hz / 稳定 60fps”是提示词目标，未实测。本项目可借鉴低分辨率几何/像素遮罩、周期粒子、角色主次及色板约束；不必引入精灵视频。[原帖](https://x.com/majidmanzarpour/status/2102476258948927543) / [story.yaml 固定来源](https://github.com/Li-Evan/awesome-opus-5.5-video-prompts/blob/7f731215e5efbce772caf49d8dd016f310a97377/data/story.yaml)。
- **Paris night diorama**：`data/worlds.yaml` 提示词规定 240 秒循环，全部动画由时间 modulo 240、整数频率和确定性粒子驱动。这是长循环背景组织原则；有很多叙事细节和资产，不能据此承诺原生实现低成本。
- README 声称提示词逐字取自原作者并由 validator 验证；本轮没有重新抓取全部原帖或运行 validator，应保留为**仓库维护者的方法声明**。站点构建/检查代码不是所收录示例的动画引擎。画廊 [li-evan.github.io/awesome-opus-5.5-video-prompts/](https://li-evan.github.io/awesome-opus-5.5-video-prompts/) HEAD 返回 200。

## 目录复用与重复样本

在固定快照中按 **完全相同的原帖 URL** 比较，Beat 的 20 条与 X-Ray 的 84 条有 **8 条**重叠；Beat 与 Li-Evan 有 **14 条**重叠；X-Ray 与 Li-Evan 有 **26 条**重叠。Li 数据中抽取得到 332 个 `https://x.com/...` 主 url，和 README 的 334 条总数并不冲突（其他平台/其他 URL 不在此比较集合）。这是粗粒度最小重叠：大小写不同、跨帖补充、镜像 URL 未归一化，不宜当作完整去重率。

可核查的同源例子：twoclipping `2103273003555402193`（Beat/Li）；koldo2k `2103129343253778767`（Beat/X-Ray/Li）；gandamu `2102919394775220530`（Beat/X-Ray/Li）；LCSlates `2102503027340988559`（X-Ray/Li）。Tripo 也引用许多相同作者群体和同类条目，但本轮未做其 CMS 条目的完整来源归一化。共同作者/相同提示词不等于相同源码版本或同一预览视频。

Beat 对 yihui-dev 的固定上游 revision 有直接声明，这是明确的目录来源关系；其他目录之间仅凭内容重叠不能断言 fork、抄袭或共同后端。[Beat JSON](https://github.com/BeatAPI/awesome-opus-5-5-videos/blob/a6af6772454710aab3ff8ee8a819f066701300e8/cases/catalog.json) / [X-Ray JSON](https://github.com/X-RayLuan/awesome-opus-5-5-video-prompts/blob/33fbd3712817cf056056b70a7c71d8313917e0ea/data/entries.json) / [Li YAML 目录](https://github.com/Li-Evan/awesome-opus-5.5-video-prompts/tree/7f731215e5efbce772caf49d8dd016f310a97377/data)。

## 许可证与产品复用边界

| 仓库 | 本轮实际读取的许可规则 | 产品方向 |
|---|---|---|
| Tripo | [LICENSE](https://github.com/TripoGrowthLab/awesome-opus-5-5-prompts/blob/d083bc4d56564a26cfd2e590a34376362f8c8b4b/LICENSE) 为 MIT；[RIGHTS](https://github.com/TripoGrowthLab/awesome-opus-5-5-prompts/blob/d083bc4d56564a26cfd2e590a34376362f8c8b4b/RIGHTS.md) 限于原创 tooling/editorial，不重新许可第三方提示词、图片、视频、链接代码、商标、素材。 | 可以研究原则；不得把目录里的图/片/外部源码都视为 MIT。 |
| Beat | [LICENSE.md](https://github.com/BeatAPI/awesome-opus-5-5-videos/blob/a6af6772454710aab3ff8ee8a819f066701300e8/LICENSE.md)：原创说明/摘要 CC BY 4.0；校验代码为 MIT 式授权；原帖、提示词、媒体、姓名、链接页面没有由 Beat 授权。 | 只是来源索引，素材/示例复用另查作者。 |
| Lemo | [LICENSE](https://github.com/lemomo-ai/lemo-opuscar/blob/3a7132b996be0241c1ea0ab180d519f06e6f10b6/LICENSE) / [REUSE.toml](https://github.com/lemomo-ai/lemo-opuscar/blob/3a7132b996be0241c1ea0ab180d519f06e6f10b6/REUSE.toml)：原创代码 MIT；**所有 Markdown 与指定图片提示词 CC BY-NC 4.0**；演示影片/静态图 CC BY 4.0；第三方字体、音乐、模型、数据独立许可；若干在线 TTS 轨道和 Clawd/商标有例外。另授予文档用于制作自己影片的有限额外许可；把原文或改写文档纳入商业产品/服务超出该许可，需另取商业授权。 | 原生实现可独立设计；若移植源码，逐文件查 REUSE、demo/CREDITS、MIT 通知与第三方声明。不要将 STYLE.md/DEMO.md 全量作为插件内置提示词或文档出售/随产品打包。 |
| X-Ray | [LICENSE](https://github.com/X-RayLuan/awesome-opus-5-5-video-prompts/blob/33fbd3712817cf056056b70a7c71d8313917e0ea/LICENSE) / [RIGHTS](https://github.com/X-RayLuan/awesome-opus-5-5-video-prompts/blob/33fbd3712817cf056056b70a7c71d8313917e0ea/RIGHTS.md)：原创文档/tooling MIT；第三方媒体、提示词、音乐、代码权利归创作者。 | MIT 不覆盖第三方 demo。 |
| specimen-and-signal | 不可读取，许可未知。 | 不纳入复用依赖。 |
| Li-Evan | [LICENSE](https://github.com/Li-Evan/awesome-opus-5.5-video-prompts/blob/7f731215e5efbce772caf49d8dd016f310a97377/LICENSE) 及 README Rights：目录编排/摘要/翻译/导演工具包 CC BY 4.0；原提示词、视频和缩略图归创作者。 | 如引用目录自己的文本要署名；不据此复制原作者全部媒体或源码。 |

GitHub metadata 对某些混合许可显示 `NOASSERTION`；表中依据实际 LICENSE/RIGHTS/REUSE 内容，不能把 `NOASSERTION` 写成“没有许可证”。这部分是上游许可文件的整理，不替代具体复用行为的审查。

## 面向 OBS C++ 的候选内核（研究推论，尚未实现）

1. **时间与角色先统一**：每个模板定义内容/主体、标题、辅助标签、背景、前景装饰和 mask 角色；每个绑定记录稳定实例 ID 与适配规则。整体模板时钟输出每角色的位姿/透明度/几何/遮罩参数；主体与装饰共同求值。仅给背景加循环而主体完全无关，不等于多层角色模板。
2. **seekable 纯求值**：从 `phase=fract(t/period)` 生成周期几何；种子与实例 ID 固定，粒子位置按 time/seed 解析；弹簧或依赖状态的动画在导入/编辑时预计算或写成闭式。相位、几何和速度跨边界应一致；首尾同图不足以保证速度连续。复用本项目 `evaluateSceneAt()`，不得另建冲突坐标/旋转规则。
3. **几何/裁切直接渲染**：矩形/圆/环/楔形/曲线/条带适合生成顶点或 SDF mask；内容源先取得纹理，再按角色 mask 与变换绘制。用局部平面/少量深度层构建纸雕视差、空间窗和 HUD；主体纹理始终来自用户绑定的 OBS source，不需预渲染透明 WebM。
4. **分离静态纹理与每帧动画**：文字字形、纸纹、轮廓、网格可缓存；每帧更新少量参数或顶点。不在渲染线程运行 Python、Node、Playwright、ffmpeg、下载、音频分析或生成字体纹理。Lemo 的高分辨率后期、2× supersampling、TTS/音色下载和离线 grain 不应成为默认实时要求。
5. **可组合模板候选**：A 网格内容窗与数字层（Swiss）；B 几何窗 + 滚动/扩张信号形（Bauhaus）；C 主体锚点 + 扫描/引线/参数板（HUD）；D 前后层洞口 + 主体 + 轻视差（Paper-cut）；E 连续变形内容窗 + 独立文字换场（UI morph）。这些应先做少量可换源的原生样机，再按实际 OBS 输出选择，不按目录标题批量生成假预设。
6. **验证门槛**：检查 0/周期边界/中间时刻/随机跳转、两个同源实例不同参数、删除绑定、场景切换、尺寸变化、开关关闭；画面重点看 premultiplied alpha、裁切边缘、遮挡、中文可读性。1080p60 的 CPU/GPU/丢帧和资源增长必须在 OBS 33.0.1 真机测量；本轮对性能和画质均 **NOT RUN**。

## 本轮记录状态

| 项目 | 状态 | 证据 |
|---|---|---|
| 五个公开仓库固定提交、README、树和许可证读取 | PASS | `work/v3.6/catalog-b/*json` 和固定 SHA 原文快照；本文链接 |
| Lemo 四个 poster 的静态构图检查 | PASS | 本地 `lemomo-ai__lemo-opuscar__styles__*__poster.jpg`，已通过图片查看工具观察 |
| Lemo 四个 Release MP4 资产元数据存在 | PASS | `work/v3.6/catalog-b/lemo-films-release.json`；只核对元数据，未播放 |
| specimen-and-signal 源码/许可/演示检查 | BLOCKED | 网页/API 404，无法取得内容 |
| 完整视频播放、原作者模型声明复验 | NOT RUN | 不据目录或 poster 推断视频成片质量 |
| 示例本地运行、原生移植、真实 OBS 输出、性能测量 | NOT RUN | 本任务仅调研，无构建/产品修改 |
