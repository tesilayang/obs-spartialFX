# v3.6 预设库上游实现研究

检索日期：2026-11-13（Asia/Shanghai）。范围：除 JIZURA、Bilibili 外的官方开源/源码可见项目；目标是为 OBS 原生 C++ 实时运镜、程序遮罩、转场和色块动画建立可追溯候选。本文件只做研究，不构成功能开发方案获批，不修改产品源码。

结论：优先取用 **gl-transitions 的形状遮罩/色块算法**、**Motion Canvas 的相机轨迹语义**，把它们改写为本项目已有参数轨或 OBS effect；FFmpeg xfade 可作 CPU 算法对照。Remotion、GSAP 可核查成品行为与编排方式，但不能把“源码可见/商业使用免费”等同于 MIT 授权。以下共 **40 项已定位到实际实现的候选**，不是 40 项已实现或已测试的插件功能。

## 1. 来源和核查边界

已读项目 `DEVELOPMENT_STANDARDS.md`。下表固定的是本次实际获取的上游 commit；不是建议新增这些运行时依赖。源码快照及 API 树保存在 `work/v36-upstream-research/`，不应复制到发行包。

| 来源 | 本次检查的 commit | 直接证据与许可证 |
|---|---|---|
| gl-transitions/gl-transitions | `22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7` | [规范与 README][gl-readme]、[总许可证][gl-license]；逐文件另查头部，下面 24 项中只有 InvertedPageCurl 为 BSD-3-Clause，其余头部写 MIT |
| FFmpeg/FFmpeg | `88c46fce27e311c73772d9b83db4047dd83b1294` | [vf_xfade.c][ff-src] 文件头为 LGPL-2.1-or-later；[总许可证说明][ff-license] 区分可选 GPL 组件；[官方 xfade 文档][ff-doc] |
| motion-canvas/motion-canvas | `7b91435c301d530351dcf5ebb91dd139c002e405` | [Camera.ts][mc-camera]、[MIT 许可证][mc-license]；相机类包含可运行示例，但本次未运行 |
| remotion-dev/remotion | `6c2cc70896bd449a42dfadbe7f8a14f825a14514` | [根许可证][re-license] 为 Remotion License；本次五个文件没有发现单独的 permissive 许可证 |
| mltframework/mlt | `2a51ed9cca0b7edca5f1c34749bad67b76b0051a` | [COPYING][mlt-license] 为 LGPL 2.1 文本；检查的服务 YAML 声明 LGPLv2.1 |
| greensock/GSAP | `5e6c35ff8eedea12d4f229129def635277d8b0f2` | [README][gsap-readme] 和 [Flip.js][gsap-flip] 头部指向 GreenSock Standard License，不能按 MIT 引入 |

方法：调用 web.run 搜索/打开第一方来源（本次工具未返回可读正文），随后直接请求官方 GitHub API、固定 SHA 的 raw 文件、FFmpeg 官方文档；实际阅读文件头、参数、核心函数和服务描述。GSAP Standard License 网页本次只返回加载页，故不推断其具体再分发授权；以代码头部的特殊许可证事实为边界。

本次未编译、运行或录制任何上游示例，未测 OBS 帧率/画质。下面“低/中/高”仅为**源码静态估计的移植成本及 GPU 工作量**：低＝少量几何/混合运算；中＝非线性 UV、噪声、透视或多对象；高＝多采样或复杂遮挡。它们不是性能测量结果，也不对应工时承诺。

## 2. 24 个可直接研究的 GLSL 成品

所有路径均为 `transitions/<名称>.glsl`，链接固定到本次检查 SHA。它们输入两幅纹理与进度，主要是**二维图像转场**；其中“镜头”“立方体”“门”等视觉命名不意味着上游有本项目式三维相机/图层场景。下表 OBS 方向是推论。

