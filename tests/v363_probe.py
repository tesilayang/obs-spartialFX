"""Focused v3.6.3 installed OBS 33 acceptance; uses real Qt and stage output.

Run only against the isolated, ready v34-hold QA process.  The WebSocket is
used for fixture sources, screenshots and OBS statistics.  Stage mutations
go through the owned-payload native bridge, never SetInputSettings.
"""
import argparse
import base64
import copy
import hashlib
import io
import json
import math
import platform
import statistics
import subprocess
import time
import traceback
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageStat
from obs_client import OBS
from v361_native import UI


STYLES = ("gentle", "geometry", "pop", "paper", "tech", "rhythm", "dream", "glitch")
GRAPH_KEYS = ("layers", "animation", "presets_geometries", "presets_motion_blocks", "presets_instances", "loop")
FRAME_TIMES = (0, .55, 1.5, 3, 4.5)


def write_json(path, value):
    Path(path).write_text(json.dumps(value, ensure_ascii=False, indent=2), encoding="utf-8")


def image_diff(a, b):
    return sum(ImageStat.Stat(ImageChops.difference(a.convert("RGBA"), b.convert("RGBA"))).mean)


def graph(model):
    return {key: copy.deepcopy(model.get(key, [])) for key in GRAPH_KEYS}


def recipe(model):
    result = model["presets_instances"][-1]["definition_snapshot"]
    return json.loads(result) if isinstance(result, str) else copy.deepcopy(result)


def ref_id(value):
    return value.get("instance_id", "") if isinstance(value, dict) else value


def next_frame_us(end_us, numerator, denominator):
    """Independent integer oracle: first actual OBS frame strictly after tail."""
    if end_us <= 0:
        return 0
    scale = 1_000_000 * denominator
    frame = end_us * numerator // scale + 1
    return (frame * scale + numerator - 1) // numerator


def normalized_candidate(model):
    """Compare resolved candidate while allowing fresh UUIDs and time shift.

    Original source curves outside this candidate's interval are deliberately
    checked separately by existing_animation_preserved, not erased here.
    """
    preset = model["presets_instances"][-1]
    owner = preset["instance_id"]
    start, end = int(preset["start_us"]), int(preset["end_us"])
    geometries = [copy.deepcopy(g) for g in model["presets_geometries"] if ref_id(g.get("owner")) == owner]
    blocks = [copy.deepcopy(b) for b in model["presets_motion_blocks"] if ref_id(b.get("owner")) == owner]
    identities = {owner: "candidate-owner"}
    identities.update({g["instance_id"]: "candidate-geometry-" + str(i) for i, g in enumerate(geometries)})
    identities.update({b["instance_id"]: "candidate-block-" + str(i) for i, b in enumerate(blocks)})

    def identities_only(value):
        if isinstance(value, list):
            return [identities_only(v) for v in value]
        if isinstance(value, dict):
            return {k: identities_only(v) for k, v in value.items()}
        return identities.get(value, value) if isinstance(value, str) else value

    def relative_animation(animation, interval_only=False):
        result = copy.deepcopy(animation)
        for curve in result:
            if interval_only:
                curve["keys"] = [k for k in curve.get("keys", []) if start <= int(k["time_us"]) <= end]
            for key in curve.get("keys", []):
                key["time_us"] = str(int(key["time_us"]) - start)
        return result

    for geometry in geometries:
        geometry["animation"] = relative_animation(geometry.get("animation", []))
        if geometry.get("reveal", 0):
            geometry["reveal_start_us"] = str(int(geometry["reveal_start_us"]) - start)
    for block in blocks:
        for key in ("start_us", "end_us", "time_origin_us"):
            if key in block:
                block[key] = str(int(block[key]) - start)
    tracks = []
    for track in preset.get("generated_tracks", []):
        tracks.append(dict(target=track["target"], parameter=track["parameter"],
                           animation=relative_animation(track["after"].get("animation", []), True)))
    resolved = recipe(model)
    resolved["start_us"] = "0"
    return identities_only(dict(recipe=resolved, geometries=geometries, blocks=blocks,
                                tracks=tracks, duration_us=end-start, seed=preset["seed"]))


