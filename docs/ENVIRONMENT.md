> V1 历史记录（保留原始结果）。当前 v2 文档与验收见 docs/v2/。

# Environment evidence — 2026-10-30
- Windows NT 10.0.19045 x64 (Windows 10 22H2).
- Reference OBS: F:\Program Files (x86)\obs-studio new\bin\64bit\obs64.exe, file version 32.2.2.
- OBS-bundled Qt6Core.dll: 6.11.1.0. No Qt DLL will be redistributed.
- Display adapter registry: AMD Radeon RX 6900 XT; driver 32.0.21045.5002.
- An older OBS 29.0.2 also exists; installer must reject that version.
- No VS2022 installation or cmake/ninja on PATH was found. Provisioning VS2022 14.44 and Windows SDK 22621 inside workspace using Microsoft package manifests, with payload SHA256 verification.
- Official obs-plugintemplate commit: 3e7d7ac3b5342cd7d9b88890b9c70b472d1520fc.
- Official OBS32.2.2 headers: upstream codeload source archive SHA256 35d3cd0979d65664fada7119fdb612eca7c34b61a1623a330caec74bf72626c4.
- OBS Qt6 dev archive 2025-07-11: SHA256 0e76bf0555dd5382838850b748d3dcfab44a1e1058441309ab54e1a65b156d0a (matches template buildspec).
- CMake 4.4.5 archive SHA256 326c826486d46a23cf9d5a1c9ee3a567d427bdd02c4a34071937a2a675cff004 (matches upstream release digest).
- System Schannel HTTPS fails SEC_E_NO_CREDENTIALS. Python OpenSSL HTTPS and Git OpenSSL work; GitHub release assets fetched through official API, not a third-party mirror.

## Portable OBS discovery
Source inspection found portable OBS intentionally skips ProgramData plugin scanning (frontend/widgets/OBSBasic.cpp). Installer must therefore provide a portable OBS launch shortcut that injects OBS_PLUGINS_PATH and OBS_PLUGINS_DATA_PATH for the child process only. It must not modify OBS installation files or global environment settings.