| # | 实现与已核实机制 | 已有可调项/可借鉴内容 | OBS 可移植性及成本判断 |
|---|---|---|---|
| 01 | [DirectionalWipe][g01]：方向点积和 smoothstep 形成直线擦除 | direction、smoothness | 低；可抽出 coverage，给遮罩或纯色块复用；零方向需宿主防护 |
| 02 | [CircleOpen][g02]：中心距离产生圆形开合 | smoothness、opening | 低；适合程序圆形遮罩；要按宽高比决定圆/椭圆 |
| 03 | [WindowSlices][g03]：fract 重复条带随进度变宽 | count、smoothness | 低；条纹幕帘，可保留为一个 shader 而非很多实体 |
| 04 | [WindowBlinds][g04]：奇偶水平条纹交替调节混合进度 | 上游条纹频率随进度变化 | 低；与 03 为不同算法，但应作为“条纹”家族变体，避免重复占库 |
| 05 | [RadialWipe][g05]：绕中心的角度阈值擦除 | smoothness | 低；时钟扇形遮罩，需统一零角和旋转方向 |
| 06 | [RandomSquares][g06]：每格伪随机阈值加软边 | size、smoothness | 低；方格随机揭示；随机种子必须固定以支持倒放和定位 |
| 07 | [PolkaDotsCurtain][g07]：重复圆点随距离/进度长大 | dots、center | 低；圆点幕；可拆成 coverage 后画纯色或遮源 |
| 08 | [PerlinDissolve][g08]：四角随机值插值生成二维噪声阈值 | scale、smoothness、seed | 中；本文件名为 Perlin，实际读到的是四角 value-noise 式插值，不应宣传为完整经典 Perlin 实现 |
| 09 | [ColorPanelWipe][g09]：1–3 块错峰纯色色板扫入、覆盖切换点、扫出 | direction、panels、color1–3、stagger、skew | 低；与用户的程序色块需求直接匹配；三块数量有明确上限 |
| 10 | [Pixelize][g10]：网格 UV 量化、交叉混合、恢复 | squaresMin、steps | 低；像素化转场；双纹理读取，透明边缘需预乘处理 |
| 11 | [Hexagonalize][g11]：六角网格采样再混合 | steps、horizontalHexagons | 中；蜂窝像素化，适合做 10 的形状变体 |
| 12 | [SimpleZoom][g12]：中心 UV 缩放后切换淡入 | zoom_quickness | 低；二维推近切换，可作为“镜头感转场”，不代替真实 Z 推轨 |
| 13 | [RotateScaleFade][g13]：绕中心旋转、缩放、淡入及背景色 | center、rotations、scale、backColor | 低；旋缩出入场，可分别转为图层参数轨或像素 effect |
| 14 | [DreamyZoom][g14]：前后两阶段缩放、旋转并增加亮度 | rotation、scale | 低至中；是二维旋缩闪白；源中 DEG2RAD 系数须重新核算，不能无检查复制 |
| 15 | [Mosaic][g15]：平移/缩放穿过重复纹理格，格内旋转，落到指定格 | endx、endy | 中；“穿越照片墙”视觉；真实空间图层巡游应另外实现 |
| 16 | [Kaleidoscope][g16]：多次镜像/旋转 UV 并混合 | speed、angle、power | 中；万花筒畸变；适合 FX 转场，不是几何遮罩 |
| 17 | [Swirl][g17]：中心局部扭转，附带角度方向模糊 | strength、radius、center；常量 TAPS=12 | 高于普通形状擦除；可提供无模糊变体，不能当成一次普通采样 |
| 18 | [CrossZoom][g18]：向移动中心径向采样并交叉混合 | strength；循环 t=0…40 | 高；41 轮每轮混合两张纹理，源码静态上约 82 次纹理取样/像素，不含编译器优化 |
| 19 | [WhipPan][g19]：拼接图像条带高速平移，沿运动方向模糊 | direction、blur；SAMPLES=24 | 高；约 24 个采样点；图像甩镜，不能据此声称相机真实运动 |
| 20 | [CameraShake][g20]：确定性跳动序列、包络、滚转、动态裁切、色散 | strength、jolts、blur、split；TAPS=10 | 高；每 tap 三次 shot，静态约 30 次取样；若只需真实相机震动，可单独提取轨迹思路 |
| 21 | [Cube][g21]：射线与旋转盒侧面相交，带地面反射、阴影 | persp、unzoom、reflection、floating、shading | 中至高；可先移植 shader 视觉；若改三维图层需定义遮挡和反射，不是简单 rotateY |
| 22 | [Doorway][g22]：两扇门透视打开，后景推进，地面反射 | reflection、perspective、depth | 中至高；可选双平面几何方案；反射与门厚度会扩大实现范围 |
| 23 | [GridFlip][g23]：网格牌片错峰翻面，邻格遮挡、阴影 | size、pause、dividerWidth、bgcolor、randomness | 高；源码每像素检查 3×3 邻格，不应按旧版简化网格翻面估算 |
| 24 | [InvertedPageCurl][g24]：卷页曲面与阴影 | 卷曲/背面/阴影公式 | 高；**BSD-3-Clause**，需保留该文件完整声明，不能只保留总 MIT |

