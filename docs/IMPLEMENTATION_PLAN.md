> V1 历史记录（保留原始结果）。当前 v2 文档与验收见 docs/v2/。

# Implementation and verification — 2026-10-30

- [x] Phase0：环境检查、固定官方 template、VS2022 编译与 OBS32 真实模块加载。logs/phase0-build.log、phase0-obs-runtime.log。
- [x] Phase1：FINISHED_LOADING 后注册五标签原生 Dock；自动路径识别脚本。logs/phase1/、phase1-build.log、installer-discovery.json。
- [x] Phase2：场景枚举/UUID/项 ID、XYZ 透视、摄像机视差、缓存纹理。logs/phase2/、phase2-build.log、core-tests-red.log、core-tests-green.log。
- [x] Phase3：关键帧/缓动、命名预设、OBS settings/重启持久化。logs/phase3/、phase3-restart/、phase3-build.log。
- [x] Phase4：点光、Alpha 简化投影、曝光/饱和度/暗角；真实输出像素对比。logs/phase4/、phase4-build.log。
- [x] Phase5：删除/重命名/场景切换/还原安全、60秒1080p60动态场景采样。logs/phase5-lifecycle-fixed/、release-installed-performance/。
- [x] Phase6：原生 EXE、真实安装/失败回滚/自卸载/重装、常规和便携 OBS 加载、资源字节审核及完整文档。logs/installer-tests.json、release-audit.json。

完成表示本设备已执行并有证据；并不将其他平台、NVIDIA 或长时间压力测试标成 PASS。所有状态、真实命令和失败修复记录见 TEST_REPORT.md。
