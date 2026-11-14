"""Build the frozen offline v3.6.3 manual, using only Python's standard library.

Run from any directory: python docs/v3.6.3/manual/build_manual.py
Screenshots and page-base.html are checked in. To refresh the base prose layout,
use the external Page renderer command documented in MAINTAINING.md first.
No production source, binary or installer is modified by this script.
"""
from pathlib import Path
import base64
import hashlib
import html
import json
import re
import shutil
import sys

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
WORK = ROOT / 'work/publish-v363'
SHOTS = HERE / 'screenshots'
EVIDENCE = ROOT / 'work/v3.6.3/release'
ESC = html.escape


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class AggregateParser:
    """Parse data-only C++ aggregates, never execute C++ or eval source text."""
    def __init__(self, source):
        source = re.sub(r'//[^\n]*', '', source)
        source = re.sub(r'\bFxParameter\s*(?=\{)', '', source)
        self.tokens = re.findall(r'"(?:[^"\\]|\\.)*"|[{} ,]|-?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?|[A-Za-z_][\w:]*', source)
        self.tokens = [t for t in self.tokens if t.strip() and t != ',']
        self.i = 0

    def value(self):
        tok = self.tokens[self.i]
        self.i += 1
        if tok == '{':
            result = []
            while self.tokens[self.i] != '}':
                result.append(self.value())
            self.i += 1
            return result
        if tok.startswith('"'):
            return json.loads(tok)
        if tok in ('true', 'false'):
            return tok == 'true'
        if re.match(r'^-?(?:\d|\.)', tok):
            return float(tok)
        return tok


def catalogue():
    groups_source = (ROOT / 'src/fx_catalog.cpp').read_text(encoding='utf-8')
    groups = re.findall(r'\{"([^"\n]+)","([^"\n]+)"\}', groups_source)[:13]
    assert len(groups) == 13
    defs = []
    for suffix in 'abc':
        source = (ROOT / f'src/fx_catalog_{suffix}.inc').read_text(encoding='utf-8')
        for block in source.split('add(FxDefinition')[1:]:
            data = AggregateParser(block.split(');', 1)[0]).value()
            identity, name, help_text, group, shader, params, *optional = data
            defaults = ['FxAsset::None', False, False, False, False, False]
            defaults[:len(optional)] = optional
            asset, color, color2, history, geometry, stage = defaults
            parameters = [dict(zip(['key', 'label', 'help', 'initial', 'minimum', 'maximum', 'choices'], p)) for p in params]
            defs.append(dict(id=identity, name=name, help=help_text, group=int(group), shader=int(shader),
                             parameters=parameters, asset=asset.split('::')[-1], color=color, color2=color2,
                             history=history, geometry=geometry, stage=stage))
    assert len(defs) == 95 and len({d['id'] for d in defs}) == 95
    runtime_file = HERE / 'runtime-catalog.json'
    # On the original author machine, compare every default/range against the real runtime export.
    checked = False
    if runtime_file.exists():
        rt = json.loads(runtime_file.read_text(encoding='utf-8'))
        if isinstance(rt, dict):
            rt = next(v for v in rt.values() if isinstance(v, list) and v and isinstance(v[0], dict) and 'id' in v[0])
        byid = {d['id']: d for d in rt}
        assert set(byid) == {d['id'] for d in defs}
        for d in defs:
            actual = byid[d['id']]
            assert d['name'] == actual['name'] and d['group'] == actual['group']
            assert len(d['parameters']) == len(actual['parameters'])
            for p in d['parameters']:
                limits = actual['parameter_limits'][p['key']]
                assert (p['initial'], p['minimum'], p['maximum']) == (limits['initial'], limits['min'], limits['max'])
        checked = True
    result = dict(version='3.6.3', groups=[dict(number=i+1, name=n, help=h) for i, (n, h) in enumerate(groups)], effects=defs)
    (HERE / 'catalog.json').write_text(json.dumps(result, ensure_ascii=False, indent=2)+'\n', encoding='utf-8', newline='\n')
    return result, checked