这些候选里，01–09 能抽出 `coverage(uv, progress)` 形成遮罩，也能乘颜色画装饰；10–24 大多数需要采样原图/两路纹理。不能把所有转场统一标成“程序形状”：扭曲、模糊、三维伪透视属于另一类渲染能力。[依据：各 shader 与 GL 规范][gl-readme]

## 3. 8 个 FFmpeg xfade 算法对照

统一源文件为 [libavfilter/vf_xfade.c][ff-src]，选项表与同名宏函数都已查到；许可证均随该文件 LGPL-2.1-or-later。以下选取有不同算法含义的项目，未把四个方向展开为四项。官方文档要求两路输入为恒定帧率且分辨率、像素格式、帧率、timebase 一致。[文档][ff-doc]

| # | 实现位置（固定 SHA 行） | 已检查到的行为 | OBS 方向与成本判断 |
|---|---|---|---|
| 25 | [rectcrop / RECTCROP_TRANSITION，L622][f25] | 中心矩形缩合/张开，中间外围填背景色 | 低；可用矩形 coverage。背景填黑不应无条件迁入透明图层流程 |
| 26 | [distance / DISTANCE_TRANSITION，L657][f26] | 根据两路像素各平面颜色差的欧氏距离决定切换，再混合 | 低至中；内容感知阈值；不能误写成“空间距离擦除” |
| 27 | [smoothleft / SMOOTHLEFT_TRANSITION，L788][f27] | 横向变化的 smoothstep 混合权重 | 低；用于柔边擦除对照，进度方向必须核验 |
| 28 | [hlslice / HLSLICE_TRANSITION，L1255][f28] | smoothstep 进度与 fract(10*x/width) 比较，条带揭示 | 低；与 GL WindowSlices 同家族，可合并为参数变体 |
| 29 | [hblur / HBLUR_TRANSITION，L1377][f29] | 横向均值模糊增强再减弱，并混合两图；CPU 用滑动累加更新窗口 | GPU 移植中至高；不是逐像素按半幅宽度重采样，也不能把 CPU 的线性扫描更新直接放入独立片元 |
| 30 | [fadegrays / FADEGRAYS_TRANSITION，L1422][f30] | 彩色过渡到灰度再恢复；RGB 与非 RGB 路径分开处理 | 低；OBS RGBA 重写一条路径即可，需定义灰度系数和线性/显示空间 |
| 31 | [squeezeh / SQUEEZEH_TRANSITION，L1624][f31] | 用纵向坐标除以 progress，将图像压成横条，外面露出另一图 | 低；名称 h 指结果横条，源码实际改变 y；避免方向实现反了 |
| 32 | [zoomin / ZOOMIN_TRANSITION，L1709][f32] | zoom 坐标重映射和分阶段 smoothstep 混合 | 低；二维缩放基线；CPU ceil 采样改为 GPU 双线性会改变外观，应记录差异 |

选择 FFmpeg 的价值是“每个名字都有明确实现”，不是要向 OBS 渲染线程注入离线 FFmpeg 流水线。其逐平面 CPU AVFrame 模型和本项目 gs_texture_t/预乘 Alpha 并不等价。此处是否引用代码、独立重写算法、还是只做视觉对照，应在正式方案中分别记录授权和改写范围。[文件结构及总许可说明][ff-src][ff-license]

## 4. 3 个 Motion Canvas 相机脚本来源

三项均来自 [packages/2d/src/lib/components/Camera.ts][mc-camera]，MIT。[许可证][mc-license] 类明确为正交相机，不能宣传其已有透视相机、真实三维绕拍或景深。OBS 实现判断如下。

| # | 实际接口/示例 | 已检查到的计算 | 可落地候选与成本判断 |
|---|---|---|---|
| 33 | `centerOn(node / position, duration)`；类内示例同时执行 rotation、zoom | 节点绝对位置转 scene.worldToLocal，再补间位置；zoom 内部使用 scale 倒数 | 目标 A→B 聚焦、聚焦+滚转+缩放；CPU 低成本，映射到项目轨道时须保留自己的相机投影规则 |
| 34 | `followCurve` / `followCurveReverse` | 按归一化时间取曲线点，转换到世界位置；reverse 使用 1-timing(value) | 曲线巡游/往返巡游；CPU 低至中，曲线及弧长参数化需显式规定，不能把时间比例当恒定物理速度 |
| 35 | `followCurveWithRotation` / `followCurveWithRotationReverse` | 同时用曲线 point 与 normal，计算切线朝向的角度 | 路径随行滚转；CPU 低至中；上游是 2D 朝向角，OBS 的 3D look-at、俯仰、避奇异点要另设计 |