def existing_animation_preserved(before, after):
    def objects(model):
        return {"camera": model.get("animation", []), **{l["instance_id"]: l.get("animation", []) for l in model["layers"]}}
    after_objects = objects(after)
    for identity, animation in objects(before).items():
        current = {c["parameter"]: c for c in after_objects.get(identity, [])}
        for old in animation:
            new = current.get(old["parameter"])
            if new is None:
                return False
            if any(new.get(k) != old.get(k) for k in ("enabled", "locked", "shortest")):
                return False
            keys = {int(k["time_us"]): k for k in new.get("keys", [])}
            if any(keys.get(int(k["time_us"])) != k for k in old.get("keys", [])):
                return False
    for kind in ("presets_geometries", "presets_motion_blocks", "presets_instances"):
        current = {v["instance_id"]: v for v in after.get(kind, [])}
        if any(current.get(v["instance_id"]) != v for v in before.get(kind, [])):
            return False
    return True


def coverage(image):
    """Measured output coverage, with RGB fallback for opaque PNG encoders."""
    rgba = image.convert("RGBA")
    alpha = rgba.getchannel("A")
    opaque = alpha.getextrema() == (255, 255)
    mask = rgba.convert("RGB").convert("L") if opaque else alpha
    histogram = mask.histogram()
    pixels = image.width * image.height
    return dict(method="luminance" if opaque else "alpha",
                visibleFraction=sum(histogram[9:])/pixels,
                solidFraction=sum(histogram[102:])/pixels,
                meanAlpha=ImageStat.Stat(alpha).mean[0],
                bbox=list(mask.point(lambda v: 255 if v > 8 else 0).getbbox() or (0, 0, 0, 0)))


def hardware_info(out):
    recorded = out.parent / "hardware.json"
    if recorded.is_file():
        return dict(source=str(recorded), data=json.loads(recorded.read_text(encoding="utf-8-sig")))
    command = "$ErrorActionPreference='Stop'; [pscustomobject]@{CPU=@(Get-CimInstance Win32_Processor|Select-Object Name,NumberOfCores,NumberOfLogicalProcessors);GPU=@(Get-CimInstance Win32_VideoController|Select-Object Name,DriverVersion,AdapterRAM);System=Get-CimInstance Win32_ComputerSystem|Select-Object TotalPhysicalMemory,Model;OS=Get-CimInstance Win32_OperatingSystem|Select-Object Caption,Version,BuildNumber}|ConvertTo-Json -Depth 5 -Compress"
    try:
        result = subprocess.run(["powershell.exe", "-NoProfile", "-NonInteractive", "-Command", command],
                                capture_output=True, text=True, timeout=20, check=True)
        return json.loads(result.stdout.lstrip("\ufeff"))
    except Exception as error:
        return dict(platform=platform.platform(), processor=platform.processor(), collectionError=str(error))