def collect_screenshots(catalog):
    # Historical examples stay explicitly attributed. Current patch outputs replace
    # the effects whose visible behaviour changed; no runtime is claimed here.
    old = ROOT/'docs/v3.4.1/manual'
    historical=json.loads((old/'SCREENSHOT_PROVENANCE.json').read_text(encoding='utf-8'))['screenshots']
    SHOTS.mkdir(exist_ok=True)
    rows={}
    for row in historical:
        name=Path(row['file']).name
        if not (SHOTS/name).exists(): shutil.copy2(old/row['file'], SHOTS/name)
        rows[name]={**row, 'version':'3.4.1'}
    current={
        'overview.png':'v3.6.3-shadow-isolation/final-bridge/obs-main.png',
        'import-timeline.png':'v3.6.3/verified-bridge/dock-tab-0.png',
        'lights.png':'v3.6.3-shadow-isolation/final-bridge/dock-tab-1.png',
        'fx-layout.png':'v3.6.3-shadow-isolation/final-bridge/shadow-panel.png',
        'presets.png':'v3.6.3/verified-bridge/dock-tab-3.png',
        'project.png':'v3.6.3-shadow-isolation/final-bridge/dock-tab-4.png',
        'append-controls.png':'v3.6.3/verified-bridge/append-conflict-tail-controls.png',
        'FX04.0.png':'v3.6.3-shadow-isolation/final-optics/glow-0-red.png',
        'FX04.1.png':'v3.6.3-shadow-isolation/final-optics/star-red-target-1.png',
        'FX05.5.png':'v3.6.3-shadow-isolation/final-isolation/shadow-0-rim-on.png',
        'glow-other.png':'v3.6.3-shadow-isolation/final-optics/glow-1-green.png',
        'star-other.png':'v3.6.3-shadow-isolation/final-optics/star-blue-target-3.png',
        'surface-rim.png':'v3.6.3-shadow-isolation/final-isolation/surface-rim.png',
        'shadow-rim-off.png':'v3.6.3-shadow-isolation/final-isolation/shadow-0-rim-off.png',
        'shadow-rim-on.png':'v3.6.3-shadow-isolation/final-isolation/shadow-0-rim-on.png',
        'decor-rings.png':'v3.6.3/verified-presets/D01-thick-solid-stage.png',
        'decor-ribbons.png':'v3.6.3/verified-presets/D06-thick-solid-stage.png',
    }
    for name,relative in current.items():
        original=ROOT/'work'/relative
        if not original.exists():
            # Rebuilding from a clone uses the frozen checked-in screenshot.
            if not (SHOTS/name).exists(): raise FileNotFoundError(original)
        else: shutil.copy2(original,SHOTS/name)
        rows[name]=dict(file='screenshots/'+name,source='work/'+relative,
                       version='3.6.3',origin='OBS 33.0.1 / retained v3.6.3 evidence',
                       type='actual Qt UI' if 'bridge/' in relative else 'actual GPU output on synthetic material',altered=False)
    for name,row in rows.items():
        row.update(sha256=sha(SHOTS/name),bytes=(SHOTS/name).stat().st_size)
    result=list(rows.values())
    (HERE/'SCREENSHOT_PROVENANCE.json').write_text(json.dumps(dict(version='3.6.3',note='Retained real evidence; historical examples labelled by version. HTML illustrations are not runtime tests.',screenshots=result),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return result

def preset_catalog_html():
    source=(ROOT/'src/presets/composer.cpp').read_text(encoding='utf-8')
    out=''
    for func,title,expected in [('motion','素材运动',18),('mask','露出遮罩',22),('decoration','可见装饰',21)]:
        block=source.split(func+'Catalog()',1)[1].split('return v;',1)[0]
        rows=re.findall(r'\{"([MTD][0-9]+)","([^"\n]+)","([^"\n]+)"',block)
        assert len(rows)==expected,(func,len(rows))
        out+='<h3>'+title+' · '+str(len(rows))+' 种</h3>'+table(['编号','名称','效果'],rows)
    return out


def image(name, caption, cls=''):
    raw = base64.b64encode((SHOTS/name).read_bytes()).decode('ascii')
    version = json.loads((HERE/'SCREENSHOT_PROVENANCE.json').read_text(encoding='utf-8'))['screenshots']
    version = next(r['version'] for r in version if r['file']=='screenshots/'+name)
    return f'<figure class="manual-shot {cls}"><button type="button" class="zoom-shot" aria-label="放大：{ESC(caption)}"><img src="data:image/png;base64,{raw}" alt="{ESC(caption)}" loading="lazy"></button><figcaption>真实截图 · v{version} · {ESC(caption)}</figcaption></figure>'


def table(headers, rows):
    return '<div class="manual-table"><table><thead><tr>'+''.join('<th>'+ESC(x)+'</th>' for x in headers)+'</tr></thead><tbody>'+''.join('<tr>'+''.join('<td>'+ESC(str(x))+'</td>' for x in row)+'</tr>' for row in rows)+'</tbody></table></div>'


def effect_html(d):
    extra = []
    if d['color']: extra.append('光效颜色：初始白色。' if d['id'] in ('FX04.0','FX04.1') else '效果颜色：初始 RGB=(1, 0.85, 0.6)。')
    if d['color2']: extra.append('目标颜色：选择可产生星芒的原图颜色，初始白色。' if d['id']=='FX04.1' else ('阴影颜色：唯一阴影色，初始 RGB=(0.1, 0.2, 0.4)。' if d['id']=='FX05.5' else ('辅助颜色：初始白色。' if d['id']=='FX04.0' else '第二颜色：供渐变/双色等用途。初始 RGB=(0.1, 0.2, 0.4)。')))
    if d['asset'] == 'Source': extra.append('需要辅助素材：从当前时间轴选择实例，读取原始画面。')
    if d['asset'] == 'Image': extra.append('可选择辅助素材或加载本地图片纹理；外部文件不随预设自动打包。')
    if d['asset'] == 'Lut': extra.append('需要加载有效 .cube 或 PNG LUT，具体格式由文件读取器校验。')
    if d['history']: extra.append('使用前一输出历史帧；第一帧没有历史时保留当前画面。')
    if d['geometry']: extra.append('在统一空间求值中作用于位姿/透明度，不是把原素材文件改写。')
    if d['stage']: extra.append('作用于最终舞台画布；上下黑边/透明裁切影响舞台，不改变 OBS 输出分辨率。')
    extra.append('共同混合强度：0～1，初始 1；大组与子功能默认关闭。')
    rows = []
    for p in d['parameters']:
        help_text = re.sub(r'\[shape=([^\]]+)\]', lambda m: '（仅模式索引 '+m[1]+' 使用；模式从 0 起算）', p['help'])
        choices = '；'.join(f'{i}={v}' for i, v in enumerate(p['choices']))
        default = f'{p["initial"]:g}'
        if p['choices']:
            default += '（'+p['choices'][int(p['initial'])]+'）'
        rows.append([p['label']+' / '+p['key'], default, f'{p["minimum"]:g} ～ {p["maximum"]:g}', help_text+(' 可选：'+choices if choices else '')])
    terms = ESC(' '.join([d['name'], d['help'], d['id']] + [p['label']+' '+p['help']+' '+' '.join(p['choices']) for p in d['parameters']]))
    return f'''<details class="fx-entry" data-group="{d['group']}" data-search="{terms}" id="{d['id'].replace('.', '-')}">
      <summary><span class="fx-id">{d['id']}</span><strong>{ESC(d['name'])}</strong><span class="fx-count">{len(rows)} 项参数</span></summary>
      <div class="fx-body"><p>{ESC(d['help'])}</p>{image(d['id']+'.png', d['id']+' '+d['name']+'：GPU 示例；动态效果仅为一个时刻，参数表以当前源码为准。')}
      <p class="asset-note">{'<br>'.join(ESC(x) for x in extra)}</p>{table(['参数','默认','范围','用途与模式'],rows)}</div></details>'''


def catalog_html(cat):
    groups = ''.join(f'<option value="{g["number"]}">{g["number"]:02d} · {ESC(g["name"])}</option>' for g in cat['groups'])
    output = f'''<section class="fx-browser" aria-label="FX 功能目录"><div class="fx-searchbar"><label>搜索功能或参数<input id="fx-search" type="search" placeholder="例如：模糊、中心、SMAA" autocomplete="off"></label><label>功能分组<select id="fx-group"><option value="all">全部 13 组</option>{groups}</select></label><button id="fx-expand" type="button">展开筛选结果</button><button id="fx-collapse" type="button">全部折叠</button></div><p id="fx-results" aria-live="polite">95 项功能</p>'''
    for g in cat['groups']:
        output += f'<section class="fx-group" data-group="{g["number"]}"><h3>{g["number"]:02d} · {ESC(g["name"])}</h3><p>{ESC(g["help"])}</p>'
        output += ''.join(effect_html(d) for d in cat['effects'] if d['group'] == g['number'])+'</section>'
    return output+'</section>'


def main():
    cat, runtime_checked = catalogue()
    shots = collect_screenshots(cat)
    base = (HERE/'page-base.html').read_text(encoding='utf-8')
    prose_hash = (HERE/'page-base.sha256').read_text().strip()
    if sha(HERE/'USER_GUIDE.md') != prose_hash:
        raise RuntimeError('USER_GUIDE changed: refresh Page-rendered page-base.html and page-base.sha256 first.')
    widgets = (HERE/'widgets.html').read_text(encoding='utf-8')
    fragments = {}
    for match in re.finditer(r'<!-- WIDGET ([A-Z_]+) -->(.*?)<!-- END WIDGET -->', widgets, re.S):
        fragments[match[1]] = match[2]
    pieces = {
        'QUICKNAV': '<nav class="quick-links" aria-label="常用入口"><a href="#manual-start">从零开始</a><a href="#manual-timeline">时间轴怎么用</a><a href="#manual-fx">找一个 FX</a><a href="#manual-trouble">没效果怎么查</a></nav><p class="offline-note">本文件可离线阅读：截图、样式、动画脚本全部内置。浏览器动画不连接 OBS，不会修改你的场景。</p>',
        'SHOT_OVERVIEW': image('overview.png','OBS 33.0.1 中 v3.6.3 原生 Dock 的位置；这是界面测试场景，不是示范成片。','wide-shot'),
        'SHOT_IMPORT': image('import-timeline.png','导入素材、对象选择与时间轴，截自 v3.6.3 留存测试。','ui-shot'),
        'SHOT_LIGHTS': image('lights.png','灯光与阴影参数。画面为留存 UI 状态，截图范围以外的参数请按正文查找。','ui-shot'),
        'SHOT_FX': image('fx-layout.png','图层卡片、13 组入口和子功能展开。','ui-shot'),
        'SHOT_QUALITY': image('quality-controls.png','v3.6.3 抗锯齿与 RCAS 独立子板块；图中为保存测试值，不是默认值。','ui-shot'),
        'SHOT_PRESETS': image('presets.png','风格卡、目标素材和生成操作；参数为留存测试值。','ui-shot'),
        'CATALOG': catalog_html(cat),
        'PRESET_CATALOG': preset_catalog_html(),
        'SHOT_PROJECT': image('project.png','工程管理四个入口；不包含用户的私人工程。'),
        'FOOTER': '<footer class="manual-footer">OBS Spatial FX · v3.6.3 固定版说明 · 95 项目录 / 13 组<br><a href="https://github.com/tesilayang/obs-spartialFX/releases/tag/v3.6.3">发布与下载</a> · <a href="https://github.com/tesilayang/obs-spartialFX">源码仓库</a><br>离线阅读无需联网；只有主动点击这些链接才会访问 GitHub。</footer>',
    }
    pieces.update(fragments)
    for key, pairs in {
        'QUALITY_COMPARE':[('quality-original.png','抗锯齿测试：原始斜线'),('quality-smaa.png','同一测试素材：SMAA Low'),('quality-fxaa.png','同一测试素材：FXAA'),('quality-rcas.png','同一测试素材：RCAS；不是抗锯齿')],
        'BLUR_COMPARE':[('blur-gaussian.png','高斯卷积'),('blur-box.png','方框卷积'),('blur-lens.png','镜头散景')],
        'OPTICS_COMPARE':[('FX04.0.png','辉光：红色扩散可超出原边界'),('glow-other.png','另一辉光模式，绿色光效'),('FX04.1.png','星芒：红色目标，4条射线'),('star-other.png','星芒：蓝色目标，6条射线')],
        'SHADOW_COMPARE':[('surface-rim.png','原表面轮廓光'),('shadow-rim-off.png','局部外阴影，舞台轮廓光关闭'),('shadow-rim-on.png','局部外阴影加舞台轮廓光；新增影子不生成亮边')],
        'DECOR_COMPARE':[('decor-rings.png','厚圆环与色面'),('decor-ribbons.png','宽交叉色带')],
        'STAGE_COMPARE':[('led-rgb.png','LED 的 RGB 子像素模式'),('21x9-blackbars.png','舞台 21:9 黑边'),('21x9-transparent.png','舞台 21:9 透明裁切；透明需在棋盘或叠加背景上观察')]
    }.items():
        pieces[key] = '<div class="shot-grid">'+''.join(image(n,c) for n,c in pairs)+'</div>'
    for key, value in pieces.items():
        token = '@@'+key+'@@'
        assert token in base, f'Unknown placeholder {key}'
        base = base.replace('<p>'+token+'</p>', value).replace(token, value)
    assert not re.search(r'@@[A-Z_]+@@', base), 'Unfilled manual placeholder'
    # The Page renderer uses external fonts by default. Offline manual uses system fonts.
    base = re.sub(r'<link\b[^>]*(?:fonts\.googleapis|fonts\.gstatic)[^>]*>', '', base)
    base = re.sub(r'@import\s+url\([^)]*\)\s*;', '', base)
    # The generic Page settings offer remote fonts and editable prose. A fixed
    # offline release manual needs neither; keep the local TOC script only.
    base = re.sub(r'<div class="hf-fab">.*?</script>', '', base, flags=re.S)
    base = re.sub(r'<footer><p>归藏 Page.*?</footer>', '', base, flags=re.S)
    base = re.sub(r'<img\b[^>]*class="hero-bg"[^>]*>', '', base)
    base = base.replace('<span>归藏 Page</span>', '<span>OBS Spatial FX</span>')
    base = re.sub(r'\scontenteditable=(?:"[^"]*"|\'[^\']*\')', '', base)
    base = re.sub(r'\scontenteditable(?=\s|>)', '', base)
    css = (HERE/'manual.css').read_text(encoding='utf-8')
    js = (HERE/'manual.js').read_text(encoding='utf-8')
    base = base.replace('</head>', '<style id="manual-style">'+css+'</style></head>')
    base = base.replace('</body>', '<dialog id="shot-dialog"><button type="button" id="shot-close" aria-label="关闭放大图">关闭 ×</button><img alt=""><p></p></dialog><script>'+js+'</script></body>')
    # Stable convenience anchors survive prose changes and renderer heading slug rules.
    for title, anchor in [('03 · 十分钟做出第一个镜头','manual-start'),('06 · 时间轴：先选对象，再选时间','manual-timeline'),('11 · 95 项 FX 与全部参数速查','manual-fx'),('15 · 常见问题：按这张表检查','manual-trouble')]:
        base, count = re.subn(r'(<h2\b[^>]*>)(.*?'+re.escape(title)+r'.*?)(</h2>)', lambda m: f'<span id="{anchor}" class="section-anchor"></span>'+m[0], base, count=1)
        assert count == 1, title
    # No editable article and no Google font requests are acceptable in this fixed document.
    base = base.replace("el.setAttribute('contenteditable', 'true');", "/* Fixed release manual: editing disabled. */")
    base = '\n'.join(line.rstrip() for line in base.splitlines())+'\n'
    (HERE/'index.html').write_text(base,encoding='utf-8',newline='\n')
    stats = dict(version='3.6.3', effects=len(cat['effects']), groups=len(cat['groups']),
                 parameters=sum(len(d['parameters']) for d in cat['effects']), screenshots=len(shots),
                 runtime_defaults_and_ranges_crosschecked=runtime_checked,
                 prose_sha256=sha(HERE/'USER_GUIDE.md'),html_sha256=sha(HERE/'index.html'),
                 bytes=(HERE/'index.html').stat().st_size,
                 animations=['depth parallax','keyframe interpolation and Bezier','bound light movement','FX activation gates','BPM grid','preset selection and append','project destination'],
                 note='Illustrations are not OBS runtime tests. Browser validation is reported separately.')
    (HERE/'BUILD_MANIFEST.json').write_text(json.dumps(stats,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')
    print(json.dumps(stats,ensure_ascii=False,indent=2))


if __name__ == '__main__':
    main()
