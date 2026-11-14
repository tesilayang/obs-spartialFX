# v3.6 调研：六个 Opus/Claude 视频合集与原生预设可借鉴点

核验日期：2026-11-13（Asia/Shanghai）。任务仅研究，不修改生产代码。已阅读项目 `DEVELOPMENT_STANDARDS.md`。本报告的原生方案判断以项目既定 C++17 / Qt6 / OBS 33.0.1、`evaluateSceneAt()`、实例级绑定和合法 `gs_*` 图形上下文要求为边界。

## 结论及证据边界

- 六个指定地址中四个可公开读取；另两个 GitHub API 返回 404。`athemeroy/awesome-opus-5-5-videos` 已改名为 `athemeroy/awesome-claude-5-5-videos`，旧地址仍解析到同一仓库。四个可读仓库全部是**链接/提示词/研究合集**，不是可直接集成的 OBS 渲染器；文件树与根 README 均已检查。[S1–S6]
- 本次实际读了提示词、JSON/CSV、工作流、Python 工具及一个合集外匹配样例的 WebGL 源码；报告采用四张实际看过的样例静帧：UI 播放器、绿色产品舞台、巴黎雨夜、水面。**未播放完整视频，未验证运动顺滑度、循环无缝、音画同步、模型归属或 OBS 性能。**仓库标题及“高级/专业/viral”等形容词不作为质量证据。
- 最可迁移的是：固定角色的持续身份、共享舞台裁剪、匹配位置过渡、显隐交接、单一绝对时间求值、周期起止状态与速度一致。HTML、Playwright、FFmpeg、离线时间超采样本身不适合进入本项目运行链路。此处为对明确提示词与源码的工程推导，而非仓库已有 C++ 能力。[S7–S13]
- 已按要求调用 `web.run` 搜索及打开 GitHub/原始文件，但工具返回空载荷，无法生成可用 web citation reference。可核验证据来自 GitHub 官方 `gh api`、HTTPS Git clone、创作者/站点直接资源；以下均给出完整可追溯链接与 SHA。

## 一、逐仓库核验