“先聚焦 A、停留、移动 B、回到总览”可由 33 的 centerOn、zoom、rotation、reset 编排成剧本；这是基于已有接口的**建议组合**，不是上游已命名的某一成品预设。不要把同一方法的正反向扩充为大量独立算法。

## 5. 5 个 Remotion 成品行为参考（受特殊许可限制）

位置统一在 `packages/transitions/src/presentations/`。根 [Remotion License][re-license] 允许符合条件的实体为生成视频/图像使用和修改，但明确限制为销售、出租、许可/再许可自有 Remotion 衍生品而复制或修改代码。研究时不能把下列源码直接放入可销售/许可的 OBS 插件；本报告仅记录已实现的视觉行为，建议如需同类效果，采用已核实 MIT/BSD 来源或独立实现并记录来源关系。

| # | 文件 | 已核实成品实现 | 对 OBS 的参考价值（推论） |
|---|---|---|---|
| 36 | [slide.tsx][r36] | 进入/退出元素分别计算百分比 translateX/Y，四方向；有接缝 epsilon 修正 | 低；双路位置编排、边缘缝隙处理，可用本项目参数轨表达 |
| 37 | [flip.tsx][r37] | 根据进入/退出方向选 rotateX/Y；容器 perspective；切换可见面 | 中；卡片单次翻面；需真实处理背面和中点，不能只动画角度 |
| 38 | [wipe.tsx][r38] | 八方向 polygon 裁切，分别计算进入/退出多边形 | 低；直线/对角几何遮罩，优先改用 GL DirectionalWipe 等宽松许可来源 |
| 39 | [clock-wipe.tsx][r39] | makePie 创建足够覆盖画面的扇形路径，再平移用于 CSS clipPath | 低；参考扇形最大半径与中心定位；原生实现应直接用解析覆盖率 |
| 40 | [iris.tsx][r40] | makeCircle 动态半径路径，平移到指定中心后裁切 | 低；圆形开幕；已有 GL CircleOpen 算法可用，不能重复统计成新底层算法 |

这里保留成品名字是为了可查找、可对照，不把 React/CSS/DOM 引入 OBS。上述成本仅指独立实现同类行为的结构复杂度，不包含取得额外授权的工作。

## 6. MLT、GSAP 的补充结论

### MLT：借鉴参数契约，不依赖整个剪辑引擎

已读三个实际服务描述：

- [`src/modules/core/transition_luma.yml`][mlt-luma]：`identifier: luma`，title Wipe；用灰度图阈值确定 A/B 露出，包含 softness、reverse、invert。可借鉴“单通道进度场＋阈值”的通用遮罩接口；如要纯程序绘制，可生成同等数学进度场，运行时不必读 PGM。
- [`src/modules/plus/transition_affine.yml`][mlt-affine]：`identifier: affine`，title Transform；已有关键帧旋转、位移、剪切、repeat_off、mirror_off、cycle 等语义。值得借鉴往返/重复时序和变换参数分类，不能据此声称 MLT 与本项目投影相同。
- [`src/modules/core/transition_composite.yml`][mlt-composite] 明写 `Composite (*DEPRECATED*)`，并建议 affine 或 qtblend。**不把 composite 选为新实现依赖**。

三者元数据均声明 LGPLv2.1；本次同时阅读根 COPYING 与 luma C 实现。MLT 来源提供了可用的参数模型，但未运行 Shotcut/Kdenlive，也未逐一核实它们当前 UI 如何映射这些服务，因此不把这三项标成“已在某剪辑器亲测”。

### GSAP：编排范式有价值，许可证与环境必须分开

已检查固定 SHA 的 [README][gsap-readme]、[`src/gsap-core.js`][gsap-core]、[`src/Flip.js`][gsap-flip]。README 宣称包含插件的免费商业使用；代码头部仍写 GreenSock 版权所有、受 Standard License 约束。该“免费”不能推出 MIT、BSD 或无条件可再分发。

