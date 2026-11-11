"""Fail-closed v3.4 color/UI audit; replace dist only with --package.

This tool never builds, launches OBS, invents results or rewrites documentation.
Default/--check writes only work/v3.4-colors-20261111/package-checks.json.
"""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import sys
import uuid
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "work/v3.4-colors-20261111"
RELEASE, DIST = WORK / "release", ROOT / "dist"
PLUGIN = ROOT / "build/obs-spatial-fx.dll"
SETUP = ROOT / "build/OBS_Spatial_FX_V3.4_Setup.exe"
EXPECTED_PLUGIN = "a6c5111abe23d28019b088c7987e96bb4d117a2202c753e01c9203a284e3daca"
ORIGINAL_MAP_SHA = "9fc81c83fd12390449cfeed88c5309b73d3441ff5e19c5f3b61fc48da4154cb8"
SELECTION_SHA = "909de115de4fcdfa4ae5280fadfc7e6febd3e8a3bb31f06185df7cc252210e11"
OLD_SETUP_SHA = "1e704adb6fd4967b6674d3c4cf33deed65b03813c2a3dc27366141e54ac94f72"
REPORT = ROOT / "docs/v3.4/V34_COLORS_REPORT.md"
UI = ("ui32", "persist32", "ui33", "persist33")
GPU = ("gpu32", "gpu33")
TARGETED = {"colors": ("live-colors.json", 13), "quality": ("blur-quality.json", 14), "edge": ("blur-edgecases.json", 10),
            "light": ("lighting-controls.json", 17), "patch": ("patch-tests.json", 36),
            "siblings": ("blur-siblings.json", 6)}
FOLDERS = UI + GPU + tuple(prefix + str(major) for prefix in TARGETED for major in (32, 33))
NAMES = ("OBS_Spatial_FX_V3.4_Setup.exe", "OBS_Spatial_FX_Setup.exe",
         "OBS_Spatial_FX_V3.4_Source.zip", "OBS_Spatial_FX_V3.4_Symbols.zip",
         "OBS_Spatial_FX_V3.4_Evidence.zip")
LIMITATIONS = [
    "NOT RUN: NVIDIA hardware verification (no NVIDIA device available).",
    "NOT RUN: OBS34+ and future hardware/runtime combinations (no runtime available).",
    "NOT RUN: hours-long or all-92-simultaneous stress test; performance coverage is a 30-second representative FX chain per OBS version.",
    "Pixel effects remain bounded by the source rectangle; outer shadows need transparent source margins.",
    "Large blurs use antialiased reduced-resolution convolution; variable-radius radial/tilt blur remains an approximation to a full varying 2D kernel.",
]


def read_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8-sig"))


def json_bytes(value):
    return (json.dumps(value, ensure_ascii=False, indent=2) + "\n").encode("utf-8")