| 用户指定仓库 | 公开访问与核验 HEAD | 实际内容 / 栈 | 授权与采用判断 |
|---|---|---|---|
| `yihui-dev/awesome-opus5-5-videos` | 可读；`main` / `92aee2f586ac0715cb3cbcb6384834f98a33383d`；API pushed_at `2026-10-13T14:46:44Z` | 704 个 `.md`（README + 703 个提示词）、一个 `data/videos.json`、MIT LICENSE、gitignore；没有 HTML/JS/C++ renderer。数据含 `tech_tags`、`has_remake`、原帖、海报、Skillry 页；这些标签描述外部复刻，不能当作仓库内实现。README 称 418 项有 Skillry remake；未运行或复核全部。[S1] | 根 LICENSE 为 MIT，版权 2026 yihui-dev；不能据此认定 X 视频、作者提示词、品牌或 Skillry 复刻的全部媒体权利都已转授。适合找案例和技术意图，不适合直接导入为 OBS 预设代码。[S1] |
| `athemeroy/awesome-opus-5-5-videos` | 可读；API canonical `athemeroy/awesome-claude-5-5-videos`；`main` / `87e5cbfaa4bf1f6516a6c69be58c42a9b85a1041`；pushed_at `2026-10-11T00:47:21Z` | 研究目录；424 行 `collection-index.csv`、168 案例审计、静帧、图表、13 个 Python 脚本、文档。Python 负责提取缩略图、统计、生成图表/索引；`generate_case_thumbnails.py` 明说完整 MP4 和九帧证据不随仓库分发，生成过程需要私有本地预览与 receipts。[S2,S10] | 原创文字/表格/图表 CC BY 4.0；`THIRD_PARTY.md` 明确排除 X 帖、视频、音乐、创作者提示词、截图、商标、外部代码与样例静帧。适合借鉴证据分层与风格分解；不是动画引擎。[S2,S10] |
| `opuvideo/awesome-claude-video` | metadata、`commits/HEAD`、递归 tree 三个 API 均 `404 Not Found`；无可验证 HEAD | 本次无法读取，不能分类为 renderer，也不能替换成同名近似仓库。[S3] | 无法核验源码、许可证、样例。404 只能证明当前凭据/公开接口不可访问；不能区分删除、私有、改名或地址错误。 |
| `joeseesun/opus-video-prompts` | 可读；`main` / `7977c807781d4ba7c235247b8899a72575a30d00`；pushed_at `2026-09-27T00:16:11Z` | README、54 行 `cases.json`、18 个提示词 Markdown；没有可执行代码、项目清单或视频素材。提示词所需栈从 HTML/Canvas/SVG/Three.js 到 Remotion、Playwright/FFmpeg；这是请求和作者叙述，不是已交付的渲染实现。[S4,S7] | 文件树无根 LICENSE，API license 为 null。提示词页声明版权属原作者、转载请保留作者与原帖；这不等于一个通用软件开源许可。适合阅读抽象方法，不复制整套提示词/媒体到产品。[S4,S7] |
| `chuspseism/awesome-opus-5-5-videos` | metadata、`commits/HEAD`、递归 tree 三个 API 均 `404 Not Found`；无可验证 HEAD | 无可验证内容；不以其他人的同名项目替代。[S5] | 未知；同上访问限制。 |
| `zhuyansen/awesome-opus-5.5-video` | 可读；`main` / `e3bfd49f4a1922513b5df34781af2387dd4ad8a9`；pushed_at `2026-11-12T13:38:30Z` | 2770 条 `cases.json`、分类 Markdown、README、中英索引与一个同步 workflow；没有动画 renderer。工作流使用 Node 22 从 `zhuyansen/jasonzhu.ai` 拉取 `build-repo.mjs` 与站点案例库生成目录，说明它是站点目录镜像。[S6,S11] | 文件树无 LICENSE，API license null；README 明说作品归创作者、收录不授予许可。案例模型字段是 creator report，README 明示未独立复现。适合大范围检索，再回原作者证据核验。[S6,S11] |

四个可访问仓库的 metadata 都显示 `fork=false`，但这只表示 GitHub fork 关系，不表示资料互不重合。[S1,S2,S4,S6]

## 二、具体案例：看到了什么、只读到了什么

### A. 同一 UI 形态连续变化：最强的确定性运动参考

来源是 @twoclipping 原帖 `2103273003555402193`；Yihui 提供 `twoclipping-402193` 提示词和 Skillry 页面，Joeseesun 的 `15-ui-morph-loop.md` 提供完整文字，Zhuyansen 同一 ID 指向同一原帖并标为 full prompt，resources 为空。三个目录不是三份独立作品。[S7,S8,S9]

**本次看见的静帧**：浅暖灰方形底色，大面积留白，居中的黑色圆角播放器面板；左侧彩色封面、白色主标题、灰色副标题、细进度条、少量控制图标。能确认构图与层级；不能由这张图确认 morph、spring 或无缝循环。[S8]

**读到的提示词要求**：一个连续元素依次变成 button、loader、player、slider、toggle、tabs、chart、command palette、toast，再回到 button；尺寸、圆角、色彩随绝对 `seek(t)` 计算；弹簧使用闭式 step response，连续目标变化用多个响应叠加；tab indicator 两边使用不同弹簧形成拉伸；文字内容分别安排 enter/exit；最后一帧连光标位置与速度都须接回第一帧。外部导出链是 HTML + NumPy 音乐分析 + Playwright + 四个子帧/帧 + FFmpeg tmix，**不是原生实现，也未验证作者真的逐项完成**。[S7]

**建议迁移（工程推导）**：预设内用一个稳定的 `container` 角色承载 `image/title/subtitle/progress/controls` 子角色；插值容器宽高/圆角，单独调度文字显隐；场景源实例只绑定相应角色，不随“镜头切换”销毁重建。Native 中可用闭式 spring 或预先求值的有限曲线保持拖动/定位可重复。对循环实际验证 `position/scale/opacity/mask/velocity` 起止一致，不只比较最后一张图。