可研究的已有机制是 Timeline 的位置参数/错峰编排、核心属性补间、Flip 状态前后布局匹配。它们是生成本项目参数轨的参考，不能把浏览器布局测量、DOM 重排及 JS runtime 当作 OBS C++ 方案。MotionPathPlugin、MorphSVGPlugin 本次未逐文件读取，不列为已核实候选。GSAP 官方许可页本次未取得正文，任何复制/派生用途留待正式方案复核。

## 7. 建议如何收敛，避免“数量多但效果重复”

下面是研究判断，不是已确认实现范围。

1. 第一组优先：DirectionalWipe、CircleOpen、WindowSlices、RadialWipe、RandomSquares、PolkaDotsCurtain、ColorPanelWipe，加上 centerOn 编排。它们最接近现有程序几何/参数轨，先建立遮罩与纯色装饰的共同 coverage 和时序定义。
2. 第二组：Pixelize、Hexagonalize、RotateScaleFade、Mosaic、曲线路径、切线朝向。它们扩充纹理重采样或轨迹能力；同家族方向/开合/正反向作为变体，不虚增数量。
3. 第三组：CrossZoom、WhipPan、CameraShake、GridFlip、Cube、Doorway、PageCurl。多采样、局部遮挡、反射的实现与性能成本明显更大，正式方案应分别评估，并提供采样档位或简化版；本次没有 FPS 证据。
4. Remotion 用于视觉行为核对；FFmpeg 用于公式对照；GSAP 用于时序编排研究。不要为了借鉴一个效果把整套浏览器/离线视频框架放进原生 Dock。

对接本项目的必须项：所有相机/空间运动经 `evaluateSceneAt()`；现有格式和基础布局分离；绑定按实例 ID；GL UV 左下原点、straight RGBA 与项目坐标和预乘 Alpha 显式适配；0/1 端点、透明源、非 16:9、尺寸变化、倒放与随机定位、失效绑定均需专项验证；纹理/effect 缓存、合法图形上下文和关闭即停止处理遵守开发标准。上游 GL 规范对输入输出透明度和端点有明确契约，但本项目仍须在 OBS 真输出验证，不能继承上游 CI 结果。[GL 规范][gl-readme]

## 8. 本次验证状态

| 项目 | 状态 | 证据/范围 |
|---|---|---|
| 官方源码获取与核心实现阅读 | PASS | `work/v36-upstream-research/revisions.json`、各 repo tree JSON 和选定源文件；PASS 仅表示研究证据已获取 |
| 候选计数/固定路径检查 | PASS | 24 GLSL + 8 xfade + 3 Camera 方法组 + 5 Remotion = 40；固定 SHA 路径来自 GitHub API 树与 raw 获取 |
| 上游示例执行/画质核对 | NOT RUN | 未启动上游运行时 |
| OBS 移植、编译、GPU 对照、性能 | NOT RUN | 本任务为研究，不存在实现载荷 |
| GSAP Standard License 完整条款页 | BLOCKED | 官方页返回加载 HTML；未据此作授权结论 |

## 来源索引

