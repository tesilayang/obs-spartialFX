# OBS Spatial FX v3.6.4 开发记录

日期：2026-11-15。依据已确认的透明边缘与统一时间轴计划实施。本版不更新使用说明书，不执行 Git 提交、推送或 Release 发布。

## 功能与修复

1. **透明来源白边**：来源缓存绘制前恢复 OBS 默认混合状态，来源插件可以按自身约定输出直通或预乘 Alpha；最终舞台保持预乘合成。修正 Spout ALPHA 模式在中间缓存被错误当成预乘数据的问题，不收缩透明边缘、不硬切 Alpha。
2. **预设普通关键帧**：新预设的有限重复运动写入普通参数轨。记录、移动、删除、曲线、复位和撤销使用原生时间轴操作；删除运动后不再被隐藏块延长播放终点。超过 250000 个生成关键帧时整笔拒绝，不截断数据。
3. **旧运动迁移**：载入旧循环块时合成原手动曲线与块偏移，使用采样边界及自适应插值保留相位、循环和端点。成功后移除运行块、保留原始 JSON 备份；失败保留原数据和原因。默认探测误差为绝对 1e-5 加相对 1e-8，属于采样点误差控制。迁移核心支持取消回调；自动加载入口没有独立的取消按钮。
4. **时间轴右键**：复制/粘贴时间轴信息、删除对象、复制/粘贴图层。粘贴以最早关键帧对齐播放头，过滤不兼容参数，覆盖冲突区间前确认；复制图层生成新实例 ID，共享 OBS 来源，不重复打开捕获连接。
5. **灯光动画**：点光源和面光源进入时间轴。位置、强度、范围、补光、阴影、尺寸、衰减、轮廓光及 RGB 颜色支持关键帧。灯光页与时间轴共享求值、自动关键帧和撤销。绑定时先求灯光局部动画，再跟随所属图层最终位姿。
6. **FX 动画**：启用的子效果显示在素材下方。全部目录数值参数、混合强度和支持的颜色进入普通动画轨；模式保持插值，计数取整数。关闭效果保留动画并停止该效果处理。文件、LUT、辅助素材引用保持静态。
7. **FX 时间**：像素、几何、舞台效果共用时间轴时钟。普通非预览状态使用实际时间；定位、倒退、循环和播放域变化清除旧历史有效标记，复用纹理。暂停时不累计历史，定位回同一时刻恢复相同程序化画面。
8. **审查补修**：已删除 FX 的旧页面刷新安全返回；效果结构变化重建面板。灯光删除和 FX 卡片删除遵守锁定，FX 卡片删除加入完整撤销。

## 实际修改文件

| 模块 | 文件 |
|---|---|
| 对象与动画访问 | `src/timeline_object.hpp/.cpp`、`src/animation_io.hpp`、`src/model.hpp/.cpp`、`src/fx_catalog.hpp`、`src/fx_model.cpp` |
| 时间轴与操作 | `src/editor.hpp/.cpp`、`src/editor_objects.cpp`、`src/editor_timeline.cpp`、`src/editor_curves.cpp`、`src/editor_curve_widget.cpp`、`src/editor_help.cpp` |
| 预设迁移与生成 | `src/motion_migration.hpp/.cpp`、`src/presets/composer.hpp/.cpp`、`src/presets/presets.cpp`、`src/presets/panel.cpp` |
| 页面联动 | `src/dock.hpp/.cpp`、`src/dock_v2.cpp`、`src/dock_v3.cpp`、`src/fx_panel.hpp/.cpp` |
| 求值与渲染 | `src/scene_evaluator.hpp/.cpp`、`src/stage.cpp`、`src/fx_renderer.hpp/.cpp`、`src/fx_stage.hpp` |
| 构建及版本 | `CMakeLists.txt`、`src/plugin.cpp`、`installer/setup.cpp`、`installer/setup.rc.in` |
| 测试 | 新增对象、编辑器、时钟 C++ 测试及 `tests/v364_*.py`、`tests/v364_gpu_sample.ps1`、`tests/v364_alpha_source.inc`；更新模型、预设、组合器测试和隔离 QA 桥 |
| 开发记录 | 本目录、已确认计划、`docs/architecture/SPATIAL_COORDINATE_CONTRACT.md` |

具体文件与摘要以交付源码包的文件清单及 SHA256 为准。工作区原有 v3.6.3 说明书重命名和展示页保持原样。

## 数据与线程

- 新写出 schema 6。灯光持久 UUID，FX 身份为所属实例 ID＋目录效果 ID；参数使用字符串键。UI 临时句柄不保存为对象身份。
- `evaluateSceneAt()` 输出完整灯光和逐层 FX 状态；图形线程只消费不可变快照，不回写基础布局。
- 新增灯光主对象、额外灯光与 FX 均保存动画和锁定状态。旧主灯采用固定迁移 ID，随后保存；新建对象分配 UUID。
- 沿用 C++17、Qt6、MSVC `/utf-8` 和已有依赖。本版未增加第三方运行库、未更换渲染算法依赖。

编译命令、最终安装载荷哈希、真实 OBS 输出和专项状态见 `TEST_REPORT.md` 与证据包。
