# OBS Spatial FX v2 原生控制 API

版本：2.0.0，2026-10-30。

原生 Dock 是模型编辑入口。渲染器读取不可变 StageModel 快照，模型随 OBS 场景集合保存于 source.private_settings.spatial_fx_model。v1 public settings 仅用于首次创建及旧数据迁移；不要编辑保存文件中的 private_settings 来进行实时控制。

## 进程内插件集成
对 obs_spatial_fx_stage 来源的 proc_handler 调用：

- `void spatial_fx_get_state(out ptr settings)`：立即返回独立 obs_data_t。调用者负责 obs_data_release；可读取、序列化而不触碰渲染器共享数据。
- `void spatial_fx_set_state(in ptr settings)`：输入为完整状态，调用期间由实现复制。调用者可在调用返回后 release，更新排队到 OBS UI 线程。不要把一个 obs_data_t 同时交给多个写者。

建议先 get，修改独立副本，再 set。set 是完整替换而不是稀疏补丁；队列更新异步，不保证立即 get 可看到新值。来源移除后更新会安全丢弃。

字段与 model.cpp/model.hpp 定义一致：schema_version=2、layers、keyframes、camera_*、light_*、extra_lights。最多32平面、4灯、256关键帧。空间单位为画布像素比例世界单位，角度为度，时间为秒。

## OBS WebSocket 边界
标准 GetInputSettings 返回 OBS 的公共创建/旧版配置，不代表 Dock 当前权威模型。标准 SetInputSettings 不会实时修改 v2 模型，避免 shared obs_data hash、数组、JSON cache 的跨线程争用。本版本没有对外暴露网络远控协议。QA 的 native-command.json 文件桥属于测试模块，不在安装包中。

## 保存
OBS frontend 在正常保存场景集合时保存 private_settings。命名预设另外用 QSaveFile 原子覆盖 presets.json。强制结束进程可能丢失尚未执行的 OBS 保存；正常退出/重启已做真实测试。导入隐藏原 scene item，移除并还原恢复原可见性。
