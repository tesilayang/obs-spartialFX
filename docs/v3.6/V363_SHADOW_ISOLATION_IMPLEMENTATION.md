# v3.6.3 阴影与舞台轮廓光隔离修复

## 确认范围

- 舞台轮廓光只识别局部光效合成前的素材轮廓，排除 FX 新增阴影。
- 移除 FX 投影／内阴影的辅助强度与辅助颜色。
- 保持版本 3.6.3，不更新说明书，不发布 GitHub。

## 根因与实现

原受光路径从最终纹理的 Alpha 提取轮廓；局部外投影新增的 Alpha 也被识别成可受轮廓光照亮的边缘。辅助参数则另外向 RGB 叠加辅助色，两种亮边来源独立。

`src/stage.cpp` 复用已存在且已应用几何遮罩的 `casterTextures`，为 `spatial.effect` 绑定独立 `rim_image`。该表面保留普通表面 FX，位于投影、辉光、星芒等局部光学合成之前。禁止从最终光学合成结果提取舞台轮廓。

设原素材尺寸 W/H、透明扩展边距 P/Q，`rim_rect=(P/(W+2P),Q/(H+2Q),W/(W+2P),H/(H+2Q))`；最终纹理 UV 通过 `(uv-rim_rect.xy)/rim_rect.zw` 返回原表面 UV。邻域采样步长使用原表面纹理尺寸。`rim_crop` 使用现有 `cropParam()` 传入有效裁切范围，中心及全部邻域采样在裁切范围外返回透明，防止已裁掉的原轮廓在扩展阴影区域重新受光。轮廓颜色也取该表面。保留平面姿态、灯光方向及图层透明度；复用纹理，不新增渲染目标或额外绘制 pass。

`fx05.body` 删除内外阴影的辅助 RGB 叠加，保留独立二维偏移、角度、连续高斯模糊、扩散、阴影浓度与混合比例。阴影色继续使用原 `color2`，避免旧工程变色。目录移除 `auxiliary` 参数并关闭第一个颜色入口；UI 仅保留阴影颜色。

## 存档规则

旧 `FX05.5.parameters.auxiliary` 不再读取或写出；旧 `FX05.5.color` 不再读取或写出。`color2`、启用状态、混合强度及六个保留参数按原键名保存，不改变场景或动画格式。

## 修改文件

- `src/stage.cpp`、`src/fx_renderer.hpp`：原表面纹理复用与语义说明。
- `data/effects/spatial.effect`：原表面轮廓采样与 UV 映射。
- `src/fx_catalog_a.inc`、`src/fx_panel.cpp`、`src/fx_model.cpp`、`data/effects/fx05.body`：辅助能力移除及存档清理。
- `tests/fx_tests.cpp`、`tests/v363_fx_optics_probe.py`：更新已移除参数的原测试合同。
- `tests/v363_shadow_isolation_probe.py`、`tests/v363_shadow_isolation_restart.py`：真实 GPU、原生 Dock、旧数据和重启验证。

## 专项验证范围

旧载荷先复现；新载荷验证轮廓光开关、内外阴影、旧参数、UI、颜色预览取消、保存重启、裁切切入实体、旋转、镜像、非等比缩放、几何遮罩、半透明原素材和图层透明度。公共受光 shader 关联验证全局遮挡物与接收面映射；辉光／星芒运行现有短探针，不重做其完整参数测试。1080p60 分别记录舞台轮廓光及轮廓光加外投影的稳定资源与丢帧。

工程导入导出、缺素材报警、其他平台和无关功能全量回归为 NOT RUN。
