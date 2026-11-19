"""Review final raster frames exported directly from OBS and generate an offline comparison sheet."""
from pathlib import Path
import base64,io,json,shutil,sys
from PIL import Image,ImageDraw,ImageStat,ImageChops,ImageFont
from v363_probe import STYLES
w=Path(sys.argv[1]);dest=Path(sys.argv[2]);dest.mkdir(parents=True,exist_ok=True)
names=['温柔舒缓','简洁几何','活泼波普','纸片拼贴','科技界面','节奏快剪','朦胧梦幻','故障实验']; seeds=[17,73,419]; entries={};metrics=[]
def img(path):return Image.open(path).convert('RGB')
def encoded(image):
    data=io.BytesIO();image.save(data,'WEBP',quality=92);return 'data:image/webp;base64,'+base64.b64encode(data.getvalue()).decode()
def inspect(a,b):return round(sum(ImageStat.Stat(ImageChops.difference(a,b)).mean),4)
clean=img(w/'final/clean.png').resize((768,432));font=ImageFont.truetype('C:/Windows/Fonts/msyh.ttc',27);sheet=Image.new('RGB',(1400,8*222+68),'#fcf9f3');d=ImageDraw.Draw(sheet);d.text((18,12),'OBS 33.0.1   ·   左侧 v3.6.5     /     中间 v3.6.6-b 少量     /     右侧 v3.6.6-b 丰富',fill='#223a48',font=font)
for row,(key,title) in enumerate(zip(STYLES,names)):
    data={};entries[title]=data
    for seed in seeds:
        frames={}
        for variant,folder in [('旧版','baseline'),('少量','final'),('丰富','rich')]:
            if variant=='丰富' and seed!=17:continue
            images=[img(w/folder/f'{key}-{seed}-{s}.png') for s in ['entry','mid','exit']];frames[variant]=[encoded(im) for im in images]
            if variant=='少量':
                m=json.loads((w/folder/f'{key}-{seed}.json').read_text(encoding='utf-8-sig'));metrics.append(dict(style=key,seed=seed,midChange=inspect(clean,images[1]),entryToMid=inspect(images[0],images[1]),exitToMid=inspect(images[2],images[1]),entities=len(m['presets_geometries']),motif=m['presets_geometries'][0]['label']))
            if seed==17:
                col=['旧版','少量','丰富'].index(variant);sheet.paste(images[1].resize((380,214)),(245+col*382,68+row*222))
        data[str(seed)]=frames
    d.text((15,142+row*222),title,fill='#253d4f',font=font)
sheet.save(dest/'八种风格实拍对照.jpg',quality=92);(w/'style-metrics.json').write_text(json.dumps(metrics,ensure_ascii=False,indent=2),encoding='utf-8');assert len(metrics)==24 and all(r['midChange']>0 and r['entryToMid']>0 and r['exitToMid']>0 for r in metrics)
html='''<!doctype html><html lang="zh-CN"><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>v3.6.6-b 装饰实拍</title><style>*{box-sizing:border-box}body{margin:0;padding:clamp(16px,4vw,60px);font:16px system-ui;color:#253949;background:#faf8f3}h1{font-size:clamp(24px,4vw,48px);margin:0}p{color:#50616b}select,button{font:inherit;padding:6px;border:1px solid #a8b5b9;border-radius:6px;background:white;color:inherit}label{display:inline-block;margin:8px 16px 8px 0}section{display:grid;grid-template-columns:repeat(auto-fit,minmax(min(100%,350px),1fr));gap:14px}figure{margin:12px 0}img{width:100%;border-radius:6px}figcaption{padding:6px 0}</style><h1>八种风格的装饰构图</h1><p>同一套角色、标题和桌面。画面取自 OBS 33.0.1 的舞台输出，三帧分别位于 0.55、3.00 和 5.48 秒。</p><label>风格 <select id="style"></select></label><label>种子 <select id="seed"><option>17</option><option>73</option><option>419</option></select></label><label>时刻 <select id="phase"><option>入场</option><option selected>中段</option><option>退场</option></select></label><section id="items"></section><script>const data=DATA,clean=CLEAN;const s=document.getElementById('style'),n=document.getElementById('seed'),t=document.getElementById('phase');for(const k of Object.keys(data))s.add(new Option(k,k));s.value='纸片拼贴';function show(){const cards={'原始舞台':clean};for(const [kind,frames] of Object.entries(data[s.value][n.value]))cards[kind==='旧版'?'v3.6.5 · 少量':'v3.6.6-b · '+kind]=frames[t.selectedIndex];items.replaceChildren();for(const [title,src] of Object.entries(cards)){const fig=document.createElement('figure'),c=document.createElement('figcaption'),i=new Image();i.alt=title;i.src=src;c.textContent=title;fig.append(i,c);items.append(fig)}}for(const el of [s,n,t])el.onchange=show;show();</script></html>'''
(dest/'装饰实拍对照.html').write_text(html.replace('DATA',json.dumps(entries,ensure_ascii=False)).replace('CLEAN',json.dumps(encoded(clean))),encoding='utf-8')
