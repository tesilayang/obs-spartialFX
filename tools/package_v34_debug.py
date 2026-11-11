"""Audit the 2026-11-11 v3.4 patch; replace dist files only with --package.

Default execution is check-only. Missing/incomplete runtime evidence fails closed.
The original manuals and implementation map are never rewritten.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import shutil
import struct
import sys
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "work/v3.4-debug-20261111"
DIST = ROOT / "dist"
EXPECTED_PLUGIN = "b0d9cebee34cdd499480eac0e874eea5f9f34e2669df96bfe1cfad597477de40"
ORIGINAL_MAP_SHA = "9fc81c83fd12390449cfeed88c5309b73d3441ff5e19c5f3b61fc48da4154cb8"
UI = ("installed-ui32-final", "installed-persist32", "installed-ui33-final", "installed-persist33")
GPU = ("gpu32-final", "gpu33-final")
PATCH = ("patch32", "patch33-final2")
LIMITATIONS = [
    "NOT RUN: NVIDIA hardware verification (no NVIDIA device available).",
    "NOT RUN: OBS34+ and future hardware/runtime combinations (no runtime available).",
    "NOT RUN: hours-long or all-92-simultaneous stress test; completed performance coverage is a 30-second representative FX chain per OBS version.",
    "Pixel effects remain bounded by the source rectangle; outer shadows need transparent source margins.",
    "Large fixed-budget blur radii approximate a dense convolution and may undersample tiny details.",
]


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def write_json(path, data):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def result_passes(result, count=None):
    if not isinstance(result, dict) or result.get("failed") != 0:
        return False
    checks = result.get("checks")
    return (isinstance(checks, list) and bool(checks)
            and (count is None or len(checks) == count)
            and all(isinstance(check, dict) and check.get("pass") is True for check in checks))


def public_json(value):
    """Retain results while omitting embedded scene configuration from native QA."""
    if isinstance(value, dict):
        return {key: public_json(item) for key, item in value.items()
                if key not in {"stageSettings", "sceneSettings", "sourceSettings"}}
    if isinstance(value, list):
        return [public_json(item) for item in value]
    return value


PUBLIC_JSON = {"dock-smoke.json", "installed-path.json", "render-tests.json", "performance.json",
               "coordinate-tests.json", "patch-tests.json", "installer-tests.json",
               "install-running-guard.json", "blur-kernel-signatures.json", "final-package-audit.json",
               "package-checks.json", "package-input-hashes.json", "package-validation.json", "commands.json"}


def include_evidence(relative):
    relative = Path(relative)
    pieces = {part.lower() for part in relative.parts}
    if pieces & {"baseline", "baseline-runtime", "installed-before", "__pycache__", "package-staging"}:
        return False
    name = relative.name.lower()
    if any(word in name for word in ("control-", "native-command", "native-answer", "websocket", "scene", "config", "fixture", "answer")):
        return False
    if relative.suffix.lower() == ".json":
        return name in PUBLIC_JSON
    return relative.suffix.lower() in {".log", ".md", ".png", ".py", ".cmd", ".cpp"}


def archive(target, files, root=ROOT, sanitize=False):
    files = sorted(set(files))
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as output:
        for path in files:
            name = path.relative_to(root).as_posix()
            if sanitize and path.suffix.lower() == ".json":
                output.writestr(name, json.dumps(public_json(read_json(path)), ensure_ascii=False, indent=2) + "\n")
            else:
                output.write(path, name)
    with zipfile.ZipFile(target) as output:
        if output.testzip() is not None:
            raise RuntimeError(f"CRC failure: {target.name}")
        for path in files:
            expected = path.read_bytes()
            if sanitize and path.suffix.lower() == ".json":
                expected = (json.dumps(public_json(read_json(path)), ensure_ascii=False, indent=2) + "\n").encode("utf-8")
            if output.read(path.relative_to(root).as_posix()) != expected:
                raise RuntimeError(f"Archive content mismatch: {path}")


class Audit:
    def __init__(self):
        self.checks = []

    def check(self, name, condition):
        try:
            success = bool(condition())
            error = "" if success else "condition not satisfied"
        except (OSError, ValueError, KeyError, TypeError, AttributeError, IndexError, struct.error) as exception:
            success, error = False, f"{type(exception).__name__}: {exception}"
        self.checks.append({"name": name, "pass": success, **({"detail": error} if error else {})})
        print(("PASS " if success else "PENDING/FAIL ") + name, flush=True)

    @property
    def failed(self):
        return sum(not check["pass"] for check in self.checks)


def generate_mapping():
    runtime = read_json(ROOT / "build/fx-runtime-catalog.json")["effects"]
    original = read_json(ROOT / "docs/v3.4/V34_IMPLEMENTATION_MAP.json")
    old = {effect["effect_id"]: effect for effect in original["effects"]}
    effects = []
    for effect in runtime:
        entry = dict(effect)
        entry["effect_id"] = entry.pop("id")
        entry["source_ids"] = old.get(entry["effect_id"], {}).get("source_ids", [])
        entry["debug_patch_status"] = "added" if entry["effect_id"] == "FX12.9" else "retained-stable-id"
        effects.append(entry)
    mapping = {"version": "3.4.0", "patchDate": "2026-11-11", "groups": 13,
               "suboptions": len(effects), "catalogSha256": sha(ROOT / "build/fx-runtime-catalog.json"),
               "originalImplementationMapSha256": sha(ROOT / "docs/v3.4/V34_IMPLEMENTATION_MAP.json"),
               "originalSelectionFileUnchanged": True,
               "retired": [{"effect_id": "FX11.1", "reason": "User requested removal of layered camera parallax", "source_ids": old.get("FX11.1", {}).get("source_ids", [])}],
               "originalSourceSelections": 113,
               "mappingNote": "Original selections remain historical evidence; removed FX11.1 source IDs are recorded as retired, and new FX12.9 is a user requested patch addition.",
               "effects": effects}
    write_json(ROOT / "docs/v3.4/V34_DEBUG_IMPLEMENTATION_MAP.json", mapping)
    return mapping


def evaluate_gates():
    audit = Audit()
    audit.check("final plugin equals approved installed build SHA256", lambda: sha(ROOT / "build/obs-spatial-fx.dll") == EXPECTED_PLUGIN)
    audit.check("CTest eight tests passed", lambda: "100% tests passed out of 8" in (WORK / "ctest-final.log").read_text(encoding="utf-8-sig"))
    for name, marker in [("build-final.log", "Linking CXX executable OBS_Spatial_FX_V3.4_Setup.exe"),
                         ("build-qa-final.log", "Linking CXX shared module spatial-qa-driver.dll")]:
        audit.check(name + " successful final build trace", lambda name=name, marker=marker:
                    marker in (WORK / name).read_text(encoding="utf-8-sig") and not re.search(r"FAILED:|fatal error|error C\d{4}", (WORK / name).read_text(encoding="utf-8-sig")))
    for folder in UI:
        expected_count = 13 if "persist" in folder else 62
        audit.check(folder + f" {expected_count} native UI/persistence checks", lambda folder=folder, count=expected_count:
                    result_passes(read_json(WORK / folder / "dock-smoke.json"), count))
        def exact_installed(folder=folder):
            result = read_json(WORK / folder / "installed-path.json")
            expected_major = 32 if "32" in folder else 33
            paths = result["LoadedPaths"]
            expected = "C:/ProgramData/obs-studio/plugins/obs-spatial-fx/" + ("bin/64bit/obs-spatial-fx.dll" if expected_major == 32 else "portable33/obs-spatial-fx/obs-spatial-fx.dll")
            return (result["Major"] == expected_major and result["InstalledHash"].lower() == EXPECTED_PLUGIN
                    and len(paths) == 1 and paths[0].replace("\\", "/").lower() == expected.lower())
        audit.check(folder + " exact installed path and hash", exact_installed)
    for folder in PATCH:
        audit.check(folder + " 36 targeted pixel/stage regressions", lambda folder=folder:
                    result_passes(read_json(WORK / folder / "patch-tests.json"), 36))
    for folder in GPU:
        def render_pass(folder=folder):
            result = read_json(WORK / folder / "render-tests.json")
            effects = result["effects"]
            return (result_passes(result, 196) and len(effects) == 92
                    and len({effect["id"] for effect in effects}) == 92
                    and sum(effect["variants"] for effect in effects) == 237
                    and all(effect["gpuPass"] and effect["defaultVisible"] for effect in effects))
        audit.check(folder + " 92 effects / 237 variants / 196 GPU checks", render_pass)
        def performance_pass(folder=folder):
            result = read_json(WORK / folder / "performance.json")
            samples = result["samples"]
            before, warm, after = result["before"], result["warmMetrics"], result["afterMetrics"]
            return (len(samples) >= 30 and warm["allocations"] == after["allocations"]
                    and warm["failures"] == after["failures"] == 0
                    and all(sample["activeFps"] > 59 and sample["renderSkippedFrames"] == before["renderSkippedFrames"] for sample in samples)
                    and samples[-1]["renderTotalFrames"] - before["renderTotalFrames"] >= 1700)
        audit.check(folder + " 30-second performance plateau >59fps and no new skipped frames", performance_pass)
        audit.check(folder + " eight coordinate regressions", lambda folder=folder:
                    result_passes(read_json(WORK / folder / "coordinates/coordinate-tests.json"), 8))
    audit.check("installer eighteen upgrade/rollback/uninstall checks", lambda:
                len(read_json(WORK / "installer/installer-tests.json")) == 18
                and all(check.get("pass") is True for check in read_json(WORK / "installer/installer-tests.json")))
    audit.check("running OBS guard exit 10 and unchanged installed hash", lambda:
                read_json(WORK / "install-running-guard.json")["ExitCode"] == 10
                and read_json(WORK / "install-running-guard.json")["HashUnchanged"] is True)
    for relative, expected in read_json(ROOT / "work/v3.4-development/historical-hashes.json").items():
        audit.check("preserved historical file " + relative, lambda relative=relative, expected=expected: sha(ROOT / relative) == expected.lower())
    for name, expected in read_json(ROOT / "work/v3.3-ui-20261110/manual-hashes.json").items():
        audit.check("unchanged manual " + Path(name).name, lambda name=name, expected=expected: sha(name) == expected.lower())
    audit.check("original selection file exact preserved SHA256", lambda:
                sha(ROOT / "docs/v3.4/OBS_Spatial_FX_v3.4_我的选择.json") == "909de115de4fcdfa4ae5280fadfc7e6febd3e8a3bb31f06185df7cc252210e11")
    audit.check("v3.3 installer exact preserved SHA256", lambda:
                sha(DIST / "OBS_Spatial_FX_V3.3_Setup.exe") == "1e704adb6fd4967b6674d3c4cf33deed65b03813c2a3dc27366141e54ac94f72")
    audit.check("original v3.4 implementation map unchanged", lambda: sha(ROOT / "docs/v3.4/V34_IMPLEMENTATION_MAP.json") == ORIGINAL_MAP_SHA)
    baseline = read_json(WORK / "baseline/V34_RELEASE.json")
    for name, expected in baseline["artifacts"].items():
        audit.check("original v3.4 artifact preserved in baseline " + name, lambda name=name, expected=expected: sha(WORK / "baseline" / name) == expected)
    base = Path("C:/ProgramData/obs-studio/plugins/obs-spatial-fx")
    for name in ("obs-spatial-fx.dll", "bin/64bit/obs-spatial-fx.dll", "portable33/obs-spatial-fx/obs-spatial-fx.dll"):
        audit.check("final installed layout exact hash " + name, lambda name=name: sha(base / name) == EXPECTED_PLUGIN)
    audit.check("installed uninstaller equals final setup", lambda: sha(base / "Uninstall.exe") == sha(ROOT / "build/OBS_Spatial_FX_V3.4_Setup.exe"))
    audit.check("installed payload contains no Qt or QA DLL", lambda: base.is_dir() and not list(base.rglob("Qt*.dll")) and not list(base.rglob("*qa-driver*")))
    for major in (32, 33):
        audit.check(f"OBS{major} temporary test websocket disabled", lambda major=major:
                    read_json(ROOT / f"runtime/obs{major}/config/obs-studio/plugin_config/obs-websocket/config.json")["server_enabled"] is False)
    def windows_installer():
        data = (ROOT / "build/OBS_Spatial_FX_V3.4_Setup.exe").read_bytes()
        pe = struct.unpack_from("<I", data, 0x3c)[0]
        return data[:2] == b"MZ" and struct.unpack_from("<H", data, pe + 4)[0] == 0x8664 and struct.unpack_from("<H", data, pe + 24 + 68)[0] == 2
    audit.check("real Windows x64 GUI installer", windows_installer)
    audit.check("v3.4 version unchanged", lambda: "project(obs-spatial-fx VERSION 3.4.0" in (ROOT / "CMakeLists.txt").read_text(encoding="utf-8-sig"))
    def latest_catalog():
        effects = read_json(ROOT / "build/fx-runtime-catalog.json")["effects"]
        ids = [effect["id"] for effect in effects]
        return len(ids) == len(set(ids)) == 92 and "FX11.1" not in ids and ids.index("FX12.9") + 1 == ids.index("FX12.6")
    audit.check("latest runtime catalog stable IDs and gradient order", latest_catalog)
    return audit


def changed_files():
    extensions = {".cpp", ".hpp", ".h", ".inc", ".body", ".effect", ".py", ".ps1", ".cmd", ".json", ".in"}
    files = [ROOT / "CMakeLists.txt"]
    for folder in ("src", "data", "installer", "tests", "tools"):
        iterator = (ROOT / folder).iterdir() if folder == "tools" else (ROOT / folder).rglob("*")
        files.extend(path for path in iterator if path.is_file() and path.suffix in extensions)
    return [path.relative_to(ROOT).as_posix() for path in sorted(files)
            if not (WORK / "baseline" / path.relative_to(ROOT)).exists()
            or sha(path) != sha(WORK / "baseline" / path.relative_to(ROOT))]


def result_summary(folder, filename):
    path = WORK / folder / filename
    if not path.exists():
        return "待完成：结果文件尚不存在"
    result = read_json(path)
    return f"{len(result.get('checks', []))} 项，失败 {result.get('failed', '未知')}"


def write_report(audit):
    rows = []
    for folder in UI:
        rows.append(f"| {folder} | {result_summary(folder, 'dock-smoke.json')} |")
    for folder in PATCH:
        rows.append(f"| {folder} | {result_summary(folder, 'patch-tests.json')} |")
    for folder in GPU:
        rows.append(f"| {folder} | {result_summary(folder, 'render-tests.json')} |")
        rows.append(f"| {folder}/coordinates | {result_summary(folder + '/coordinates', 'coordinate-tests.json')} |")
    pending = [check["name"] for check in audit.checks if not check["pass"]]
    status = "全部打包门禁通过；本报告本身不代表 dist 已替换。" if not pending else f"尚有 {len(pending)} 项打包门禁未满足；禁止交付打包。"
    text = """# v3.4 修复验证报告（2026-11-11）

