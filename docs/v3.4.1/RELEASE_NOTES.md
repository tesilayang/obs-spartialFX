# OBS Spatial FX v3.4.1

Windows x64 OBS 原生 Dock 插件，提供空间图层、摄像机与关键帧、舞台灯光、13 组 95 项 FX 和预设。

本版在画质修正下新增独立“抗锯齿”和“清晰化”子板块：SMAA 1x Low/Medium、FXAA、AMD RCAS。SMAA 与 FXAA 二选一，RCAS 可叠加。新功能默认关闭；AMD Native AA 留待后续研发，本版隐藏。

## 下载选哪一个

| 资产 | 用途 |
|---|---|
| OBS_Spatial_FX_V3.4.1_Setup.exe | 普通用户安装/覆盖升级，先关闭所有 OBS |
| OBS_Spatial_FX_V3.4.1_Manual.html | 新版中文操作说明，下载后双击，截图与动画均可离线阅读 |
| OBS_Spatial_FX_V3.4.1_Source.zip | 与原安装器对应的原始源码快照，保留原始字节 |
| OBS_Spatial_FX_V3.4.1_Manual.zip | HTML、Markdown、111 张真实截图、目录元数据、维护脚本与验证结果 |
| OBS_Spatial_FX_V3.4.1_Downloads_SHA256.txt | 上述四个下载文件的 SHA256 校验值 |

HTML 包含 15 章说明、95 项 FX 与 474 项参数、111 张真实测试截图和 5 组交互示意。HTML 示意不是 OBS 运行画面。GitHub 的仓库 HTML 预览会显示源码，阅读请下载 HTML 到本地打开。

原始源码 ZIP 与安装器未重打包。源码包中的旧 README/说明保留交付时状态；更新后的长期 README 与固定说明书在仓库及本次独立附件提供。GitHub 自动生成的 Source code ZIP/TAR 对应标签提交，会包含后来补充的文档。

## 安装与实际验证范围

目标 Windows 10 1909+ / Windows 11 x64、OBS 32+。插件安装在 ProgramData，不覆盖 OBS 安装目录，不附带 OBS/Qt DLL；OBS 正在运行时拒绝替换。便携版使用安装器提供的启动入口。支持已有 3.4 覆盖升级，Windows 控制面板可卸载。

实际设备：Win10 22H2 build19045、Ryzen 7 5800X、Radeon RX 6900 XT、驱动32.0.21045.20006。原交付在 OBS32.2.2 与33.0.1 实测加载、UI、保存恢复、全部95效果回归、短时1080p60本地x264编码；8组C++测试、18项安装器检查通过。详情见仓库 `docs/v3.4.1/V341_REPORT.md`。

未测整机电功耗，不承诺其他设备和开发机相同能耗或性能。集显/无独显、NVIDIA 实机、互联网推流、数小时稳定性均为 NOT RUN。本次补充文档不包含新的插件构建或运行测试；说明书的真实浏览器离线/交互检查15项通过。

开源许可与算法来源见 LICENSE、`docs/v3.4.1/ALGORITHM_PROVENANCE.md` 和 `data/effects/AA_NOTICES.txt`。
