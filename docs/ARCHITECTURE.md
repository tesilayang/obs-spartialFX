> V1 历史记录（保留原始结果）。当前 v2 文档与验收见 docs/v2/。

# Architecture / OBS Spatial FX 1.0

## 宿主集成
C++17 OBS 模块，Qt6 QWidget 内容嵌入 OBS 自己创建的 QDockWidget。只在 OBS_FRONTEND_EVENT_FINISHED_LOADING 后使用 obs_frontend_add_dock_by_id 注册；退出事件移除。五个原生标签：空间图层、摄像机、灯光、FX 特效、预设。产品不启动外部独立面板进程；安装器的便携启动模式只启动 OBS 本身。

## 数据模型与关联
model.hpp/cpp 定义 StageModel / Layer / Camera / Light / Fx / Keyframe。输入通过 source UUID、父场景 UUID、scene item ID 关联，场景项通过 obs_scene_enum_items 枚举。名称仅用于显示。导入时隐藏原项，显式还原按钮按三重身份校验恢复原可见状态，避免误改同名或重建场景项。删除输入会安全跳过，并在面板标记。

支持当前场景顶层普通视频输入。组、嵌套场景、裁剪、边界、镜像暂不展开。摄像机/布局/关键帧/灯光/FX 存于 OBS 源 settings，随场景集合保存；命名预设通过 QSaveFile 写到模块配置的 presets.json。预设应用保留图层身份，布局按现有图层顺序映射。

## 渲染与线程
stage.cpp 注册 custom draw 输入源 obs_spatial_fx_stage。UI/OBS update 发布不可变快照；读取计数保护旧快照，后续更新在静默点回收。video_tick 推进时间，video_render 读取快照并提交图形。渲染没有等待 UI 的自有互斥锁。输入使用 weak source ref；活跃子源去重关联，拒绝舞台/场景/组递归。

core.cpp 计算 XYZ 旋转、透视、近裁剪安全检查、关键帧插值（最短角路径、缓动和循环）与点光投影。GPU 着色器提供透视校正 UV、预乘 Alpha、单点漫反射、衰减、Alpha 投影、EV/饱和度/暗角。按图层中心深度排序；不实现相交网格深度缓冲或完整遮挡。

每个层索引缓存 gs_texrender，最大32份；捕获最长边2048。同一源更新尺寸时复用已有表面，不逐帧重新分配。共享动态四顶点缓冲、一个 effect、白纹理在首次 video_render 创建。gs_* 创建/绘制仅在图形上下文，销毁显式 obs_enter_graphics/leave_graphics。首个接收平面绘制前初始化所有 shader uniform，发布日志不再出现未设置参数警告。渲染线程无 UI 等待，UI 仅读取性能原子计数。

## 安装
Win32 GUI 安装器，静态 MSVC CRT，内嵌真实插件 DLL、effect、README、GPL 许可证。系统目录固定 ProgramData/obs-studio/plugins/obs-spatial-fx。不写 Program Files，不包含 Qt DLL，不在 OBS 运行时安装/更新/卸载。

通过卸载注册表、运行进程、常见目录及快捷方式识别 OBS32+；旧版列为不支持。极深的便携目录可从运行进程识别，交互安装要求先关闭再重试，路径跨提权保留。无正在运行实例且无常见路径/快捷方式的任意磁盘位置无法保证自动发现，可通过 /OBSROOT 明确指定。

OBS 便携模式主动跳过 ProgramData 扫描（官方 OBS32 frontend/widgets/OBSBasic.cpp 的 AddExtraModulePaths）。因此生成开始菜单专用便携快捷方式，仅对子进程设置 OBS_PLUGINS_PATH / OBS_PLUGINS_DATA_PATH。不会更改系统环境变量或 OBS 文件。

安装先在同级 staging 解出资源，备份已拥有文件，通过 MoveFileEx 提交；失败回滚。卸载器复制自身到临时位置后删除已拥有文件，保留 OBS 配置及未知文件。fs::equivalent 处理路径分隔符，不依赖字符串相等。Windows 卸载项与快捷方式随安装/卸载管理。

## 文件职责
- src/plugin.cpp：模块/宿主生命周期。
- src/dock.cpp、dock_actions.cpp、dock.hpp：原生控件、导入/还原、预设、演示。
- src/core.*：纯数学、动画。
- src/model.*：有边界校验的 OBS 配置序列化。
- src/stage.*、data/effects/spatial.effect：图形缓存、源关联、渲染与统计。
- installer/setup.cpp、setup.rc.in：自包含安装器、资源、便携启动与卸载。
- tests/obs_qa_driver.cpp：独立 QA 模块，永不打包。

官方 API 的对应源码基线为 third_party/obs-studio-32.0.0；依赖提交和 SHA256 见 tools/dependencies.json。测试证据和未覆盖平台见 TEST_REPORT.md。