版本仍为 3.4.0。此报告与 `V34_DEBUG_IMPLEMENTATION_MAP.json` 是本次修复的新增记录；原说明书、原实现映射、原始用户选择 JSON 和 v3.3 安装包保留。

## 当前交付门禁

{status}

最终插件 SHA256：`{plugin}`。`tools/package_v34_debug.py` 默认只检查门禁并刷新报告/映射/审计；只有显式 `--package` 且所有门禁通过后，才在 work 暂存并替换 dist 中同名 v3.4 安装包、源码包、符号包、证据包、发布清单和校验和。原 v3.4 安装包及三个压缩包保存在本次 work 的 baseline 目录，并按原发布清单校验。

| 验证目录 | 实际结果 |
| --- | --- |
{rows}

完整门禁结果见 `work/v3.4-debug-20261111/package-checks.json`。构建证据为 `build-final.log` 与仅重建 QA driver 的 `build-qa-final.log`；`ctest-final.log` 记录 8/8 通过。安装测试需 18/18；运行中保护需退出码 10 且原 DLL 哈希不变。UI 主测试每版本 62 项，重启持久化每版本 13 项；针对性像素/舞台回归每版本 36 项；最终广泛 GPU 回归每版本 92 个效果、237 个默认/模式变体、196 项检查；坐标回归每版本 8 项。