### B. 绿色产品舞台 + 同一封面贯穿多镜头

Yihui 的 `brainextends-606193.md` 提示词包含具体时间表与 persistent player 定义；原作者是 @brainextends，技术标签 `GLSL · SVG` 是外部 remake 标记。[S12]

**本次看见的静帧**：外层绿色环境，内层近黑圆角舞台；小型搜索框与一条绿色选中结果位于舞台中部，底端有一条窄播放器。面板内部内容和背景层级明确，播放器视觉上较次要。预览分辨率小，不能评判字体抗锯齿、运动模糊或细节质量。[S12]

**读到的提示词要求**：同一 hero artwork 从搜索结果移交到 album detail、居中放大、左图右文、多个浮动封面、向中心会聚；播放器大部分时间固定在舞台底部；oversized cover 与 camera move 都裁到同一圆角舞台；标题先退后进；避免随机漂浮和任意整屏 crossfade；18 秒绝对时间 seek、3–5 个时间子帧、真实 60fps 是交付要求，**未当作实际测得表现**。[S12]

**建议迁移（工程推导）**：定义 `stageMask/hero/sideCards/title/player`，hero 使用同一个实例绑定和连续轨道，sideCards 用固定 phase offsets 与有限最大数量；在多镜头之间持续保留 stage/player 作为视觉锚点。共享 rounded-rect mask 一次配置，多层复用；文字角色有独立可见窗口。原提示词用 Spotify 品牌，本项目只借构图与运动关系，使用用户自己的素材、配色和标识。

### C. 巴黎雨夜：适合常驻循环背景，但不是多图层产品片

Athemeroy 的原帖入口 @minosdevs `2103112945341251920`，文档引用作者后续法文提示词 `2103112948478570675`；提示词请求 standalone HTML/Canvas、240 秒确定性无缝循环、480×270 nearest-neighbor 放大到 1080p、60fps。[S13]

**本次看见的静帧**：深蓝紫夜空、层叠云、中央偏右的金色埃菲尔铁塔、左侧近景楼房、零散暖黄窗户、深色远景/水面，画面中可见斜向雨线。单帧不能确认降雨方向/速度、视差或真正的 240 秒闭环。文档自己的 review limit 也不保证所请求指标达成。[S13]

**建议迁移（工程推导）**：将用户底图、远景、近景、雨/闪光几何分成固定角色；几何雨线由 seed + index + 周期相位计算，保持数量上限，不每帧随机生成。前后景采用小幅不同频率但公共循环周期的平移，形成受控纵深；材质/原画需要另行取得，不把样例截图作为产品背景。像素美术可提供 nearest 模式，不能全局强制覆盖清晰 UI 的采样策略。

### D. 水面：有真实可运行 WebGL 源，但不等于廉价或确定性循环

