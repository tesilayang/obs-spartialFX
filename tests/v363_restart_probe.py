"""Verify saved v3.6.3 appended composition after normal isolated OBS restart."""
import argparse
import base64
import io
import json
import time
import traceback
from pathlib import Path

from PIL import Image
from obs_client import OBS
from v361_native import UI
from v363_probe import GRAPH_KEYS, graph, image_diff, write_json


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("out", type=Path)
    parser.add_argument("bridge", type=Path)
    parser.add_argument("previous", type=Path, help="first-run verified-presets evidence directory")
    parser.add_argument("--port", type=int, default=4457)
    args = parser.parse_args()
    out, bridge, previous = args.out.resolve(), args.bridge.resolve(), args.previous.resolve()
    out.mkdir(parents=True, exist_ok=True)
    checks, o = [], None

    def check(ok, name, value=None):
        row = dict(name=name, **{"pass": bool(ok)}, status="PASS" if ok else "FAIL", value=value)
        checks.append(row)
        print(row["status"] + " " + name, flush=True)
        write_json(out / "restart-tests.json", dict(checks=checks, failed=sum(not c["pass"] for c in checks)))

    try:
        info = json.loads((previous / "restart-scene.json").read_text(encoding="utf-8"))
        expected = json.loads((previous / "restart-expected.json").read_text(encoding="utf-8"))
        o = OBS(port=args.port)
        check(any(s["sceneName"] == info["scene"] for s in o.call("GetSceneList")["scenes"]), "QA scene retained after normal process restart")
        o.call("SetCurrentProgramScene", dict(sceneName=info["scene"]))
        time.sleep(.6)
        n = UI(bridge, info["stage"])
        n.command("ui-refresh")
        n.command("ui-tab", value=3)
        model = n.get()
        write_json(out / "restart-actual.json", model)
        for key in GRAPH_KEYS:
            check(model.get(key) == expected.get(key), key + " exactly retained across process restart")
        controls = n.inspect("dock-v363-restart.png")
        check("3.6.3" in controls["spatial.version"]["text"], "restarted installed Dock reports v3.6.3")
        check(not controls["spatial.preset-cancel36"]["enabled"], "committed graph returns without a temporary preview")
        check("spatial.preset-append363" in controls, "append control retained after restart")
        check(model["presets_instances"][-1]["seed"] == expected["presets_instances"][-1]["seed"], "appended seed retained without rerandomizing")
        check(int(model["presets_instances"][-1]["start_us"]) == int(info["expectedStartUs"]), "exact appended video-frame boundary retained")
        inputs = {i["inputUuid"] for i in o.call("GetInputList")["inputs"]}
        check(all(l["uuid"] in inputs for l in model["layers"]), "all imported source identities resolve after restart")
        # Seek through the same owned bridge, after checking the untouched disk graph.
        model.pop("_metrics", None)
        model.update(playing=False, timeline_preview=True, timeline_position_us=info["screenshotTimeUs"], play_token=time.time_ns()//1000)
        n.request(model)
        time.sleep(.25)
        result = o.call("GetSourceScreenshot", dict(sourceName=info["stage"], imageFormat="png", imageWidth=960, imageHeight=540))
        raw = base64.b64decode(result["imageData"].split(",", 1)[1])
        (out / "append-final-restarted-frame.png").write_bytes(raw)
        actual = Image.open(io.BytesIO(raw)).convert("RGBA")
        original = Image.open(previous / info["screenshot"]).convert("RGBA")
        difference = image_diff(actual, original)
        check(difference < .05, "same saved clock reproduces actual OBS output after restart", dict(meanRGBADifference=difference))
        check(graph(n.command("ui-inspect")["ui"]["authoritative"]) == graph(expected), "restart screenshot seek preserves saved animation graph")
        n.command("ui-save")
    except Exception:
        check(False, "restart probe exception", traceback.format_exc())
    finally:
        if o:
            o.close()
        write_json(out / "restart-tests.json", dict(checks=checks, failed=sum(not c["pass"] for c in checks)))
    return int(any(not c["pass"] for c in checks))


if __name__ == "__main__":
    raise SystemExit(main())
