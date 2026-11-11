# AMD Native AA 后续研发记录

状态：DEFERRED，未实现、未排期，不在 v3.4.1 界面显示。该处理已获用户明确确认。

目标是评估 AMD FSR 的原分辨率时域抗锯齿，不得把 FXAA、SMAA 或普通锐化冒名为 Native AA。

当前 Spatial FX 的图层 FX 输入只有单帧颜色/透明度纹理。近似的整层平移量不能替代逐像素运动矢量；图层平面深度也不能替代视频内容的逐像素深度。窗口捕捉和普通多媒体素材无法直接提供这些时域重建数据。

后续单独验证：

1. 明确限定可支持的舞台几何与素材类型，研究独立的合成后时域通道。
2. 提供相机抖动、当前/上一帧几何、逐像素深度、运动矢量及透明/反应遮罩；不能对未知视频内部运动假装已有可靠数据。
3. 正确处理遮挡、透明叠加、图层增删、窗口尺寸变化、镜头切换与历史帧失效，检查重影、闪烁和 UI 文字。
4. 评估 SDK 许可、D3D11/D3D12 接口及跨显卡支持，完成集显上 1080p60 与 x264 编码并行实测后，决定是否加入产品。

主要依据：AMD 官方 FidelityFX SDK Super Resolution Upscaler 文档的 Input resources、Depth buffer configurations、Providing motion vectors、Camera jitter、Temporal Antialiasing 章节。已下载的原始页面及 SHA256 保存在 `work/v3.4.1/references/`；访问地址为 `https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-upscaler/`。

本记录只建立后续研发范围，不代表当前能够对所有 OBS 素材实现 Native AA。