Athemeroy 目录提供 @Aurelien_Gz 原帖 `2102786378282987591` 与匹配仓库 [Aureliengmz/clearwater](https://github.com/Aureliengmz/clearwater)。本次额外浅克隆该样例，HEAD `4bc826134321043a25df3c2b6fed16fb7b9241e8`，读了 README、LICENSE 与 `index.html`，未启动 demo。[S14]

**本次看见的目录静帧**：青绿色浅水、明亮反射和水下图案；图像本身不能证明水体物理、实时帧率、无资产或生成过程。[S14]

**本次源码确认**：一个真实 HTML/WebGL2 renderer；显式获取 float color-buffer extension；源码包含 FFT ocean spectrum、wave-equation ripple render targets、caustics pass、main shader、post/glare、pointer input、`requestAnimationFrame`、adaptive resolution。底部包含 base64 JPEG seabed texture，并有 Python `make_pebbles.py` 生成工具。因此 README 的“no external assets”是无外部运行时取材，**不是没有材质数据**。[S14]

**本次源码发现的限制**：frame loop 的 `tSim += dt`、波纹状态推进、交互输入和 camera inertia 有历史状态；`?t=` 冻结截图入口并不能自动证明任意 seek 或无缝周期。多 pass + 模拟 + 自适应分辨率也说明 README “real-time”不能直接推出适合 OBS 同时运行多个 source 的预算。根 LICENSE 为 MIT，2026 Lumaris；源码许可证不替代本项目实现审核。[S14]

**建议迁移（工程推导）**：作为“真实实时 shader 源码”的对照材料；初版预设只吸收低振幅周期折射/扫光等可限预算的外观原则，不把整套 FFT/ripple simulation 生搬进常驻模板。若未来实现必须单列 GPU 验收，依照项目标准测实际 OBS 输出和时耗。

### E. 高端产品片提示词：可复用布局语法，不能把导出手段当引擎

Joeseesun 的 `16-high-end-product-video.md` 提供 masked type reveal、match cut、真实素材墙、3D carousel、主片手机框、results panel、统计大字、三字 ticker 等明确构图；每 bar 一个主动作、一个 accent color、留白和一致 camera language 是文本要求。它的直接收录来源为 YouMind，属于转录提示词，**本次没有从原作者重现这份提示词的完整执行记录，也没有观看成片**。[S15]

工程上可提炼成 `heroClip/supportClips/phoneFrame/title/stats` 角色及 staged handoff；不能采用“抽帧 JPEG 每帧换 src、await decode、Playwright 三子帧、FFmpeg tmix”作为 OBS 实时方案。这里与 A 的闭式时间模型可共用，但多素材墙、相机、mask 的组合必须受上限控制。[S15]

## 三、重复与共享目录：按原帖 ID 去重，而非仓库数量

本次对**主数据集**提取 `/status/{id}`，不混入引用帖、方法文档和站点导航：Yihui `data/videos.json.post_url`；Athemeroy `data/collection-index.csv.post_id`；Joeseesun `cases.json.url`；Zhuyansen `cases.json.cases[].id`。统计结果保存在 `work/v3.6/catalog-a/overlap-dataset.json`，数据来源固定到本报告 SHA。[S1,S2,S4,S6]

| 集合 | 数据行 | 唯一原帖 ID |
|---|---:|---:|
| Yihui | 703 | 703 |
| Athemeroy | 424 | 424 |
| Joeseesun | 54 | 42 |
| Zhuyansen | 2770 | 2770 |

| 两集合 | 共享唯一原帖 ID |
|---|---:|
| Yihui / Athemeroy | 24 |
| Yihui / Joeseesun | 5 |
| Yihui / Zhuyansen | 62 |
| Athemeroy / Joeseesun | 20 |
| Athemeroy / Zhuyansen | 116 |
| Joeseesun / Zhuyansen | 40 |

解释限度：是按明确 ID 求交的可复核统计，不等于视频文件去重，不证明作者之间复制关系；同原帖也可能有多个附件。Joeseesun 的 54 行中有 42 个不同 X 原帖，其余 12 行指向 9 个不同外部 URL（包括提示词合集、文章、GitHub 项目与 Bilibili）；外部 URL 没有混入 ID 求交，54 行也不能当成 54 个独立 X 作品。@twoclipping 的 UI loop 是 Y/J/Z 共享实例；Athemeroy 的 `prompt-overlap.json` 还研究两个创作者提示词的 n-gram 重叠，但那是其作者计算，不等于本次跨仓库重复统计。[S7,S8,S9,S16]

## 四、对 v3.6 原生实现的具体输入（建议，不是本次已实现）

1. **模板是角色关系和时间函数。** `hero/title/support/stageMask/accentGeometry` 绑定稳定的场景实例 ID，每个角色有独立 transform、alpha、可见窗口与裁剪参数。一次绑定后，各子镜头继续操作相同实例，允许同源多实例角色不同；不按名称/列表索引绑定。来源启发是 A 的连续容器、B 的 persistent player 与 artwork handoff。[S7,S12]
2. **两类时间轨足够支撑本组参考。** 有限关键帧负责 enter/hold/exit/镜头交接，解析周期函数负责常驻 drift、雨、轨道、扫光等几何。都以绝对场景时间、稳定 seed 与公共 loop phase 求值，再走 `evaluateSceneAt()`；不要把 JS/Node/browser 或离线渲染作为运行时依赖。纯时间函数优于逐帧积累，是从 A 的 prompt 和 D 的反例推导。[S7,S14]
3. **几何与内容共同编排。** rounded stage mask、双边拉伸 indicator、环/线/点作为可独立调色和周期控制的原生图元，而非播放透明 WebM；几何数量、顶点数、shader pass、mask 分辨率设硬上限。每个方案要讲清哪些图层跟随、哪些固定，避免仅增加装饰粒子却没有角色关系。[S7,S12；预算上限为本报告建议]
4. **连贯优先于持续随机运动。** 共享运动方向、同一 anchor 的匹配缩放、成组错峰、主次节奏；标题显隐窗口避免重叠；常驻播放器/边框作稳定锚点。Loop 边界的状态与一阶速度需要一致；mask 参数、随机 seed、颜色相位也必须闭合。[S7,S12]
5. **针对真实 OBS 验收。** 先检查静帧布局，再检查多处跳转、反复定位与完整播放、起止交界、暂停恢复、来源删除/绑定失效与尺寸变化。离线 3–5 子帧的运动模糊不能当作免费效果；未经实际测量不承诺 60fps 或多实例预算。目录里的点赞、浏览数、模型标记不进入效果质量或性能验收。[S7,S12,S14；验收方式按本地开发标准]

建议首轮只验证三种有清晰角色契约的方向：**连续卡片/控件 morph、舞台内 hero 与 side cards 交接、分层常驻背景 + 周期几何**。这些是设计研究候选，不代表本报告授予新增功能实现确认。

## 五、可追溯来源

- **S1 — Yihui 官方 API / 固定源码**：[metadata](https://api.github.com/repos/yihui-dev/awesome-opus5-5-videos)、[HEAD tree](https://github.com/yihui-dev/awesome-opus5-5-videos/tree/92aee2f586ac0715cb3cbcb6384834f98a33383d)、[README](https://github.com/yihui-dev/awesome-opus5-5-videos/blob/92aee2f586ac0715cb3cbcb6384834f98a33383d/README.md)、[dataset](https://github.com/yihui-dev/awesome-opus5-5-videos/blob/92aee2f586ac0715cb3cbcb6384834f98a33383d/data/videos.json)、[LICENSE](https://github.com/yihui-dev/awesome-opus5-5-videos/blob/92aee2f586ac0715cb3cbcb6384834f98a33383d/LICENSE)。
- **S2 — Athemeroy 官方 API / 固定源码**：[旧地址 metadata / canonical 信息](https://api.github.com/repos/athemeroy/awesome-opus-5-5-videos)、[tree](https://github.com/athemeroy/awesome-claude-5-5-videos/tree/87e5cbfaa4bf1f6516a6c69be58c42a9b85a1041)、[README](https://github.com/athemeroy/awesome-claude-5-5-videos/blob/87e5cbfaa4bf1f6516a6c69be58c42a9b85a1041/README.md)、[dataset](https://github.com/athemeroy/awesome-claude-5-5-videos/blob/87e5cbfaa4bf1f6516a6c69be58c42a9b85a1041/data/collection-index.csv)、[LICENSE](https://github.com/athemeroy/awesome-claude-5-5-videos/blob/87e5cbfaa4bf1f6516a6c69be58c42a9b85a1041/LICENSE)。
- **S3 — 不可读仓库检查**：[opuvideo metadata](https://api.github.com/repos/opuvideo/awesome-claude-video)、[commits/HEAD](https://api.github.com/repos/opuvideo/awesome-claude-video/commits/HEAD)、[tree](https://api.github.com/repos/opuvideo/awesome-claude-video/git/trees/HEAD?recursive=1)。本地原始响应：`work/v3.6/catalog-a/opuvideo_awesome-claude-video-*-error.txt` 与 `*-metadata.json` 等。
- **S4 — Joeseesun 官方 API / 固定源码**：[metadata](https://api.github.com/repos/joeseesun/opus-video-prompts)、[tree](https://github.com/joeseesun/opus-video-prompts/tree/7977c807781d4ba7c235247b8899a72575a30d00)、[README](https://github.com/joeseesun/opus-video-prompts/blob/7977c807781d4ba7c235247b8899a72575a30d00/README.md)、[cases.json](https://github.com/joeseesun/opus-video-prompts/blob/7977c807781d4ba7c235247b8899a72575a30d00/cases.json)。
- **S5 — 不可读仓库检查**：[chuspseism metadata](https://api.github.com/repos/chuspseism/awesome-opus-5-5-videos)、[commits/HEAD](https://api.github.com/repos/chuspseism/awesome-opus-5-5-videos/commits/HEAD)、[tree](https://api.github.com/repos/chuspseism/awesome-opus-5-5-videos/git/trees/HEAD?recursive=1)。原始响应在 `work/v3.6/catalog-a/chuspseism_awesome-opus-5-5-videos-*`。
- **S6 — Zhuyansen 官方 API / 固定源码**：[metadata](https://api.github.com/repos/zhuyansen/awesome-opus-5.5-video)、[tree](https://github.com/zhuyansen/awesome-opus-5.5-video/tree/e3bfd49f4a1922513b5df34781af2387dd4ad8a9)、[README](https://github.com/zhuyansen/awesome-opus-5.5-video/blob/e3bfd49f4a1922513b5df34781af2387dd4ad8a9/README.md)、[cases.json](https://github.com/zhuyansen/awesome-opus-5.5-video/blob/e3bfd49f4a1922513b5df34781af2387dd4ad8a9/cases.json)。
- **S7 — 实际读过的 UI morph 原文**：[Joeseesun prompt 15](https://github.com/joeseesun/opus-video-prompts/blob/7977c807781d4ba7c235247b8899a72575a30d00/prompts/15-ui-morph-loop.md)、[Yihui 同例](https://github.com/yihui-dev/awesome-opus5-5-videos/blob/92aee2f586ac0715cb3cbcb6384834f98a33383d/prompts/twoclipping-402193.md)、[创作者原帖](https://x.com/twoclipping/status/2103273003555402193)。原帖未完整视频播放，文字以固定 repo 中转录为准。
- **S8 — 实际获取页面与静帧**：[Skillry UI 页](https://skillry.dev/ai-videos/opus-5-5/twoclipping-402193)、[original poster](https://media.skillry.dev/opus-5-5/twoclipping-402193/original.9c664f011f.webp)。HTML 中实际为 `original.mp4` 与 `remake.mp4` 两个 video 标签；未发现可直接移植的渲染源码，也未将页面标记“live remake”等同浏览器实时执行动画。页面临时快照为 `work/v3.6/catalog-a/skillry-twoclipping.html`。
- **S9 — Zhuyansen 同例索引**：[motion.md](https://github.com/zhuyansen/awesome-opus-5.5-video/blob/e3bfd49f4a1922513b5df34781af2387dd4ad8a9/cases/motion.md)、[prompt 页面入口](https://jasonzhu.ai/en/prompts/claude-opus-5-5/2103273003555402193)。后者仅列作 repo 提供的入口，未用其未读取内容支撑额外结论。
- **S10 — Athemeroy 授权边界与脚本**：[THIRD_PARTY.md](https://github.com/athemeroy/awesome-claude-5-5-videos/blob/87e5cbfaa4bf1f6516a6c69be58c42a9b85a1041/THIRD_PARTY.md)、[generate_case_thumbnails.py](https://github.com/athemeroy/awesome-claude-5-5-videos/blob/87e5cbfaa4bf1f6516a6c69be58c42a9b85a1041/scripts/generate_case_thumbnails.py)。
- **S11 — Zhuyansen 目录生成来源**：[sync.yml](https://github.com/zhuyansen/awesome-opus-5.5-video/blob/e3bfd49f4a1922513b5df34781af2387dd4ad8a9/.github/workflows/sync.yml)。
- **S12 — 实际读过的产品舞台 prompt 和看过的静帧**：[brainextends prompt](https://github.com/yihui-dev/awesome-opus5-5-videos/blob/92aee2f586ac0715cb3cbcb6384834f98a33383d/prompts/brainextends-606193.md)、[poster](https://media.skillry.dev/opus-5-5/brainextends-606193/preview.webp)、[原帖](https://x.com/brainextends/status/2103801834930606193)、[demo 入口](https://skillry.dev/ai-videos/opus-5-5/brainextends-606193)。未播放 demo。
- **S13 — 巴黎雨夜**：[固定目录说明](https://github.com/athemeroy/awesome-claude-5-5-videos/blob/87e5cbfaa4bf1f6516a6c69be58c42a9b85a1041/docs/domain-style-atlas.md)、[本次看过的静帧](https://github.com/athemeroy/awesome-claude-5-5-videos/blob/87e5cbfaa4bf1f6516a6c69be58c42a9b85a1041/assets/case-thumbnails/2103112945341251920.webp)、[原帖](https://x.com/minosdevs/status/2103112945341251920)、[作者提示词入口](https://x.com/minosdevs/status/2103112948478570675)。提示词长度/输出要求来自该目录对原帖的明确转述，未独立获取完整法文原文。
- **S14 — 水面目录与匹配源代码**：[目录证据说明](https://github.com/athemeroy/awesome-claude-5-5-videos/blob/87e5cbfaa4bf1f6516a6c69be58c42a9b85a1041/docs/evidence-examples.md)、[看过的静帧](https://github.com/athemeroy/awesome-claude-5-5-videos/blob/87e5cbfaa4bf1f6516a6c69be58c42a9b85a1041/assets/case-thumbnails/2102786378282987591.webp)、[Clearwater README](https://github.com/Aureliengmz/clearwater/blob/4bc826134321043a25df3c2b6fed16fb7b9241e8/README.md)、[源码，重点 L65/L127/L231/L283/L868](https://github.com/Aureliengmz/clearwater/blob/4bc826134321043a25df3c2b6fed16fb7b9241e8/index.html#L868)、[LICENSE](https://github.com/Aureliengmz/clearwater/blob/4bc826134321043a25df3c2b6fed16fb7b9241e8/LICENSE)、[live demo 入口](https://aureliengmz.github.io/clearwater/)。Demo 未运行。
- **S15 — 高端产品片 prompt 转录**：[Joeseesun prompt 16](https://github.com/joeseesun/opus-video-prompts/blob/7977c807781d4ba7c235247b8899a72575a30d00/prompts/16-high-end-product-video.md)。其中 YouMind URL 仅作原文件自报收录来源，本报告未以 YouMind 页面作独立实证。
- **S16 — Athemeroy 作者自己的重叠研究**：[prompt-overlap.json](https://github.com/athemeroy/awesome-claude-5-5-videos/blob/87e5cbfaa4bf1f6516a6c69be58c42a9b85a1041/data/prompt-overlap.json)。

## 六、核验命令及本地证据位置

实际执行主要命令：`gh api repos/OWNER/REPO`、`gh api repos/OWNER/REPO/commits/HEAD`、`gh api repos/OWNER/REPO/git/trees/HEAD?recursive=1`、`git clone --depth 1 https://github.com/OWNER/REPO.git work/v3.6/catalog-a/NAME`、`git -C ... rev-parse HEAD`；使用 PowerShell `ConvertFrom-Json` / `Import-Csv` 提取 ID、`Sort-Object -Unique` 去重与集合求交。未运行任何生产构建或测试，未添加可执行产品功能。

metadata/HEAD/tree 原始 JSON、错误输出、四个固定 HEAD 浅克隆、Clearwater 样例克隆、两个海报与 Skillry HTML 都在 **`E:/obs spartialFX develop/work/v3.6/catalog-a/`**。这些只作为临时研究证据；无媒体被复制为最终产品素材，无外部源码被并入本仓库生产目录。

本轮状态：公开内容与静帧读取 **PASS**；两个不可访问仓库 **BLOCKED**；完整视频播放、示例运行、插件编译与 OBS 性能验证 **NOT RUN**。