每个最终 GPU 目录还必须有至少 30 个逐秒样本：预热后分配数不增加、无 shader 失败、每个样本 >59fps、无新增 renderSkippedFrames。较早 `gpu33` 运行使用中间构建，仅留作诊断；交付门禁要求 `gpu33-final` 最终安装版本重新运行。

## 行为修改与边界

- 模糊：高斯采用中心加权的固定 7×7 核；方框采用同范围均匀权重；镜头散景采用等面积光圈采样，支持圆形和多边形。镜像是边界延伸规则，不是独立模糊核；画面内部相同核应相同。固定采样预算避免半径增大时无界增长，最大半径仍是近似。
- 受光：没有启用的空间灯时，只为已启用材质 FX 提供随图层位置/朝向/范围变化的局部暖色效果灯；不改模型中的舞台灯状态。存在明确启用的空间灯时沿用其绑定、颜色、强度和位置，包括用户指定的零强度。环境辅助源是可选输入；背光增加可见内侧轮廓，镜面反射采用有限介电 Fresnel。外侧阴影受素材矩形裁切，需源图内留透明边；新建阴影默认内侧，旧外侧模式仍按原键值读取。
- 移除 FX11.1 分层镜头视差，保留兄弟 ID。新增 FX12.9 渐变叠加，紧邻混合模式之前；纯色、线性和径向模式，两端颜色、中心、范围、柔度、反向复选框、线性角度和径向椭圆比例。模式无关参数隐藏，数值步进 0.01；只改变颜色、保留素材 Alpha。
- LED：改为按真实素材单元采样，再以 RGB 灯珠或圆形灯珠重建画面，带暗缝与发光。使用当前素材的颜色与画面结构。
- 宽银幕：FX13.1 在完整舞台合成后统一处理固定居中的 21:9。黑边模式外围不透明；裁切模式外围透明。它不修改 OBS 全局分辨率。多个图层同时启用时，以时间轴中第一个实际有效项决定最终舞台配置；禁用后恢复。