[gl-readme]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/README.md
[gl-license]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/LICENSE
[ff-src]: https://github.com/FFmpeg/FFmpeg/blob/88c46fce27e311c73772d9b83db4047dd83b1294/libavfilter/vf_xfade.c
[ff-license]: https://github.com/FFmpeg/FFmpeg/blob/88c46fce27e311c73772d9b83db4047dd83b1294/LICENSE.md
[ff-doc]: https://ffmpeg.org/ffmpeg-filters.html#xfade
[mc-camera]: https://github.com/motion-canvas/motion-canvas/blob/7b91435c301d530351dcf5ebb91dd139c002e405/packages/2d/src/lib/components/Camera.ts
[mc-license]: https://github.com/motion-canvas/motion-canvas/blob/7b91435c301d530351dcf5ebb91dd139c002e405/LICENSE
[re-license]: https://github.com/remotion-dev/remotion/blob/6c2cc70896bd449a42dfadbe7f8a14f825a14514/LICENSE.md
[mlt-license]: https://github.com/mltframework/mlt/blob/2a51ed9cca0b7edca5f1c34749bad67b76b0051a/COPYING
[mlt-luma]: https://github.com/mltframework/mlt/blob/2a51ed9cca0b7edca5f1c34749bad67b76b0051a/src/modules/core/transition_luma.yml
[mlt-affine]: https://github.com/mltframework/mlt/blob/2a51ed9cca0b7edca5f1c34749bad67b76b0051a/src/modules/plus/transition_affine.yml
[mlt-composite]: https://github.com/mltframework/mlt/blob/2a51ed9cca0b7edca5f1c34749bad67b76b0051a/src/modules/core/transition_composite.yml
[gsap-readme]: https://github.com/greensock/GSAP/blob/5e6c35ff8eedea12d4f229129def635277d8b0f2/README.md
[gsap-flip]: https://github.com/greensock/GSAP/blob/5e6c35ff8eedea12d4f229129def635277d8b0f2/src/Flip.js
[gsap-core]: https://github.com/greensock/GSAP/blob/5e6c35ff8eedea12d4f229129def635277d8b0f2/src/gsap-core.js
[g01]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/DirectionalWipe.glsl
[g02]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/CircleOpen.glsl
[g03]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/WindowSlices.glsl
[g04]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/WindowBlinds.glsl
[g05]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/RadialWipe.glsl
[g06]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/RandomSquares.glsl
[g07]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/PolkaDotsCurtain.glsl
[g08]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/PerlinDissolve.glsl
[g09]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/ColorPanelWipe.glsl
[g10]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/Pixelize.glsl
[g11]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/Hexagonalize.glsl
[g12]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/SimpleZoom.glsl
[g13]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/RotateScaleFade.glsl
[g14]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/DreamyZoom.glsl
[g15]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/Mosaic.glsl
[g16]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/Kaleidoscope.glsl
[g17]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/Swirl.glsl
[g18]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/CrossZoom.glsl
[g19]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/WhipPan.glsl
[g20]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/CameraShake.glsl
[g21]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/Cube.glsl
[g22]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/Doorway.glsl
[g23]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/GridFlip.glsl
[g24]: https://github.com/gl-transitions/gl-transitions/blob/22f67b2bf6ee531253b71ae6c7bf29c2f31c21a7/transitions/InvertedPageCurl.glsl
[f25]: https://github.com/FFmpeg/FFmpeg/blob/88c46fce27e311c73772d9b83db4047dd83b1294/libavfilter/vf_xfade.c#L622
[f26]: https://github.com/FFmpeg/FFmpeg/blob/88c46fce27e311c73772d9b83db4047dd83b1294/libavfilter/vf_xfade.c#L657
[f27]: https://github.com/FFmpeg/FFmpeg/blob/88c46fce27e311c73772d9b83db4047dd83b1294/libavfilter/vf_xfade.c#L788
[f28]: https://github.com/FFmpeg/FFmpeg/blob/88c46fce27e311c73772d9b83db4047dd83b1294/libavfilter/vf_xfade.c#L1255
[f29]: https://github.com/FFmpeg/FFmpeg/blob/88c46fce27e311c73772d9b83db4047dd83b1294/libavfilter/vf_xfade.c#L1377
[f30]: https://github.com/FFmpeg/FFmpeg/blob/88c46fce27e311c73772d9b83db4047dd83b1294/libavfilter/vf_xfade.c#L1422
[f31]: https://github.com/FFmpeg/FFmpeg/blob/88c46fce27e311c73772d9b83db4047dd83b1294/libavfilter/vf_xfade.c#L1624
[f32]: https://github.com/FFmpeg/FFmpeg/blob/88c46fce27e311c73772d9b83db4047dd83b1294/libavfilter/vf_xfade.c#L1709
[r36]: https://github.com/remotion-dev/remotion/blob/6c2cc70896bd449a42dfadbe7f8a14f825a14514/packages/transitions/src/presentations/slide.tsx
[r37]: https://github.com/remotion-dev/remotion/blob/6c2cc70896bd449a42dfadbe7f8a14f825a14514/packages/transitions/src/presentations/flip.tsx
[r38]: https://github.com/remotion-dev/remotion/blob/6c2cc70896bd449a42dfadbe7f8a14f825a14514/packages/transitions/src/presentations/wipe.tsx
[r39]: https://github.com/remotion-dev/remotion/blob/6c2cc70896bd449a42dfadbe7f8a14f825a14514/packages/transitions/src/presentations/clock-wipe.tsx
[r40]: https://github.com/remotion-dev/remotion/blob/6c2cc70896bd449a42dfadbe7f8a14f825a14514/packages/transitions/src/presentations/iris.tsx
