> 最终集成验收已完成，见 docs/v3.6/V36_REPORT.md（560 项原生 UI、14 项重启、24 项几何、58 项模板与性能）；下文保留面板阶段的历史执行记录。

# v3.6 预设面板 QA 契约

状态：原生 Qt 行为测试 PASS；OBS 真实验收 NOT RUN。下方契约不表示 OBS 图像、持久化与撤销验收已通过。

- `spatial.presets36` 是原生 QWidget；8 个卡片 `spatial.preset-card-{blocks,cross,iris,float,cards,pop,shutter,minimal}` 带原生程序缩略动画。缩略动画只在面板可见时更新。
- `spatial.preset-subject36`、`spatial.preset-second36`、`spatial.preset-anchor36` 的 itemData 为当前舞台 instanceId；背景、前景、遮罩目标列表分别为 `spatial.preset-background36`、`spatial.preset-foreground36`、`spatial.preset-mask-targets36`，各项 UserRole 是 instanceId。重排/改名后绑定保持；删除绑定后清除并提示，不能选同源替代。
- 主体必选；快门 A/B 必须不同；分层漂浮要求背景。缺少角色、锁定、区间冲突时在 `spatial.preset-status36` 显示编译器错误。`spatial.preset-replace36` 默认关闭，只有用户明确启用后允许区间替换。
- `spatial.preset-preview36` 发布临时文档，不写盘、不修改编辑文档。活动预览时更改选项实时重编译。`spatial.preset-cancel36` 初始禁用，取消/换舞台/外部切换调用取消回调一次。`spatial.preset-apply36` 对复制文档编译成功后仅调用一次完整文档事务回调。
- `spatial.preset-motion36` 默认开，`spatial.preset-camera36` 默认关。`spatial.preset-decoration36`、`spatial.preset-mode36`、`spatial.preset-local-mask36`、`spatial.preset-screen-fixed36` 控制图形/遮罩。
- `spatial.preset-{x,y,z,rx,ry,rz,scale-x,scale-y}36` 使用 NumericSlider，连续步长 0.01。XYZ 滑条范围 -2000~2000，旋转 -360~360；数字框允许更大位置。颜色 `spatial.preset-color-{a,b}36` 对话框实时更新临时预览，取消恢复进入对话框之前配色与预览状态。
- `spatial.preset-depth36` 提供最前方、在 A 与 B 之间、跟随某层、自定义 XYZ；层间参考 `spatial.preset-depth-{a,b}36` 使用实例 ID。最前方依据统一场景求值的相机前向深度、层间依据开始时刻求值的世界中心设置初始世界位置，均加 XYZ 偏移；跟随模式保存锚点局部偏移。不会随运镜自动改成屏幕贴片。
- `spatial.preset-period36` 默认秒制；`spatial.preset-beat36` 开启后按 `spatial.preset-bpm36` 与 `spatial.preset-beat-ratio36` 换算周期，提供 1/4、1/2、1、2、4、8 拍。
- `spatial.preset-reuse-bpm36` 仅主动点击时读取镜头页 `spatial.bpm32` 与 `spatial.beat-offset32`；不存在时提示并保留手动 BPM。时间数字框上限沿用 int64 微秒范围。
- `spatial.preset-applied36` 按已保存实例 ID 显示；移除与烘焙均先确认。`spatial.preset-bake-{start,end,step}36` 明确有限区间和步长，`spatial.preset-bake-count36` 显示预计采样时刻数，超过 10001 禁止执行；成功仅一次事务回调。
- `spatial.preset-legacy-toggle36` 折叠“我的整套预设”，`spatial.preset-legacy36` 接收原有整套预设控件。
- 所有常规帮助沿用 HoverHelp 一秒气泡。状态/错误/冲突保持直接显示。刷新不覆盖用户正在编辑的选项，不反复重建相同实例列表。

待执行：真实构建；OBS 33.0.1 中卡片、角色与缺失绑定、预览取消、颜色取消、应用/撤销、移除/撤销、烘焙确认/撤销、同源多实例、切舞台与工程重载。每项实际结果和证据另记发布测试报告。

## 已执行的局部验证（2026-11-13）

- PASS：MSVC C++17 真实编译并链接 `tests/preset_panel_tests.cpp`、面板、公共 widgets、模型、统一求值、预设与 FX 模型依赖，运行 `work/v3.6/panel-qa/panel_ui_smoke.exe -platform minimal`，退出码 0。临时预览不改原模型、预览重编译不累加、应用仅一次回调、取消不重复回调、改名重排保持 ID、换文档取消、缺 B 阻止应用、极简默认关闭图形、配色实时变更取消恢复、层间求值、已有 BPM 复用、完整时间范围均已执行。
- PASS：公共 NumericSlider 的 ±9e12 数字保留及固定滑条端点回归；修复前 FAIL 证据 `work/v3.6/panel-qa/slider-red.log`，修复后 PASS 证据 `work/v3.6/panel-qa/slider-green.log`。
- NOT RUN：此局部测试未验证真实 OBS 输出、安装载荷、性能、工程重启、完整撤销栈和 GPU 合成。