## 失败、修正与重试记录

首次真实运行画面全零，原因是新舞台 shader 常量未初始化；已补齐初始化并在后续构建/实际像素测试重新验证。UI32 首次测试把 Qt deleteLater 留下的隐藏旧 QObject 当作活动控件；修正的是 QA 查找活动控件的断言，没有为测试改变产品行为。第一次 GPU32 全目录运行在性能采样完成前被主代理误关 OBS，该次不能计作通过；只接受 `gpu32-final` 完整重试证据。早期不完整/失败日志保留供追溯。

## 可复现命令

```powershell
cmd /c tools\\build.cmd 4
cmd /c tools\\test.cmd
powershell -File tools/run_v33_installed_qa.ps1 -Major 32 -Scenario v34 -Output work/v3.4-debug-20261111/installed-ui32-final -Wait
powershell -File tools/run_v33_installed_qa.ps1 -Major 33 -Scenario v34 -Output work/v3.4-debug-20261111/installed-ui33-final -Wait
powershell -File tools/test-installer-v34-debug.ps1
python tools/package_v34_debug.py --check
# 主任务最终确认完整结果后才执行：
python tools/package_v34_debug.py --package
```

重启持久化使用 `-Scenario v34-persist`，输出到相应 `installed-persist32/33`。GPU 与像素脚本依赖已通过安装启动器打开的隔离 OBS 和临时 websocket；具体本次启动/运行输出见 work 下相应 `.log`。包装脚本不启动、不关闭 OBS，不替代真实运行测试。

