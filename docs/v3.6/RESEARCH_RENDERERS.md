# v3.6 实际渲染项目核查

核查日期：2026-11-13。研究范围：852wa/JIZURA、DanteAlighieri13210914/pv-tool、chthollyphile/folia-major。只读取公开仓库，未复制代码到产品，未安装运行这些项目，未观看完整演示视频。截图序列来自用户，不能据此推定原视频具体算法。

## 1. JIZURA：最贴近“组合动画零件”的参考

- 本次固定提交：`4c8278cef161b38b132055e7b5efa112a5d41790`。
- [README.en.md](https://github.com/852wa/JIZURA/blob/4c8278cef161b38b132055e7b5efa112a5d41790/README.en.md) 将其定义为浏览器歌词动效制作器；输入歌词、可选音频、生成变化，浏览器预览及视频导出。README 宣称的零件数量属于上游自述，本次未逐个验证所有效果。
- [注册表](https://github.com/852wa/JIZURA/blob/4c8278cef161b38b132055e7b5efa112a5d41790/src/05b_registry.js) 实际区分布局、入场、停留、退场、装饰、文字处理、背景、镜头、画面特效、转场。各组有名称、风格标签、权重与 pack；装饰可选多个。这是可检查的代码结构。
- [规划器](https://github.com/852wa/JIZURA/blob/4c8278cef161b38b132055e7b5efa112a5d41790/src/08_planner.js) 保存 seed、风格、配色、节拍和单段覆盖；`centerFree` 为主体留出中间空间，统一选项控制重复段落的一致性。
- [装饰包](https://github.com/852wa/JIZURA/blob/4c8278cef161b38b132055e7b5efa112a5d41790/src/11p_decor.js) 包含测量/HUD、几何、粒子、手绘标记等函数；按内容包围盒布局，并保留上一有效包围盒，避免文字隐藏时装饰跳到中央。
- [镜头及转场包](https://github.com/852wa/JIZURA/blob/4c8278cef161b38b132055e7b5efa112a5d41790/src/11p_x2_camtr.js) 有按时间/拍相位求镜头的函数。`xqPolyIris` 用多边形裁切新旧画面并描边，`xqRingRipple` 用同心环，另有波浪擦除、方向柔化擦除。证明这些视觉元素可由几何和时间计算生成，不必预制透明视频。
- [渲染器](https://github.com/852wa/JIZURA/blob/4c8278cef161b38b132055e7b5efa112a5d41790/src/09_render.js) 使用 Canvas 和 scratch 画布，按时间查找 cut；也含缓存、后处理等。不能把这套 Canvas 调用直接当作 libobs API。
- [许可证](https://github.com/852wa/JIZURA/blob/4c8278cef161b38b132055e7b5efa112a5d41790/LICENSE)：MIT。若以后采用具体代码，应逐文件保留声明并记录改写范围；本轮只取结构与视觉原则。

**可借鉴的审美**：保持同一色板和图形语汇；同一段只安排少数主动作；图形围绕内容边界组织；入场、稳定展示、退场分别设计。既有低动感/抒情路线，也有几何/波普/故障路线，不能把所有零件同时叠到直播主体上。

**适配方式**：将“文字/句子”槽位改成用户绑定的图层实例；将背景、装饰、镜头、转场拆成预设内部组件。保留确定的 seed 和绝对时间求值。其导出 PNG/MP4/AE 的流程无需引入本插件。

## 2. PV Tool：模板结构和用户参考画面关联很强

- 本次固定提交：`0a3336e2d20ab6bd38b4bc191170fb158d9493fd`。
- [README](https://github.com/DanteAlighieri13210914/pv-tool/blob/0a3336e2d20ab6bd38b4bc191170fb158d9493fd/README.md) 说明项目使用 PixiJS 和 TypeScript；本次读取了实际引擎与模板，不仅依据介绍判断。
- [类型契约](https://github.com/DanteAlighieri13210914/pv-tool/blob/0a3336e2d20ab6bd38b4bc191170fb158d9493fd/src/core/types.ts) 的 TemplateConfig 明确含 palette、effects、bpm、animationSpeed、postfx 与 features；EffectEntry 指定 type/layer/config；UpdateContext 含时间、拍强度、画面尺寸及 motionTargets。
- [引擎](https://github.com/DanteAlighieri13210914/pv-tool/blob/0a3336e2d20ab6bd38b4bc191170fb158d9493fd/src/core/engine.ts) 实际建立 Pixi 容器、背景、装饰、文字和 overlay，并维护播放时钟、BeatProvider 与效果生命周期。
- [几何模板](https://github.com/DanteAlighieri13210914/pv-tool/blob/0a3336e2d20ab6bd38b4bc191170fb158d9493fd/src/templates/geometric.ts) 将斜线底纹、边框、大小方形和文字组合在统一色板内。
- [波普模板](https://github.com/DanteAlighieri13210914/pv-tool/blob/0a3336e2d20ab6bd38b4bc191170fb158d9493fd/src/templates/popArt.ts) 组合多条不同相位的倾斜平行四边形、网点和棋盘格；这是用少数几何规则构成丰富画面的具体例子。
- [斩击模板](https://github.com/DanteAlighieri13210914/pv-tool/blob/0a3336e2d20ab6bd38b4bc191170fb158d9493fd/src/templates/kineticSplit.ts) 将 diagonalSplit、边框、分层字与暗角组合，提供旋转速度、夹角与初始角度等参数。它与用户截图中的斜向大色块具有可比较的图形构成，但没有证据证明用户原视频来自该模板。
- [当前许可证](https://github.com/DanteAlighieri13210914/pv-tool/blob/0a3336e2d20ab6bd38b4bc191170fb158d9493fd/LICENSE) 为作者自定义 Non-Commercial License。README 也说明已从 AGPL 改为非商业许可。本方案不把该仓库代码、模板数据或资产加入 OBS Spatial FX，也不将其描述为可随意复用的通用开源库。

**可借鉴的审美**：两到三种主色、几何元素反复出现、前后层密度不同、主体留白、重复图案的相位错开、少量后处理统一质感。直接扩大故障、抖动或闪白强度容易降低直播可读性，应另设动感档位。

**适配方式**：独立设计“配色＋图层角色＋动作＋几何装饰”的原生模板。自动轮廓提取、运动检测及歌词系统不列入 v3.6 首批范围；透明主体由用户已有素材 Alpha 决定。

## 3. Folia Major：实时运行、资源管理与分层场景可借鉴

- 本次固定提交：`b0d2d74e4f4458879f6a53c31bcdb4144d06d754`。
- [场景说明](https://github.com/chthollyphile/folia-major/blob/b0d2d74e4f4458879f6a53c31bcdb4144d06d754/src/components/scenes/README.md)、[场景契约](https://github.com/chthollyphile/folia-major/blob/b0d2d74e4f4458879f6a53c31bcdb4144d06d754/src/components/scenes/definition.ts)：纯数据 manifest、按需加载 runtime、frame/resize/dispose、锚点、表面、质量档位、首次成帧通知及 seek 等事件。上游坐标是米、Y 向上，不能移植成项目当前 X 右/Y 下的第二套坐标。
- [Visualizer 代码地图](https://github.com/chthollyphile/folia-major/blob/b0d2d74e4f4458879f6a53c31bcdb4144d06d754/src/components/visualizer/README.md)：背景/视频层由共享外壳只画一次；样式有 registry、预热和真实小图预览。切换样式不必重建背景与视频。
- [花火 runtime](https://github.com/chthollyphile/folia-major/blob/b0d2d74e4f4458879f6a53c31bcdb4144d06d754/src/components/visualizer/hanabi/createHanabiPixiRuntime.ts) 维护场景缓存、换歌交接与资源销毁；每帧读取时间，具有前景/场景/overlay 层。它是浏览器/Pixi 实现，不是本地 C++ 插件。
- [Pendolo 钟表装饰](https://github.com/chthollyphile/folia-major/blob/b0d2d74e4f4458879f6a53c31bcdb4144d06d754/src/components/visualizer/pendolo/pendoloClockworkScene.ts) 的帧输入有相位、齿轮角、拍点增量、颜色、装饰强度、投影及分层视差。这支持“同一形状多个深度与相位”的设计借鉴。
- [许可证](https://github.com/chthollyphile/folia-major/blob/b0d2d74e4f4458879f6a53c31bcdb4144d06d754/LICENSE)：AGPL-3.0。v3.6 方案只借鉴模块职责与效果思路，不引入该项目运行时或代码。

**适配方式**：为原生装饰节点建立创建/缓存/销毁、质量档位、统一时间和布局锚点；模板缩略图使用真实原生渲染缩图。影厅、完整歌词播放器、Three.js 环境和音场系统不在本版范围。

## 4. 核查证据与限制

- 原始 GitHub API metadata/tree、固定提交源文件放在 `work/v3.6/`，不加入产品安装包。
- 浏览器搜索工具本次返回空内容；可核查证据来自 GitHub API 返回的仓库数据和固定提交文件。
- 对这些仓库的本地运行、完整视频观看、OBS 效果移植和性能比较均为 **NOT RUN**。
- 用户截图仅验证了大色块、交叉斜带、透视缩放、旋转构图和圆形边框等静态构成，无法证明其运动速度、节奏和循环接缝。