class Probe:
    def __init__(self, args):
        self.args, self.out, self.bridge = args, args.out.resolve(), args.bridge.resolve()
        self.out.mkdir(parents=True, exist_ok=True)
        self.checks, self.workloads, self.style_metrics, self.pictures, self.snapshots = [], [], [], [], {}
        self.o, self.n, self.final = None, None, None
        self.scene, self.stage = "", ""

    def check(self, ok, name, value=None):
        row = dict(name=name, **{"pass": bool(ok)}, status="PASS" if ok else "FAIL", value=value)
        self.checks.append(row)
        print(row["status"] + " " + name + (" " + str(value) if value is not None else ""), flush=True)
        write_json(self.out / "preset-tests.json", self.report())
        return bool(ok)

    def report(self):
        return dict(version="3.6.3", checks=self.checks, failed=sum(not c["pass"] for c in self.checks),
                    workloads=self.workloads, styleMetrics=self.style_metrics, scene=self.scene, stage=self.stage,
                    scope="8 styles sparse/rich; ring/ribbon coverage; append/cancel/undo; save/restart fixture; rich 1080p60 cache")

    def write(self, name, value):
        write_json(self.out / name, value)

    def put(self, model, seconds=0, playing=False):
        value = copy.deepcopy(model)
        value.pop("_metrics", None)
        value.update(playing=playing, timeline_preview=True, timeline_position_us=str(round(seconds*1_000_000)),
                     play_token=time.time_ns() // 1000)
        self.n.request(value)
        time.sleep(.16)

    def refresh(self):
        self.n.command("ui-refresh")
        self.n.command("ui-tab", value=3)
        time.sleep(.12)

    def cancel(self):
        if self.n.inspect().get("spatial.preset-cancel36", {}).get("enabled"):
            self.n.click("spatial.preset-cancel36")

    def reset(self, model=None):
        self.cancel()
        self.put(self.base if model is None else model)
        self.refresh()
        self.n.command("ui-list", widget="spatial.preset-targets361", all=True)

    def controls(self, style="gentle", amount=1, motion="", decor="", start=0, loop=True):
        self.n.click("spatial.preset-style-" + style)
        self.n.command("ui-combo", widget="spatial.preset-amount361", value=amount)
        self.n.combo("spatial.preset-motion-part361", motion)
        self.n.combo("spatial.preset-mask-part361", "none")
        self.n.combo("spatial.preset-decor-part361", decor)
        self.n.number("spatial.preset-period36", 6)
        self.n.number("spatial.preset-cycles36", 1)
        self.n.number("spatial.preset-start36", start)
        self.n.number("spatial.preset-seed361", 424242)
        self.n.number("spatial.preset-strength36", .6)
        self.n.flag("spatial.preset-loop36", loop)
        self.n.flag("spatial.preset-camera36", False)
        self.n.flag("spatial.preset-replace36", False)
        for name in ("motion", "decor", "colors"):
            self.n.flag("spatial.preset-lock-" + name + "361", False)

    def generate(self):
        self.n.click("spatial.preset-generate361")
        time.sleep(.2)
        return self.n.get()

    def shot(self, name, width=960, height=540):
        result = self.o.call("GetSourceScreenshot", dict(sourceName=self.stage, imageFormat="png", imageWidth=width, imageHeight=height))
        raw = base64.b64decode(result["imageData"].split(",", 1)[1])
        (self.out / (name + ".png")).write_bytes(raw)
        return Image.open(io.BytesIO(raw)).convert("RGBA")

    def fixtures(self):
        stamp = str(time.time_ns())
        self.scene, self.stage = "V363 targeted QA " + stamp, "V363 stage " + stamp
        self.o.call("CreateScene", dict(sceneName=self.scene))
        layers = []
        for i, color in enumerate(("#3688c9", "#dd9350", "#b277b5")):
            label = chr(65+i)
            image = Image.new("RGBA", (600, 340), (0, 0, 0, 0))
            draw = ImageDraw.Draw(image)
            draw.rounded_rectangle((8, 8, 592, 332), radius=38, fill=color, outline="white", width=6)
            draw.rectangle((32, 38, 568, 84), fill="#173044")
            draw.text((48, 51), "SPATIAL FX / QA " + label, fill="white")
            for y in range(115, 300, 35):
                draw.line((42, y, 558-(y % 3)*45, y), fill="#ecf5f5", width=9)
            path = self.out / ("fixture-" + label + ".png")
            image.save(path)
            name = "V363 source " + label + " " + stamp
            self.o.call("CreateInput", dict(sceneName=self.scene, inputName=name, inputKind="image_source", inputSettings={"file": str(path)}, sceneItemEnabled=False))
            source = next(s for s in self.o.call("GetInputList")["inputs"] if s["inputName"] == name)
            layers.append(dict(uuid=source["inputUuid"], instance_id="v363-" + label, label=name,
                               x=(i-1)*470, y=(i-1)*120, z=i*130, sx=.85, sy=.85, visible=True,
                               enabled=True, receive_light=True, has_default=True, import_width=600, import_height=340))
        self.base = dict(schema_version=6, width=1920, height=1080, camera_z=-1500, camera_fov=40,
                         fx_enabled=False, light_enabled=False, shadow_enabled=False,
                         playing=False, loop=True, timeline_preview=True,
                         timeline_position_us="0", layers=layers, animation=[], presets_geometries=[],
                         presets_motion_blocks=[], presets_instances=[])
        self.o.call("CreateInput", dict(sceneName=self.scene, inputName=self.stage, inputKind="obs_spatial_fx_stage",
                                        inputSettings=self.base, sceneItemEnabled=True))
        self.o.call("SetCurrentProgramScene", dict(sceneName=self.scene))
        time.sleep(.8)
        self.n = UI(self.bridge, self.stage)
        self.refresh()
        self.base = self.n.get()
        self.base.pop("_metrics", None)

    def styles(self):
        controls = self.n.inspect("dock-v363-initial.png")
        self.write("initial-controls.json", controls)
        self.check("3.6.3" in controls["spatial.version"]["text"], "installed Dock reports v3.6.3")
        self.check(all("spatial.preset-style-" + s in controls for s in STYLES), "all eight style cards exist")
        self.check("spatial.preset-append363" in controls, "append has an independent native button")
        if not controls["spatial.preset-details-toggle361"]["checked"]:
            self.n.click("spatial.preset-details-toggle361")
        for style in STYLES:
            density_rows = []
            for amount, density in ((1, "sparse"), (2, "rich")):
                label = style + "-" + density
                self.reset()
                self.controls(style=style, amount=amount)
                self.n.command("ui-combo", widget="spatial.preset-depth36", value=0)
                model = self.generate()
                self.write(label + "-model.json", model)
                self.check(len(model["presets_instances"]) == 1, label + " produces a preview")
                self.check(graph(self.n.command("ui-inspect")["ui"]["authoritative"]) == graph(self.base), label + " preview leaves saved graph intact")
                resolved = recipe(model)
                decorations = [g for g in model["presets_geometries"] if g["mode"] == 0]
                proxy = sum(g["width"]*g["height"]*g["opacity"] for g in decorations)/(1920*1080)
                self.check(resolved["style"] == style and resolved["decoration_amount"] == amount,
                           label + " persists selected style and amount")
                self.check(2 <= len(decorations) <= 32, label + " has a bounded layered decoration recipe", len(decorations))
                self.snapshots[label] = copy.deepcopy(model)
                self.cancel()
                frames = []
                for j, seconds in enumerate(FRAME_TIMES):
                    self.put(model, seconds)
                    frames.append(self.shot(label + "-stage-" + str(j)))
                variation = max(image_diff(frames[0], f) for f in frames[1:])
                self.check(variation > .01, label + " changes real stage pixels over time", round(variation, 4))
                self.pictures.append((label, frames[2]))
                decoration_only = copy.deepcopy(model)
                for layer in decoration_only["layers"]:
                    layer["visible"] = layer["enabled"] = False
                coverage_frames = []
                for j, seconds in enumerate((0, 1.5, 3)):
                    self.put(decoration_only, seconds)
                    coverage_frames.append(coverage(self.shot(label + "-decor-" + str(j))))
                row = dict(style=style, density=density, geometryCount=len(decorations), weightedAreaProxy=proxy,
                           resolvedRecipe=resolved, renderedCoverage=coverage_frames, frameDifference=variation)
                density_rows.append(row)
                self.style_metrics.append(row)
                self.check(max(v["visibleFraction"] for v in coverage_frames) > .005,
                           label + " has measurable actual decoration coverage", coverage_frames)
                self.check(max(v["solidFraction"] for v in coverage_frames) > .002,
                           label + " includes visible solid color area")
            sparse, rich = density_rows
            self.check(rich["geometryCount"] > sparse["geometryCount"], style + " rich adds composition layers",
                       [sparse["geometryCount"], rich["geometryCount"]])
            self.check(rich["weightedAreaProxy"] > sparse["weightedAreaProxy"]*1.15,
                       style + " rich increases weighted color area", [sparse["weightedAreaProxy"], rich["weightedAreaProxy"]])
        self.write("style-coverage.json", self.style_metrics)

    def thickness(self):
        for part in ("D01", "D05", "D06"):
            self.reset()
            self.controls(amount=2, motion="none", decor=part)
            model = self.generate()
            self.cancel()
            decorations = [g for g in model["presets_geometries"] if g["mode"] == 0]
            if part == "D01":
                candidates = [g for g in decorations if g["shape"] == 3]
                self.check(bool(candidates) and all(g["thickness"] >= 1080*.02 for g in candidates),
                           "D01 ring has substantial stroke width", [g["thickness"] for g in candidates])
            else:
                candidates = [g for g in decorations if g["shape"] == 1]
                self.check(bool(candidates) and all(g["height"] >= 1080*.06 for g in candidates),
                           part + " ribbons have substantial filled height", [g["height"] for g in candidates])
            self.put(model, 1.5)
            image = self.shot(part + "-thick-solid-stage")
            self.pictures.append((part + " thick", image))
            self.write(part + "-thickness.json", dict(geometries=decorations, screenshot=part + "-thick-solid-stage.png"))

    def append_fixture(self):
        def curve(parameter, values):
            return dict(parameter=parameter, enabled=True, locked=False, shortest=False,
                        keys=[dict(time_us=str(t), value=v, mode=0) for t, v in values])
        model = copy.deepcopy(self.base)
        model["loop"] = False
        model["animation"] = [curve("x", ((0, 0), (4_012_345, 35)))]
        model["layers"][0]["animation"] = [curve("x", ((0, -470), (2_300_007, -455))),
                                           curve("opacity", ((0, 1), (1_100_003, .8)))]
        model["layers"][1]["animation"] = [curve("rz", ((0, 0), (3_200_009, 8)))]
        model["presets_geometries"] = [dict(instance_id="v363-existing-geometry", label="existing animation fixture",
                                             shape=0, mode=0, width=130, height=60, x=540, y=-300, z=80,
                                             opacity=.5, color_r=.25, color_g=.65, color_b=.8,
                                             enabled=True, visible=True, animation=[curve("rz", ((0, -5), (3_700_011, 5)))])]
        model["presets_motion_blocks"] = [dict(instance_id="v363-existing-block", label="retained prior motion",
                                                target={"instance_id": "v363-existing-geometry"},
                                                start_us="500000", end_us="3900005", period_us="1000000",
                                                time_origin_us="500000", enabled=True, channels=[dict(parameter="y", amplitude=5, phase=0)])]
        return model

    def append(self):
        self.reset(self.append_fixture())
        baseline = self.n.get()
        self.write("append-baseline.json", baseline)
        self.put(baseline, 1.5)
        baseline_pixels = self.shot("append-existing-before")
        expected_start = next_frame_us(4_012_345, self.video["fpsNumerator"], self.video["fpsDenominator"])
        self.controls(style="paper", amount=2, motion="M17", start=1.2, loop=False)
        self.n.command("ui-combo", widget="spatial.preset-depth36", value=3)
        self.n.number("spatial.preset-z36", -100)
        normal = self.generate()
        self.shot("append-normal-active-preview")
        controls = self.n.inspect("append-normal-preview-controls.png")
        self.check(controls["spatial.preset-apply36"]["enabled"] and controls["spatial.preset-append363"]["enabled"],
                   "normal preview enables both apply and append")
        self.check(int(normal["presets_instances"][-1]["start_us"]) == 1_200_000, "normal candidate previews at chosen start")
        self.check(graph(self.n.command("ui-inspect")["ui"]["authoritative"]) == graph(baseline),
                   "normal preview does not commit before append")
        signature = normalized_candidate(normal)
        self.write("append-normal-preview.json", normal)
        self.n.click("spatial.preset-append363")
        appended = self.n.get()
        self.write("append-normal-committed.json", appended)
        self.check(int(appended["presets_instances"][-1]["start_us"]) == expected_start,
                   "append starts on first actual OBS frame after all retained animation", dict(tailUs=4_012_345, expectedUs=expected_start, actualUs=appended["presets_instances"][-1]["start_us"], video=self.video))
        self.check(normalized_candidate(appended) == signature, "append preserves the exact resolved preview candidate while shifting time")
        self.check(existing_animation_preserved(baseline, appended), "append preserves existing camera layer geometry and motion keys")
        self.check(appended["loop"] == baseline["loop"], "append retains the existing timeline loop setting")
        self.n.command("ui-tab", value=0)
        self.n.click("spatial.undo32")
        undone = self.n.get()
        self.write("append-after-undo.json", undone)
        self.check(graph(undone) == graph(baseline), "one undo restores entire graph before append")
        self.n.click("spatial.redo32")
        self.check(graph(self.n.get()) == graph(appended), "one redo restores entire appended graph")
        self.put(appended, 1.5)
        retained_pixels = self.shot("append-existing-after-normal")
        self.check(image_diff(baseline_pixels, retained_pixels) < .05,
                   "normal append leaves actual earlier stage pixels unchanged",
                   image_diff(baseline_pixels, retained_pixels))
        self.reset(baseline)
        self.controls(style="geometry", amount=2, motion="M04", start=0, loop=True)
        self.n.command("ui-combo", widget="spatial.preset-depth36", value=3)
        tail_preview = self.generate()
        self.shot("append-conflict-active-tail-preview")
        self.write("append-conflict-tail-preview.json", tail_preview)
        controls = self.n.inspect("append-conflict-tail-controls.png")
        self.check(not controls["spatial.preset-apply36"]["enabled"] and controls["spatial.preset-append363"]["enabled"],
                   "conflicting candidate offers append-only tail preview", controls["spatial.preset-status36"].get("text"))
        self.check(int(tail_preview["presets_instances"][-1]["start_us"]) == expected_start, "tail preview uses the same exact next OBS frame")
        self.check(graph(self.n.command("ui-inspect")["ui"]["authoritative"]) == graph(baseline), "tail preview leaves saved original animation intact")
        self.cancel()
        canceled = self.n.get()
        self.write("append-after-cancel.json", canceled)
        self.check(graph(canceled) == graph(baseline), "cancel tail preview restores exact original graph")
        tail_preview = self.generate()
        self.n.click("spatial.preset-append363")
        final = self.n.get()
        self.check(normalized_candidate(final) == normalized_candidate(tail_preview), "tail append commits the candidate already previewed")
        self.check(existing_animation_preserved(baseline, final), "tail append preserves all existing animations and geometry")
        self.check(final["loop"] == baseline["loop"], "tail append preserves original nonlooping timeline")
        self.final = copy.deepcopy(final)
        self.put(self.final, 1.5)
        retained_pixels = self.shot("append-existing-after-tail")
        self.check(image_diff(baseline_pixels, retained_pixels) < .05,
                   "tail append leaves actual earlier stage pixels unchanged",
                   image_diff(baseline_pixels, retained_pixels))
        self.put(self.final, expected_start/1_000_000 + 1.5)
        self.refresh()
        self.shot("append-final-saved-frame")
        self.n.command("ui-save")
        saved = self.n.command("ui-inspect")["ui"]["authoritative"]
        self.check(graph(saved) == graph(final), "OBS save retains committed appended graph")
        self.write("restart-expected.json", saved)
        self.write("restart-scene.json", dict(scene=self.scene, stage=self.stage, screenshot="append-final-saved-frame.png",
                                               screenshotTimeUs=str(expected_start+1_500_000), expectedStartUs=str(expected_start)))

    def performance(self):
        self.check(self.video["baseWidth"] == 1920 and self.video["baseHeight"] == 1080 and
                   self.video["fpsNumerator"] == 60*self.video["fpsDenominator"], "actual OBS workload is 1920x1080 at 60fps", self.video)
        for name, state in (("baseline", self.base), ("gentle-rich", self.snapshots["gentle-rich"]),
                            ("rhythm-rich", self.snapshots["rhythm-rich"])):
            self.put(state, 0, True)
            time.sleep(2)
            warm = self.n.get().get("_metrics", {})
            before = self.o.call("GetStats")
            start = time.monotonic()
            samples = []
            for i in range(self.args.perf_seconds):
                time.sleep(1)
                samples.append(self.o.call("GetStats"))
            end_metrics = self.n.get().get("_metrics", {})
            row = dict(name=name, elapsedSeconds=time.monotonic()-start, samples=samples,
                       meanCPU=statistics.mean(s["cpuUsage"] for s in samples),
                       meanFPS=statistics.mean(s["activeFps"] for s in samples),
                       meanRenderMs=statistics.mean(s["averageFrameRenderTime"] for s in samples),
                       addedSkippedFrames=samples[-1]["renderSkippedFrames"]-before["renderSkippedFrames"],
                       addedTotalFrames=samples[-1]["renderTotalFrames"]-before["renderTotalFrames"],
                       memoryDeltaMiB=samples[-1]["memoryUsage"]-before["memoryUsage"],
                       geometryCount=len(state["presets_geometries"]), layerCount=len(state["layers"]),
                       effects="procedural preset decorations and source animation; FX off; stage lighting off",
                       warm=warm, after=end_metrics)
            self.workloads.append(row)
            self.check(row["elapsedSeconds"] >= 30 and row["meanFPS"] > 59 and row["meanRenderMs"] < 1000/60 and row["addedSkippedFrames"] == 0,
                       name + " sustained actual 1080p60 workload", {k: row[k] for k in ("elapsedSeconds", "meanFPS", "meanRenderMs", "meanCPU", "addedSkippedFrames", "memoryDeltaMiB")})
            stable = bool(warm) and warm.get("allocations") == end_metrics.get("allocations") and end_metrics.get("failures") == warm.get("failures") == 0 and end_metrics.get("renders", 0) > warm.get("renders", 0)
            self.check(stable, name + " GPU cache stays stable after warmup", dict(warm=warm, after=end_metrics))

    def contact_sheet(self):
        if not self.pictures:
            return
        sheet = Image.new("RGB", (4*480, math.ceil(len(self.pictures)/4)*300), "#1d2430")
        draw = ImageDraw.Draw(sheet)
        for i, (label, picture) in enumerate(self.pictures):
            x, y = (i % 4)*480, (i // 4)*300
            sheet.paste(picture.convert("RGB").resize((480, 270)), (x, y))
            draw.text((x+8, y+277), label, fill="white")
        sheet.save(self.out / "styles-sparse-rich-contact-sheet.jpg", quality=94)

    def run(self):
        try:
            self.o = OBS(port=self.args.port)
            self.video = self.o.call("GetVideoSettings")
            version = self.o.call("GetVersion")
            self.write("runtime-info.json", dict(OBSversion=version["obsVersion"], version=version, video=self.video,
                                                  hardware=hardware_info(self.out), utc=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                                                  probeSHA256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                                                  nativeBridge=str(self.bridge), webSocketPort=self.args.port))
            self.check(version["obsVersion"] == "33.0.1", "actual installed acceptance runtime is OBS 33.0.1", version)
            self.fixtures()
            for name, action in (("styles", self.styles), ("ring/ribbon", self.thickness), ("append", self.append), ("performance", self.performance)):
                try:
                    action()
                except Exception:
                    self.check(False, name + " runtime probe exception", traceback.format_exc())
        except Exception:
            self.check(False, "OBS/native bridge setup exception", traceback.format_exc())
        finally:
            if self.n:
                try:
                    self.cancel()
                    if self.final:
                        info = json.loads((self.out / "restart-scene.json").read_text(encoding="utf-8"))
                        self.put(self.final, int(info["screenshotTimeUs"])/1_000_000)
                        self.refresh()
                        self.n.command("ui-save")
                except Exception:
                    self.check(False, "restore final saved append fixture", traceback.format_exc())
            self.contact_sheet()
            self.write("preset-tests.json", self.report())
            if self.o:
                self.o.close()
        return int(any(not c["pass"] for c in self.checks))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("out", type=Path, help="evidence output; normally work/v3.6.3/verified-presets")
    parser.add_argument("bridge", type=Path, help="SPATIAL_QA_OUTPUT of the ready isolated v34-hold process")
    parser.add_argument("--port", type=int, default=4457)
    parser.add_argument("--perf-seconds", type=int, default=30)
    args = parser.parse_args()
    if args.perf_seconds < 30:
        parser.error("performance acceptance requires at least 30 seconds per workload")
    return Probe(args).run()


if __name__ == "__main__":
    raise SystemExit(main())
