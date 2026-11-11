# OBS Spatial FX 2.0 design and execution record
Date: 2026-10-30. Existing v1 delivery preserved in dist.
## Contract
Keep stage/dock IDs, ProgramData path, preset location and v1 schema migration. Native Qt6 dock only. Max32 layers / 4 lights, reusable GPU capture textures, no render-thread UI locks. Use OBS32 headers and /utf-8.
## Work batches
1. Reproduce adjustment stress in real OBS, inspect available crash evidence, test invalid/near-plane/recursive sources. Fix proven causes with regression coverage.
2. Import window capture including zero-size/not-yet-ready sources, crop/bounds/mirror. Preserve source identity, derive original display footprint, initialize staggered depths with reference-view compensation. Scene selector + source browser.
3. Add camera target/orbit/pan/dolly/tilt, animation duration/speed, perspective strength and per-layer focus blur. Retain manual controls/keyframes.
4. Per-layer light/cast/receive/rim flags and explicit receiver selection. Up to4 point/area emitters with source binding, size, falloff and soft shadows. Ray-plane intersections in receiver-local UV; alpha sample caster for occlusion, alpha gradient for directional rim. No flat artificial receiver background.
5. Real native UI, window-capture recovery, parameter stress, pixel comparisons and 1080p60; v1 -> v2 upgrade/restart/config checks; package source/symbols/evidence.
## Validation
Each batch builds via cmd.exe /d /c tools\build.cmd 4. CTest via tools\test.cmd. Native QA driver and websocket GPU screenshots exercise real module, never shipped. All reports distinguish PASS/FAIL/NOT RUN/BLOCKED. Original Windows Application event on 2026-10-30 15:30:27 reports ntdll / 0xc0000374 heap corruption. Native debugger reproduced obs_data_array_release lifetime faults (module offsets; full DbgHelp symbols unavailable). Dock publishes immutable snapshots and persists detached model under source private_settings. Generic live public updates are intentionally ignored to avoid shared obs_data hash/array/JSON-cache traversal. Owned proc API is the supported live integration boundary. See API.md and TEST_REPORT.md.
## Camera reference
Read entire HoraikenHorizonCamera.lua from user path. Reference uses synchronized motion amplitude depending on depth, rather than a physical 3D camera. Implement physical focal/(distance+Z) perspective; depth assignment makes near layers move more. Do not copy the Lua shader.
## Limits
Alpha cutouts are flat planes; no volumetric mesh normals or physically based ray-traced GI. Area light shadow is deterministic multisampling of a rectangular emitter; DOF is bounded image-space alpha-aware blur.
