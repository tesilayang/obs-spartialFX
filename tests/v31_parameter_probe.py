"""Real D3D11 pixel comparisons for the three documented layer parameters."""
import base64, json, math, sys, time
from pathlib import Path
from PIL import Image, ImageDraw, ImageChops, ImageStat
from obs_client import OBS
from native_control import LegacyNative as Native

out = Path(sys.argv[1]).resolve()
out.mkdir(parents=True, exist_ok=True)
checks = []

def check(ok, name, value=None):
    checks.append({'name': name, 'pass': bool(ok), 'value': value})
    print(('PASS ' if ok else 'FAIL ') + name + ' ' + str(value), flush=True)

def pattern(path, color):
    im = Image.new('RGBA', (600, 500), color)
    d = ImageDraw.Draw(im)
    for y in range(0, 500, 12):
        for x in range(0, 600, 12):
            if (x // 12 + y // 12) % 2:
                d.rectangle((x, y, x + 5, y + 5), fill=(245, 245, 245, 255))
    d.text((230, 240), 'Spatial FX v3.1 FOCUS', fill=(0, 0, 0, 255))
    im.save(path)

pattern(out / 'pattern-a.png', (170, 40, 30, 255))
pattern(out / 'pattern-b.png', (25, 60, 180, 255))
deadline = time.monotonic() + 20
while True:
    o = None
    try:
        o = OBS()
        o.call('GetSceneList')  # WebSocket can listen before OBS finishes loading.
        break
    except (OSError, RuntimeError):
        if o is not None:
            o.close()
        if time.monotonic() >= deadline:
            raise
        time.sleep(.2)
stamp = str(int(time.time()))
scene = 'Spatial FX V31 parameters ' + stamp
stage = 'Spatial FX V31 Parameter Stage ' + stamp
o.call('CreateScene', {'sceneName': scene})

def source(name, file):
    name += ' ' + stamp
    o.call('CreateInput', {'sceneName': scene, 'inputName': name,
                         'inputKind': 'image_source', 'inputSettings': {'file': str(file)},
                         'sceneItemEnabled': False})
    return next(i['inputUuid'] for i in o.call('GetInputList')['inputs'] if i['inputName'] == name)

a_id = source('V31 Red grid', out / 'pattern-a.png')
b_id = source('V31 Blue grid', out / 'pattern-b.png')
focal = 1080 / (2 * math.tan(math.radians(20)))

def layer(uid, z=400, **extra):
    result = {'uuid': uid, 'label': uid, 'x': 0, 'y': 0, 'z': z,
              'sx': (1500 + z) / focal, 'sy': (1500 + z) / focal,
              'perspective': 1, 'focus_blur': 1, 'depth_order': 0,
              'receive_light': False, 'cast_shadow': False}
    result.update(extra)
    return result

settings = {'schema_version': 3, 'width': 1920, 'height': 1080,
            'fx_enabled': False, 'layers': [layer(a_id)],
            'camera_x': 0, 'camera_y': 0, 'camera_z': -1500, 'camera_fov': 40,
            'camera_rx': 0, 'camera_ry': 0, 'camera_rz': 0,
            'camera_look_at': False, 'timeline_preview': False, 'playing': False,
            'light_enabled': False, 'extra_lights': [], 'focus_distance': 1500,
            'aperture': 0, 'fx_exposure': 0, 'fx_saturation': 1, 'fx_vignette': 0}
o.call('CreateInput', {'sceneName': scene, 'inputName': stage,
                     'inputKind': 'obs_spatial_fx_stage', 'inputSettings': settings,
                     'sceneItemEnabled': True})
o.call('SetCurrentProgramScene', {'sceneName': scene})
control = Native(out, stage)
time.sleep(2)

def update(patch):
    control.update(patch)
    time.sleep(.25)

def shot(name):
    data = o.call('GetSourceScreenshot', {'sourceName': stage, 'imageFormat': 'png',
                                        'imageWidth': 1920, 'imageHeight': 1080})
    path = out / (name + '.png')
    path.write_bytes(base64.b64decode(data['imageData'].split(',', 1)[1]))
    return Image.open(path).convert('RGB')

def diff(a, b):
    return sum(ImageStat.Stat(ImageChops.difference(a, b)).mean)

try:
    # Same-depth order changes pixels; different-depth order cannot override Z.
    update({'layers': [layer(a_id, depth_order=0), layer(b_id, depth_order=10)]})
    a = shot('order-same-depth-blue-front')
    update({'layers': [layer(a_id, depth_order=10), layer(b_id, depth_order=0)]})
    b = shot('order-same-depth-red-front')
    check(diff(a, b) > 1, 'same-depth order changes overlap', diff(a, b))
    check(a.getpixel((960, 540)) != b.getpixel((960, 540)),
          'higher order is painted on top at identical depth')
    update({'layers': [layer(a_id, z=0, depth_order=-1000), layer(b_id, z=600, depth_order=1000)]})
    a = shot('order-different-depth-a')
    update({'layers': [layer(a_id, z=0, depth_order=1000), layer(b_id, z=600, depth_order=-1000)]})
    b = shot('order-different-depth-b')
    check(diff(a, b) == 0, 'different-depth order has no effect', diff(a, b))
    # Frontal neutral view is compensated, lateral motion exposes real parallax.
    update({'layers': [layer(a_id, perspective=0)]})
    a = shot('perspective-neutral-zero')
    update({'layers': [layer(a_id, perspective=1)]})
    b = shot('perspective-neutral-one')
    check(diff(a, b) < .05, 'neutral frontal view preserves initial appearance', diff(a, b))
    update({'camera_x': 300, 'layers': [layer(a_id, perspective=0)]})
    a = shot('perspective-pan-zero')
    update({'layers': [layer(a_id, perspective=1)]})
    b = shot('perspective-pan-one')
    check(diff(a, b) > .5, 'camera pan reveals perspective multiplier while FX is OFF', diff(a, b))
    update({'layers': [layer(a_id, z=0, perspective=0)]})
    a = shot('perspective-z0-zero')
    update({'layers': [layer(a_id, z=0, perspective=2)]})
    b = shot('perspective-z0-two')
    check(diff(a, b) == 0, 'Z zero is unaffected by multiplier', diff(a, b))
    # Confirm all three zero-blur conditions independently.
    update({'camera_x': 0, 'fx_enabled': True, 'aperture': 0, 'focus_distance': 600,
            'layers': [layer(a_id, focus_blur=0)]})
    a = shot('blur-aperture0-layer0')
    update({'layers': [layer(a_id, focus_blur=4)]})
    b = shot('blur-aperture0-layer4')
    check(diff(a, b) == 0, 'default zero aperture suppresses any layer blur multiplier', diff(a, b))
    update({'aperture': 20, 'focus_distance': 1900, 'layers': [layer(a_id, focus_blur=0)]})
    a = shot('blur-in-focus-layer0')
    update({'layers': [layer(a_id, focus_blur=4)]})
    b = shot('blur-in-focus-layer4')
    check(diff(a, b) == 0, 'layer on focus plane stays sharp regardless of multiplier', diff(a, b))
    update({'fx_enabled': False, 'focus_distance': 600, 'layers': [layer(a_id, focus_blur=0)]})
    a = shot('blur-fxoff-layer0')
    update({'layers': [layer(a_id, focus_blur=4)]})
    b = shot('blur-fxoff-layer4')
    check(diff(a, b) == 0, 'FX OFF suppresses blur even with nonzero aperture', diff(a, b))
    update({'fx_enabled': True, 'layers': [layer(a_id, focus_blur=0)]})
    clear = shot('blur-off')
    update({'layers': [layer(a_id, focus_blur=1)]})
    normal = shot('blur-normal')
    update({'layers': [layer(a_id, focus_blur=4)]})
    strong = shot('blur-strong')
    check(diff(clear, normal) > .1, 'nonzero aperture and defocus produce actual visible blur', diff(clear, normal))
    check(diff(normal, strong) > .1, 'layer multiplier changes actual defocus pixels', diff(normal, strong))
    # Crop only real GPU screenshots to make a useful 1:1 comparison artifact.
    pairs = [(clear, 'Layer blur 0'), (normal, 'Layer blur 1'), (strong, 'Layer blur 4')]
    board = Image.new('RGB', (1200, 390), '#202630')
    d = ImageDraw.Draw(board)
    for i, (im, label) in enumerate(pairs):
        board.paste(im.crop((760, 370, 1160, 730)), (i * 400, 30))
        d.text((i * 400 + 12, 10), label, fill='white')
    board.save(out / 'blur-comparison.png')
except Exception as exc:
    check(False, 'parameter probe exception', repr(exc))
finally:
    o.close()
report = {'checks': checks, 'failed': sum(not c['pass'] for c in checks),
          'scene': scene, 'stage': stage, 'capture': 'OBS D3D11 source screenshot, 1920x1080'}
(out / 'parameter-tests.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
raise SystemExit(bool(report['failed']))