## 修改文件

以下由源码与本次 baseline 逐文件 SHA256 比较生成（新增报告/映射另列）：

{files}

新增交付记录：`docs/v3.4/V34_DEBUG_REPORT.md`、`docs/v3.4/V34_DEBUG_IMPLEMENTATION_MAP.json`。源码压缩包逐字节校验，符号包包含最终插件与安装器 PDB，证据包包含相关日志、截图、公开检查结果和审计。QA 场景 JSON、内嵌 stageSettings、websocket 备份、control/native command/answer、baseline 和已安装备份不进入公开证据包。

## 已知验证范围

{limitations}

原用户选择 SHA256：`909de115de4fcdfa4ae5280fadfc7e6febd3e8a3bb31f06185df7cc252210e11`。保留 v3.3 安装包 SHA256：`1e704adb6fd4967b6674d3c4cf33deed65b03813c2a3dc27366141e54ac94f72`。原说明书逐项哈希以 `work/v3.3-ui-20261110/manual-hashes.json` 为准。
""".format(status=status, plugin=EXPECTED_PLUGIN, rows="\n".join(rows),
           files="\n".join("- `" + path + "`" for path in changed_files()),
           limitations="\n".join("- " + item for item in LIMITATIONS))
    (ROOT / "docs/v3.4/V34_DEBUG_REPORT.md").write_text(text, encoding="utf-8")


def package(audit, mapping):
    if audit.failed:
        raise RuntimeError("Required gates failed; dist has not been changed")
    staging = WORK / "package-staging"
    staging.mkdir(exist_ok=True)
    setup = ROOT / "build/OBS_Spatial_FX_V3.4_Setup.exe"
    names = ["OBS_Spatial_FX_V3.4_Setup.exe", "OBS_Spatial_FX_Setup.exe"]
    for name in names:
        shutil.copy2(setup, staging / name)
    extensions = {".cpp", ".hpp", ".h", ".inc", ".body", ".effect", ".py", ".md", ".txt", ".ps1", ".cmd", ".json", ".in", ".ini", ".bat", ".html"}
    sources = [ROOT / name for name in ("CMakeLists.txt", "README.md", "BUILDING.md", "LICENSE", ".gitignore")]
    for folder in ("src", "data", "installer", "tests", "docs"):
        sources.extend(path for path in (ROOT / folder).rglob("*") if path.is_file() and path.suffix in extensions and "__pycache__" not in path.parts)
    sources.extend(path for path in (ROOT / "tools").iterdir() if path.is_file() and path.suffix in extensions)
    source_zip = "OBS_Spatial_FX_V3.4_Source.zip"
    symbol_zip = "OBS_Spatial_FX_V3.4_Symbols.zip"
    evidence_zip = "OBS_Spatial_FX_V3.4_Evidence.zip"
    archive(staging / source_zip, sources)
    archive(staging / symbol_zip, [ROOT / "build/obs-spatial-fx.pdb", ROOT / "build/OBS_Spatial_FX_V3.4_Setup.pdb"])
    inputs = {path.relative_to(ROOT).as_posix(): sha(path) for path in sources}
    write_json(WORK / "package-input-hashes.json", inputs)
    audit.check("source archive CRC and exact bytes", lambda: (staging / source_zip).is_file())
    audit.check("symbols archive CRC and exact bytes", lambda: (staging / symbol_zip).is_file())
    write_json(WORK / "final-package-audit.json", {"checks": audit.checks, "failed": audit.failed, "pluginSha256": EXPECTED_PLUGIN, "phase": "all inputs and source/symbol archives verified; staging evidence"})
    evidence = [path for path in WORK.rglob("*") if path.is_file() and include_evidence(path.relative_to(WORK))]
    evidence.extend([ROOT / "docs/v3.4/V34_DEBUG_REPORT.md", ROOT / "docs/v3.4/V34_DEBUG_IMPLEMENTATION_MAP.json"])
    archive(staging / evidence_zip, evidence, sanitize=True)
    audit.check("public evidence archive CRC, sanitized JSON and exact remaining bytes", lambda: (staging / evidence_zip).is_file())
    artifacts = {name: sha(staging / name) for name in names + [source_zip, symbol_zip, evidence_zip]}
    manifest = {"version": "3.4.0", "date": "2026-11-11", "patch": "debug-fixes-20261111", "pluginSha256": EXPECTED_PLUGIN,
                "artifacts": artifacts, "groups": 13, "suboptions": len(mapping["effects"]), "originalSourceSelections": 113,
                "retiredEffectIds": ["FX11.1"], "addedEffectIds": ["FX12.9"], "manualsUnchanged": True,
                "guiChecks": sum(len(read_json(WORK / folder / "dock-smoke.json")["checks"]) for folder in UI),
                "gpuVariantsPerOBSVersion": 237, "gpuChecksPerOBSVersion": 196, "patchChecksPerOBSVersion": 36,
                "coordinateChecksPerOBSVersion": 8, "ctestPassed": 8, "installerChecks": 18,
                "evidenceFolders": list(UI + PATCH + GPU), "implementationMap": "docs/v3.4/V34_DEBUG_IMPLEMENTATION_MAP.json",
                "limitations": LIMITATIONS}
    write_json(staging / "V34_RELEASE.json", manifest)
    to_publish = names + [source_zip, symbol_zip, evidence_zip, "V34_RELEASE.json"]
    # Recheck pinned binary immediately before replacement; evidence always gates publication.
    if sha(ROOT / "build/obs-spatial-fx.dll") != EXPECTED_PLUGIN or sha(setup) != artifacts[names[0]]:
        raise RuntimeError("Build changed during packaging; dist has not been changed")
    for name in to_publish:
        (staging / name).replace(DIST / name)
    for name, expected in artifacts.items():
        audit.check("published artifact exact hash " + name, lambda name=name, expected=expected: sha(DIST / name) == expected)
    files = sorted(path for path in DIST.rglob("*") if path.is_file() and path.name != "SHA256SUMS.txt")
    sums = "".join(sha(path) + "  " + path.relative_to(DIST).as_posix() + "\n" for path in files)
    (staging / "SHA256SUMS.txt").write_text(sums, encoding="utf-8")
    (staging / "SHA256SUMS.txt").replace(DIST / "SHA256SUMS.txt")
    write_json(WORK / "final-package-audit.json", {"checks": audit.checks, "failed": audit.failed, "pluginSha256": EXPECTED_PLUGIN, "phase": "published", "artifacts": artifacts})
    if audit.failed:
        raise RuntimeError("Published artifact verification failed")
    print("Packaged final v3.4 debug patch; setup SHA256 " + sha(setup), flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="evaluate all gates without modifying dist (default)")
    mode.add_argument("--package", action="store_true", help="replace current v3.4 dist artifacts after all gates pass")
    args = parser.parse_args()
    audit = evaluate_gates()
    mapping = generate_mapping()
    write_report(audit)
    write_json(WORK / "package-checks.json", {"checks": audit.checks, "failed": audit.failed,
               "pluginSha256": EXPECTED_PLUGIN, "mode": "package" if args.package else "check-only"})
    if audit.failed:
        print(f"{audit.failed} gate(s) pending/failed; dist unchanged", flush=True)
        return 1
    if args.package:
        package(audit, mapping)
    else:
        print("All gates pass; check-only completed, dist unchanged", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