def write_json(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_bytes(json_bytes(value))
    os.replace(temporary, path)


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def fresh(path):
    return Path(path).is_file() and Path(path).stat().st_mtime_ns >= PLUGIN.stat().st_mtime_ns


def passes(value, count):
    return (isinstance(value, dict) and type(value.get("failed")) is int and value["failed"] == 0
            and isinstance(value.get("checks"), list) and len(value["checks"]) == count
            and all(isinstance(item, dict) and item.get("pass") is True for item in value["checks"])
            and all(value[key] is True for key in ("completed", "passed") if key in value))


class Audit:
    def __init__(self):
        self.checks = []

    def check(self, name, condition):
        try:
            passed = bool(condition())
            detail = "" if passed else "condition not satisfied"
        except (OSError, ValueError, KeyError, TypeError, AttributeError, IndexError, struct.error) as error:
            passed, detail = False, f"{type(error).__name__}: {error}"
        self.checks.append({"name": name, "pass": passed, **({"detail": detail} if detail else {})})
        print(("PASS " if passed else "PENDING/FAIL ") + name, flush=True)

    @property
    def failed(self):
        return sum(item["pass"] is not True for item in self.checks)


def installed_path_ok(folder, major, expected):
    path = RELEASE / folder / "installed-path.json"
    value = read_json(path)
    suffix = "bin/64bit/obs-spatial-fx.dll" if major == 32 else "portable33/obs-spatial-fx/obs-spatial-fx.dll"
    installed = "C:/ProgramData/obs-studio/plugins/obs-spatial-fx/" + suffix
    paths = value["LoadedPaths"]
    loaded = value.get("LoadedModules", [])
    return (fresh(path) and value["Major"] == major and value["InstalledHash"].lower() == expected
            and isinstance(paths, list) and len(paths) == 1
            and paths[0].replace("\\", "/").lower() == installed.lower()
            and len(loaded) == 1 and loaded[0]["Path"] == paths[0]
            and loaded[0]["Sha256"].lower() == expected
            and len(value.get("ShaderAssets", [])) == 17
            and all(sha(ROOT / "data/effects" / asset["Name"]) == asset["Sha256"].lower() for asset in value["ShaderAssets"]))


def release_run_ok(expected):
    path = RELEASE / "release-run.json"
    value = read_json(path)
    codes = value["exitCodes"]
    return (fresh(path) and value.get("completed") is True and value["pluginSha256"].lower() == expected
            and all(value[key] is True for key in ("passed",) if key in value)
            and len(value["folders"]) == len(FOLDERS) and set(value["folders"]) == set(FOLDERS) and isinstance(codes, dict)
            and set(codes) >= set(FOLDERS)
            and all(type(code) is int and code == 0 for code in codes.values()))


def performance_ok(path):
    value = read_json(path)
    samples, before = value["samples"], value["before"]
    warm, after = value["warmMetrics"], value["afterMetrics"]
    return (fresh(path) and len(samples) >= 30
            and all(value[key] is True for key in ("completed", "passed") if key in value)
            and warm["allocations"] == after["allocations"] and warm["failures"] == after["failures"] == 0
            and all(sample["activeFps"] > 59 and sample["renderSkippedFrames"] == before["renderSkippedFrames"] for sample in samples)
            and all(samples[index]["renderTotalFrames"] > samples[index - 1]["renderTotalFrames"] for index in range(1, len(samples)))
            and samples[-1]["renderTotalFrames"] - before["renderTotalFrames"] >= 1700)


def windows_installer_ok():
    data = SETUP.read_bytes()
    offset = struct.unpack_from("<I", data, 0x3c)[0]
    return (data[:2] == b"MZ" and data[offset:offset + 4] == b"PE\0\0"
            and struct.unpack_from("<H", data, offset + 4)[0] == 0x8664
            and struct.unpack_from("<H", data, offset + 24 + 68)[0] == 2)


def preserved_manifest(audit, path, label, absolute=False):
    audit.check(label + " hash manifest exists and is nonempty", lambda:
                isinstance(read_json(path), dict) and bool(read_json(path)))
    try:
        for name, expected in read_json(path).items():
            audit.check(label + " " + name, lambda name=name, expected=expected:
                        sha(Path(name) if absolute else ROOT / name) == expected.lower())
    except (OSError, ValueError, AttributeError):
        pass  # The manifest gate records this failure.


def evaluate_gates(expected):
    audit = Audit()
    audit.check("final build DLL matches explicitly pinned SHA256", lambda: sha(PLUGIN) == expected)
    def build_ok():
        build4 = (WORK / "build-final4.log").read_text(encoding="utf-8-sig")
        build5 = ""
        return ("Linking CXX shared module obs-spatial-fx.dll" in build4
                and "Linking CXX executable OBS_Spatial_FX_V3.4_Setup.exe" in build4
                and "Build files have been written" in build4
                and not re.search(r"FAILED:|fatal error|error C\d{4}|CMake Error", build4 + build5))
    audit.check("final build rebuilt DLL/setup successfully", build_ok)
    audit.check("fresh final CTest eight tests passed", lambda: fresh(WORK / "ctest-final.log")
                and "100% tests passed out of 8" in (WORK / "ctest-final.log").read_text(encoding="utf-8-sig"))
    audit.check("completed final release-run manifest; actual exit codes zero", lambda: release_run_ok(expected))
    for major in (32, 33):
        audit.check(f"OBS{major} GPU bridge loaded exact final installed DLL", lambda major=major: installed_path_ok(f"bridge{major}", major, expected))
        for folder in (f"bridge{major}", f"ui{major}", f"persist{major}"):
            def logs_ok(folder=folder):
                path = RELEASE / folder / "obs-runtime.log"
                log = path.read_text(encoding="utf-8-sig")
                return fresh(path) and "module unloaded" in log and not re.search(r"Not all shader parameters were set|Error compiling|Failed to compile|D3D11.*error|\[SpatialFX(?: [0-9.]+)?\].*(?:failed|failure)", log)
            audit.check(folder + ": clean render/shutdown log", logs_ok)
    for folder in UI:
        count = 18 if folder.startswith("persist") else 79
        path = RELEASE / folder / "dock-smoke.json"
        audit.check(f"{folder}: fresh {count}/{count} native checks", lambda path=path, count=count: fresh(path) and passes(read_json(path), count))
        audit.check(folder + ": exact installed path and final hash", lambda folder=folder: installed_path_ok(folder, int(folder[-2:]), expected))
    for folder in GPU:
        path = RELEASE / folder / "render-tests.json"
        def render_ok(path=path):
            value = read_json(path)
            effects = value["effects"]
            return (fresh(path) and passes(value, 196) and len(effects) == 92
                    and len({effect["id"] for effect in effects}) == 92
                    and sum(effect["variants"] for effect in effects) == 237
                    and all(effect["gpuPass"] is True and effect["defaultVisible"] is True for effect in effects))
        audit.check(folder + ": fresh 92 effects / 237 modes / 196 checks", render_ok)
        audit.check(folder + ": completed 30-second performance plateau", lambda folder=folder: performance_ok(RELEASE / folder / "performance.json"))
        path = RELEASE / folder / "coordinates/coordinate-tests.json"
        audit.check(folder + ": fresh eight coordinate checks", lambda path=path: fresh(path) and passes(read_json(path), 8))
    for prefix, (filename, count) in TARGETED.items():
        for major in (32, 33):
            path = RELEASE / (prefix + str(major)) / filename
            audit.check(f"{prefix}{major}: fresh {count}/{count} targeted checks", lambda path=path, count=count: fresh(path) and passes(read_json(path), count))
    def installer_ok():
        path = WORK / "installer/installer-tests.json"
        value = read_json(path)
        return fresh(path) and isinstance(value, list) and len(value) == 18 and all(item.get("pass") is True for item in value)
    audit.check("fresh installer eighteen upgrade/rollback/uninstall checks", installer_ok)
    def guard_ok():
        path = WORK / "install-running-guard.json"
        value = read_json(path)
        return fresh(path) and type(value.get("ExitCode")) is int and value["ExitCode"] == 10 and value.get("HashUnchanged") is True
    audit.check("fresh running OBS guard: exit10 and unchanged DLL", guard_ok)
    preserved_manifest(audit, ROOT / "work/v3.3-ui-20261110/manual-hashes.json", "unchanged manual", True)
    preserved_manifest(audit, ROOT / "work/v3.4-development/historical-hashes.json", "historical artifact")
    audit.check("original selection SHA256 preserved", lambda: sha(ROOT / "docs/v3.4/OBS_Spatial_FX_v3.4_我的选择.json") == SELECTION_SHA)
    audit.check("v3.3 setup SHA256 preserved", lambda: sha(DIST / "OBS_Spatial_FX_V3.3_Setup.exe") == OLD_SETUP_SHA)
    audit.check("original implementation map SHA256 preserved", lambda: sha(ROOT / "docs/v3.4/V34_IMPLEMENTATION_MAP.json") == ORIGINAL_MAP_SHA)
    def baseline_ok():
        baseline = WORK / "baseline/dist"
        value = read_json(baseline / "V34_RELEASE.json")
        return set(value["artifacts"]) >= set(NAMES) and all(sha(baseline / name) == digest.lower() for name, digest in value["artifacts"].items())
    audit.check("previous delivered v3.4 preserved under baseline/dist", baseline_ok)
    base = Path("C:/ProgramData/obs-studio/plugins/obs-spatial-fx")
    for relative in ("obs-spatial-fx.dll", "bin/64bit/obs-spatial-fx.dll", "portable33/obs-spatial-fx/obs-spatial-fx.dll"):
        audit.check("final installed payload SHA256: " + relative, lambda relative=relative: sha(base / relative) == expected)
    audit.check("installed uninstaller equals final setup", lambda: sha(base / "Uninstall.exe") == sha(SETUP))
    audit.check("installed payload has no Qt or QA DLL", lambda: base.is_dir() and not list(base.rglob("Qt*.dll")) and not list(base.rglob("*qa-driver*")))
    for major in (32, 33):
        path = ROOT / f"runtime/obs{major}/config/obs-studio/plugin_config/obs-websocket/config.json"
        audit.check(f"OBS{major} websocket restored byte exactly", lambda path=path, major=major: path.read_bytes() == (WORK / f"websocket{major}.json").read_bytes())
    audit.check("fresh Windows x64 GUI installer", lambda: fresh(SETUP) and windows_installer_ok())
    audit.check("final plugin/setup PDBs exist and are fresh", lambda: all(fresh(ROOT / "build" / name) for name in ("obs-spatial-fx.pdb", "OBS_Spatial_FX_V3.4_Setup.pdb")))
    audit.check("version remains 3.4.0", lambda: "project(obs-spatial-fx VERSION 3.4.0" in (ROOT / "CMakeLists.txt").read_text(encoding="utf-8-sig"))
    def catalog_ok():
        ids = [effect["id"] for effect in read_json(ROOT / "build/fx-runtime-catalog.json")["effects"]]
        return len(ids) == len(set(ids)) == 92 and "FX11.1" not in ids and ids.index("FX12.9") + 1 == ids.index("FX12.6")
    audit.check("catalog stable IDs and gradient order", catalog_ok)
    audit.check("new color/UI report is present and nonempty", lambda: REPORT.is_file() and len(REPORT.read_text(encoding="utf-8-sig").strip()) > 100)
    for major in (32,33):
        path = RELEASE / f"ui{major}/fx-layout.json"
        audit.check(f"OBS{major} compact rows readable", lambda path=path:
                    fresh(path) and read_json(path)["numericHeight"] >= read_json(path)["fontHeight"] and read_json(path)["numericRowPitch"] <= 30
                    and read_json(path)["childPitch"] <= 22 and read_json(path)["childHeight"] >= read_json(path)["childFontHeight"])
    audit.check("AA research present, new AA not claimed as implemented", lambda:
                (ROOT / "docs/v3.4/V34_AA_CLARITY_RESEARCH.md").is_file())
    return audit


PRIVATE_KEYS = {"stagesettings", "scenesettings", "sourcesettings", "settings", "scene", "stage",
                "config", "configuration", "password", "serverpassword", "token", "secret",
                "nativecommand", "nativeanswer", "requestdata", "responsedata"}
PUBLIC_JSON = {"dock-smoke.json", "installed-path.json", "render-tests.json", "performance.json",
               "research-sources.json", "fx-layout.json", "live-colors.json", "coordinate-tests.json", "blur-quality.json", "blur-edgecases.json", "lighting-controls.json",
               "patch-tests.json", "blur-siblings.json", "installer-tests.json", "install-running-guard.json",
               "release-run.json", "package-checks.json", "package-input-hashes.json", "final-package-audit.json"}
PRIVATE_PARTS = {"baseline", "baseline-runtime", "before-bridge", "installed-before", "references", "dumps",
                 "media", "user-media", "__pycache__", "package-staging", "package-transactions"}


def public_json(value):
    if isinstance(value, dict):
        return {key: public_json(item) for key, item in value.items()
                if re.sub(r"[^a-z]", "", key.lower()) not in PRIVATE_KEYS}
    return [public_json(item) for item in value] if isinstance(value, list) else value


def include_evidence(relative):
    path = Path(relative)
    if {part.lower() for part in path.parts} & PRIVATE_PARTS:
        return False
    name = path.name.lower()
    if any(word in name for word in ("control-", "native-command", "native-answer", "websocket", "scene", "config", "fixture", "answer", "dump")):
        return False
    if path.suffix.lower() == ".json":
        return name in PUBLIC_JSON
    if path.suffix.lower() in {".log", ".md", ".diff"}:
        return True
    # Only an explicit patch overview or per-effect synthetic GPU captures.
    return name in {"blur-before-after.png", "lighting-controls.png", "fx-ui-before-after.png", "color-live-preview.png", "fx-expanded.png", "gradient-controls.png", "gaussian-portrait.png", "box-portrait.png"} or bool(re.fullmatch(r"fx\d{2}\.\d[-\w.]*\.png", name))


def evidence_name(path):
    relative = path.relative_to(ROOT)
    try:
        local = path.relative_to(WORK)
    except ValueError:
        return relative.as_posix()
    current = {"release", "installer", "build-final4.log", "build-final5.log", "build-ready.log",
               "ctest-final.log", "install-running-guard.json", "package-checks.json",
               "package-input-hashes.json", "final-package-audit.json"}
    if local.parts[0] in current:
        return relative.as_posix()
    return (WORK.relative_to(ROOT) / "diagnostic-history" / local).as_posix()


def archive(target, files, sanitize=False, name_mapper=None):
    """Audit CRC and exact extracted bytes against the input snapshot."""
    inputs, contents = {}, {}
    for path in sorted(set(files)):
        source_name = path.relative_to(ROOT).as_posix()
        name = name_mapper(path) if name_mapper else source_name
        if name in contents:
            raise RuntimeError("Duplicate ZIP member: " + name)
        data = json_bytes(public_json(read_json(path))) if sanitize and path.suffix.lower() == ".json" else path.read_bytes()
        contents[name] = data
        inputs[name] = {"inputPath": source_name, "inputSha256": sha(path),
                        "archiveSha256": hashlib.sha256(data).hexdigest(), "bytes": len(data)}
    with zipfile.ZipFile(target, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as output:
        for name, data in contents.items():
            output.writestr(name, data)
    with zipfile.ZipFile(target) as output:
        if output.testzip() is not None or set(output.namelist()) != set(contents):
            raise RuntimeError("ZIP CRC/member audit failed: " + str(target))
        for name, data in contents.items():
            if output.read(name) != data:
                raise RuntimeError("ZIP exact-byte audit failed: " + name)
    assert_inputs({item["inputPath"]: item["inputSha256"] for item in inputs.values()})
    return inputs


def source_files():
    files = [ROOT / name for name in ("CMakeLists.txt", "README.md", "BUILDING.md", "LICENSE", ".gitignore") if (ROOT / name).is_file()]
    excluded = {"__pycache__", ".git", "node_modules", ".pytest_cache", "sdk", "sdk-staging"}
    for folder in ("src", "data", "installer", "tests", "tools", "docs"):
        files.extend(path for path in (ROOT / folder).rglob("*") if path.is_file()
                     and not (set(path.relative_to(ROOT / folder).parts) & excluded)
                     and path.suffix.lower() not in {".pyc", ".pyo", ".dmp"})
    return sorted(set(files))


def snapshot_inputs(paths):
    return {path.relative_to(ROOT).as_posix(): sha(path) for path in sorted(set(paths))}


def assert_inputs(snapshot):
    for name, expected in snapshot.items():
        if sha(ROOT / name) != expected:
            raise RuntimeError("Input changed before publication: " + name)


def publish(staging, names, expected_hashes, audit):
    """Atomic replacement per file, rollback on any transaction failure."""
    rollback = staging / "rollback"
    rollback.mkdir()
    existed = {}
    for name in names:
        existed[name] = (DIST / name).exists()
        if existed[name]:
            shutil.copy2(DIST / name, rollback / name)
    replaced = []
    try:
        for name in names:
            if sha(staging / name) != expected_hashes[name]:
                raise RuntimeError("Staged artifact changed: " + name)
            os.replace(staging / name, DIST / name)
            replaced.append(name)
        for name in names:
            if sha(DIST / name) != expected_hashes[name]:
                raise RuntimeError("Published hash mismatch: " + name)
    except BaseException:
        for name in reversed(replaced):
            if existed[name]:
                os.replace(rollback / name, DIST / name)
            else:
                (DIST / name).unlink(missing_ok=True)
        write_json(WORK / "final-package-audit.json", {"phase": "rolled-back", "failed": 1, "checks": audit.checks})
        raise


def package(audit, expected):
    if audit.failed:
        raise RuntimeError("Required gates failed; dist unchanged")
    staging = WORK / "package-transactions" / uuid.uuid4().hex
    staging.mkdir(parents=True)
    sources = source_files()
    symbols = [ROOT / "build/obs-spatial-fx.pdb", ROOT / "build/OBS_Spatial_FX_V3.4_Setup.pdb"]
    generated = {WORK / "package-input-hashes.json", WORK / "final-package-audit.json"}
    evidence = [path for path in WORK.rglob("*") if path.is_file() and path not in generated
                and include_evidence(path.relative_to(WORK))] + [REPORT, ROOT / "docs/v3.4/V34_AA_CLARITY_RESEARCH.md"]
    snapshot = snapshot_inputs(sources + symbols + evidence + [PLUGIN, SETUP])
    for name in NAMES[:2]:
        shutil.copy2(SETUP, staging / name)
    source_manifest = archive(staging / NAMES[2], sources)
    symbol_manifest = archive(staging / NAMES[3], symbols)
    write_json(WORK / "package-input-hashes.json", {"pluginSha256": expected, "inputs": snapshot,
               "sourceArchive": source_manifest, "symbolsArchive": symbol_manifest})
    write_json(WORK / "final-package-audit.json", {"phase": "staged-source-and-symbols-verified", "checks": audit.checks,
               "failed": 0, "pluginSha256": expected, "sourceExactBytes": True, "symbolsExactBytes": True,
               "publication": "Atomic replacement per artifact; rollback on transaction error."})
    evidence = sorted(set(evidence + [WORK / "package-input-hashes.json", WORK / "final-package-audit.json"]))
    evidence_manifest = archive(staging / NAMES[4], evidence, sanitize=True, name_mapper=evidence_name)
    artifacts = {name: sha(staging / name) for name in NAMES}
    manifest = {"version": "3.4.0", "date": "2026-11-11", "patch": "colors-ui-20261111", "pluginSha256": expected,
                "artifacts": artifacts, "groups": 13, "suboptions": 92, "originalSourceSelections": 113,
                "retiredEffectIds": ["FX11.1"], "addedEffectIds": ["FX12.9"], "manualsUnchanged": True, "guiChecks": 194, "liveColorChecksPerOBSVersion": 13,
                "gpuVariantsPerOBSVersion": 237, "gpuChecksPerOBSVersion": 196, "patchChecksPerOBSVersion": 36,
                "coordinateChecksPerOBSVersion": 8, "blurQualityChecksPerOBSVersion": 14, "blurEdgeChecksPerOBSVersion": 10,
                "lightingChecksPerOBSVersion": 17, "blurSiblingChecksPerOBSVersion": 6, "ctestPassed": 8, "installerChecks": 18,
                "evidenceFolders": list(FOLDERS), "implementationMap": "docs/v3.4/V34_DEBUG_IMPLEMENTATION_MAP.json",
                "report": REPORT.relative_to(ROOT).as_posix(), "limitations": LIMITATIONS}
    write_json(staging / "V34_RELEASE.json", manifest)
    names = list(NAMES) + ["V34_RELEASE.json"]
    expected_hashes = {name: sha(staging / name) for name in names}
    sums = {path.relative_to(DIST).as_posix(): sha(path) for path in DIST.rglob("*") if path.is_file() and path.name != "SHA256SUMS.txt"}
    sums.update(expected_hashes)
    (staging / "SHA256SUMS.txt").write_text("".join(digest + "  " + name + "\n" for name, digest in sorted(sums.items())), encoding="utf-8")
    names.append("SHA256SUMS.txt")
    expected_hashes[names[-1]] = sha(staging / names[-1])
    assert_inputs(snapshot)
    final_audit = evaluate_gates(expected)
    if final_audit.failed or sha(PLUGIN) != expected or sha(SETUP) != artifacts[NAMES[0]]:
        raise RuntimeError("Final gate recheck/build drift failed; dist unchanged")
    assert_inputs(snapshot)
    publish(staging, names, expected_hashes, final_audit)
    write_json(WORK / "final-package-audit.json", {"phase": "published-and-hash-verified", "checks": final_audit.checks,
               "failed": 0, "pluginSha256": expected, "artifacts": expected_hashes, "sourceExactBytes": True,
               "symbolsExactBytes": True, "evidenceSanitizedAndExactBytes": True, "evidenceMembers": len(evidence_manifest),
               "transaction": str(staging.relative_to(ROOT))})
    print("Packaged same-version v3.4 color/UI patch; setup SHA256 " + artifacts[NAMES[0]], flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--check", action="store_true", help="evaluate gates only (default)")
    mode.add_argument("--package", action="store_true", help="publish same-named v3.4 artifacts after all gates pass")
    parser.add_argument("--plugin-sha256", default=EXPECTED_PLUGIN, help="explicit approved final build SHA256")
    args = parser.parse_args()
    expected = args.plugin_sha256.lower()
    if not re.fullmatch(r"[0-9a-f]{64}", expected):
        parser.error("--plugin-sha256 requires exactly 64 hexadecimal characters")
    audit = evaluate_gates(expected)
    write_json(WORK / "package-checks.json", {"checks": audit.checks, "failed": audit.failed,
               "pluginSha256": expected, "mode": "package" if args.package else "check-only"})
    if audit.failed:
        print(f"{audit.failed} gate(s) pending/failed; dist unchanged", flush=True)
        return 1
    if args.package:
        package(audit, expected)
    else:
        print("All gates pass; check-only completed, dist unchanged", flush=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, RuntimeError, zipfile.BadZipFile) as error:
        print("Packaging aborted: " + str(error), file=sys.stderr, flush=True)
        sys.exit(1)
