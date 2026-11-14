# v3.6.0 组合预设实施计划

> 执行：按已确认的 V36_RESEARCH_AND_PROPOSAL.md 分项实现与验证。

**目标：** 原生预设卡片绑定现有实例，产生可编辑运动、确定性循环及参与空间合成的程序装饰与遮罩。

**架构：** 数据与纯数学编译器、原生图形渲染器、Qt 预设面板各自独立。统一场景求值、现有撤销和工程归档负责集成。

**技术：** C++17、Qt6、MSVC、libobs；验收 OBS 33.0.1；不新增运行时依赖。

## 固定约束

- 遵守 DEVELOPMENT_STANDARDS.md；不更新说明书，不自动发布 GitHub。
- 角色按 instanceId 关联；程序图形独立存储，不占 32 个来源槽。
- 模板按绝对微秒时间求值；渲染资源缓存，图形线程不等待 UI。
- 8 张模板卡片，前三张优先验证；摄影机运动默认关闭。
- 所有替换、预览和应用均可恢复；锁定和动画冲突阻止静默覆盖。

## 1. 数据、模板编译与时间求值

文件：新建 src/presets/types.hpp、presets.hpp/.cpp、tests/preset_tests.cpp；修改 model.*、scene_evaluator.*。

接口：StageModel 保存 geometries、motionBlocks、presetInstances；applyPreset(StageModel&, const ApplyRequest&, std::string&) 原子编译；evaluateSceneAt 输出 evaluated geometries。

- [x] 先编写模板缺失、角色错误、锁定、同源独立绑定、循环闭合与快照往返测试并记录失败。
- [x] 实现 8 种模板和变体，有限动作写普通关键帧；循环块维持有界存储。
- [x] 实现输入校验、冲突范围替换及循环转关键帧，验证区间外原曲线。
- [x] 按手动轨→模板偏移→约束→导入补偿→绑定顺序接入统一求值。
- [x] 验证 v3.5 无新增字段加载、项目 UUID 重映射、contentEnd 和长时间溢出。

## 2. 原生图形和遮罩

文件：新建 src/presets/graphics.hpp/.cpp；修改 stage.cpp；新增 OBS 输出探针。

接口：EvaluatedGeometry 传入已求值位姿、形状、颜色、覆盖率与接收实例；单个缓存模块处理几何绘制和图层遮罩。

- [x] 先制定真实输出的矩形、斜带、圆窗、层间遮挡与透明度断言。
- [x] 独立 shader、复用 VB/纹理；图形参与现有深度排序。
- [x] 可见装饰、指定目标遮罩、组合模式；世界空间与局部 UV 分开。
- [x] 验证暂停定位、循环接缝、分辨率变动、资源释放与关闭零新增 pass。

## 3. 预设面板

文件：新建 src/presets/panel.hpp/.cpp；修改 dock.*。

接口：面板使用 StageModel*；通过事务回调提交完整快照；临时预览单独发布，取消恢复；旧整套预设放折叠区。

- [x] 先列出 QA 可操作的控件名和预期按钮状态。
- [x] 卡片、角色、动感、周期、色板、变体、遮罩、锚点、深度和节拍设置。
- [x] 实时预览、取消、应用、移除、有限区间烘焙；每次应用一次撤销。
- [x] 缺失绑定、锁定、冲突明确提示；一秒悬停说明。

## 4. 时间轴及集成

文件：editor.*、editor_timeline.cpp、CMakeLists.txt、安装器版本资源。

- [x] 图形独立行可选中、隐藏、编辑空间参数、删除确认；循环块显示跨度/周期。
- [x] 增加外部完整文档事务和预览取消接口，禁止绕过撤销。
- [x] 更新 3.6.0 所有产品版本，旧工程回归；逐阶段真实 build/test。

## 5. 验收和交付

文件：tests/v36*、tools/package-v36.py、docs/v3.6/V36_REPORT.md。

命令：`$env:OBS_ROOT=Join-Path $PWD 'runtime/obs33'; tools/build.cmd 4`；`tools/test.cmd`；隔离 OBS `tools/run_v33_installed_qa.ps1`。

- [x] 保存基线、隔离配置备份与实际测试日志。
- [x] OBS 33.0.1 最终 DLL：八模板、运动编辑、遮罩、循环、撤销、保存重启和工程往返。
- [x] 1080p60 对比关闭/装饰/单遮罩/组合，记录 CPU、内存、帧耗时和丢帧。
- [x] 安装升级、运行拦截、回滚、卸载重装及配置保留。
- [x] 安装包、源码、符号、证据和 SHA256；恢复隔离环境；每项测试仅按实际证据报告。

自审：覆盖方案第 6～16 节；第三方内容仅研究，模板和实现独立编写；无新增素材授权依赖。

最终状态：实施与验收见 V36_REPORT.md；实际交付和哈希以 dist/v3.6/DELIVERY_MANIFEST.json 为准。
