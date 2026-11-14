// Everything runs locally. No storage, telemetry, dependencies, or connection to OBS.
(() => {
 const el=id=>document.getElementById(id);let candidate=false,round=0,raf=0;
 const tell=text=>el('preset-readout').textContent=text;
 el('preset-generate').onclick=()=>{candidate=true;round++;el('preset-block').setAttribute('opacity','.55');tell(`正在预览第 ${round} 套“${el('preset-style').value}”；没有改已有动画。`);cancelAnimationFrame(raf);const start=performance.now();function tick(now){if(!candidate)return;const x=Math.sin((now-start)/650)*55;el('preset-actor').setAttribute('transform',`translate(${x} 0)`);raf=requestAnimationFrame(tick);}raf=requestAnimationFrame(tick);};
 el('preset-append').onclick=()=>{if(!candidate){tell('请先生成候选，再补充。');return;}const tail=Math.max(0,+el('preset-tail').value||0),duration=Math.max(.01,+el('preset-duration').value||6),start=tail===0?0:(Math.floor(tail*60+1e-7)+1)/60;el('preset-block').setAttribute('opacity','1');el('preset-block-label').textContent='已补充同一候选';el('preset-end-label').textContent=`${start.toFixed(6)} 秒`;tell(`以示意 60 fps 计算：同一候选从 ${start.toFixed(6)} 秒开始，到 ${(start+duration).toFixed(6)} 秒结束。原段保留；真实插件采用 OBS 实际帧率。`);};
 el('preset-cancel').onclick=()=>{candidate=false;cancelAnimationFrame(raf);el('preset-actor').removeAttribute('transform');el('preset-block').setAttribute('opacity','.25');el('preset-block-label').textContent='预览候选';tell('未应用候选已取消；示意中的已有段保留。真实插件取消不会撤掉已经应用的段。');};
 function project(){const mode=el('project-mode').value;el('project-readout').textContent=mode==='json'?(el('project-has-b').checked?'按存档名找到 B → 显示覆盖内容 → 二次确认 → 保存恢复 JSON → 恢复 B。当前激活 A 不会被替换。':'没有同名 B → 新建 B；A、C、D 保留。缺本地素材时选择重新链接。'):(mode==='zip'?'选择 ZIP 和持久保存的父目录 → 新建唯一子目录 → 解压与校验 → 按新路径重建 → 新建场景。导入后保留解压目录。':'选择含 project.json 的已解压文件夹 → 校验 → 按文件夹内路径重建 → 新建场景。已有同名场景保留。');}el('project-preview').onclick=project;el('project-mode').onchange=project;el('project-has-b').onchange=project;project();
})();
(() => {
 'use strict';
 const $ = id => document.getElementById(id);
 // Disable the Page renderer's generic editable-article feature for this fixed manual.
 document.querySelectorAll('[contenteditable]').forEach(el => el.removeAttribute('contenteditable'));
 const dialog = $('shot-dialog');
 document.querySelectorAll('.zoom-shot').forEach(button => button.addEventListener('click', () => {
  const img = button.querySelector('img');
  dialog.querySelector('img').src = img.src;
  dialog.querySelector('img').alt = img.alt;
  dialog.querySelector('p').textContent = img.alt;
  dialog.showModal();
 }));
 $('shot-close').onclick = () => dialog.close();
 dialog.addEventListener('click', e => {if (e.target === dialog) dialog.close();});
 const entries = [...document.querySelectorAll('.fx-entry')];
 function filterFX() {
  const q = $('fx-search').value.trim().toLocaleLowerCase(), group = $('fx-group').value;
  let n = 0;
  entries.forEach(entry => {entry.hidden = !((group === 'all' || entry.dataset.group === group) && entry.dataset.search.toLocaleLowerCase().includes(q)); if(!entry.hidden)n++;});
  document.querySelectorAll('.fx-group').forEach(groupEl => {groupEl.hidden = ![...groupEl.querySelectorAll('.fx-entry')].some(e => !e.hidden);});
  $('fx-results').textContent = `找到 ${n} / 95 项功能`;
 }
 $('fx-search').addEventListener('input', filterFX);
 $('fx-group').addEventListener('change', filterFX);
 $('fx-expand').onclick = () => entries.filter(e => !e.hidden).forEach(e => e.open = true);
 $('fx-collapse').onclick = () => entries.forEach(e => e.open = false);
 filterFX();
 function parallax() {
  const x = +$('cam-x').value, same = $('same-depth').checked;
  const depths = same ? [0,0,0] : [700,0,-300];
  ['far-layer','mid-layer','near-layer'].forEach((id,i) => {$(id).setAttribute('transform',`translate(${-x*1500/(1500+depths[i])*.55} 0)`);});
  $('cam-out').textContent = x;
  $('parallax-readout').textContent = same ? '深度相同：三个图层相对平移一致，视差减弱。' : '不同深度：前景移动最多，背景移动最少。中心虚线只是参考线。';
 }
 ['cam-x','same-depth'].forEach(id => $(id).addEventListener('input',parallax));
 $('parallax-reset').onclick = () => {$('cam-x').value = 0;$('same-depth').checked = false;parallax();};
 parallax();
 const clamp = (x,a,b) => Math.max(a,Math.min(b,x));
 const cubic = (u,a,b) => 3*(1-u)*(1-u)*u*a+3*(1-u)*u*u*b+u*u*u;
 function ease(x) {
  switch($('ease-select').value) {
   case 'in':return x*x;
   case 'out':return 1-(1-x)*(1-x);
   case 'inout':return x*x*(3-2*x);
   case 'hold':return x < 1 ? 0 : 1;
   case 'bezier': {
    const a=+$('bez-x1').value/100,b=+$('bez-x2').value/100;
    let lo=0,hi=1;
    for(let i=0;i<30;i++){const u=(lo+hi)/2;if(cubic(u,a,b)<x)lo=u;else hi=u;}
    return cubic((lo+hi)/2,+$('bez-y1').value/100,+$('bez-y2').value/100);
   }
   default:return x;
  }
 }
 let keyPlaying=false,lastKey=0;
 function drawKeys() {
  const bez=$('ease-select').value==='bezier';
  $('bezier-controls').hidden=!bez;
  $('bezier-handles').setAttribute('visibility',bez?'visible':'hidden');
  // SVG hidden is not universally supported; visibility provides explicit behavior.
  if(bez)$('bezier-handles').removeAttribute('hidden');else $('bezier-handles').setAttribute('hidden','');
  const points=[];
  for(let i=0;i<=180;i++){const x=i/180;points.push(`${i?'L':'M'}${70+570*x},${255-165*ease(x)}`);}
  $('key-curve').setAttribute('d',points.join(' '));
  const hx1=70+570*(+$('bez-x1').value/100),hy1=255-165*(+$('bez-y1').value/100);
  const hx2=70+570*(+$('bez-x2').value/100),hy2=255-165*(+$('bez-y2').value/100);
  $('handle-lines').setAttribute('d',`M70 255L${hx1} ${hy1}M640 90L${hx2} ${hy2}`);
  [['handle-one',hx1,hy1],['handle-two',hx2,hy2]].forEach(([id,x,y])=>{$(id).setAttribute('cx',x);$(id).setAttribute('cy',y);});
  const t=+$('key-time').value,x=t/3,y=ease(x);
  $('time-line').setAttribute('d',`M${70+570*x} 35V255`);
  $('curve-dot').setAttribute('cx',70+570*x);$('curve-dot').setAttribute('cy',255-165*y);
  $('moving-object').setAttribute('x',65+540*y);
  $('key-time-label').textContent=t.toFixed(2);
  $('key-readout').textContent=`${t.toFixed(2)} 秒 → X=${(100*y).toFixed(2)}。播放长度始终为 3 秒，曲线改变的是途中位置。`;
 }
 function stopKeys(){keyPlaying=false;$('key-play').textContent='播放示意';}
 $('key-play').onclick=()=>{keyPlaying=!keyPlaying;if(keyPlaying&&+$('key-time').value>=3)$('key-time').value=0;lastKey=performance.now();$('key-play').textContent=keyPlaying?'暂停示意':'播放示意';drawKeys();};
 $('key-reset').onclick=()=>{stopKeys();$('key-time').value=0;drawKeys();};
 $('key-time').addEventListener('input',()=>{stopKeys();drawKeys();});
 ['ease-select','bez-x1','bez-x2','bez-y1','bez-y2'].forEach(id=>$(id).addEventListener('input',()=>{
  if(id==='bez-x1'&&+$('bez-x1').value>+$('bez-x2').value)$('bez-x2').value=$('bez-x1').value;
  if(id==='bez-x2'&&+$('bez-x2').value<+$('bez-x1').value)$('bez-x1').value=$('bez-x2').value;
  drawKeys();
 }));
 drawKeys();
 let lightPlaying=false,lightPhase=0,lastLight=0;
 function drawLight(){
  const x=+$('light-layer-x').value,offset=+$('light-offset').value,bound=$('light-bound').checked;
  const lx=415-65+offset+(bound?x:0);
  $('bound-layer').setAttribute('transform',`translate(${x} 0)`);
  $('light-marker').setAttribute('transform',`translate(${lx-415} 0)`);
  $('light-ray').setAttribute('d',`M${lx} 85L${350+x} 190`);
  $('light-shadow').setAttribute('cx',clamp(350+x+(350+x-lx)*.38,60,640));
  $('light-readout').textContent=bound?`绑定：图层 X=${x}，灯 X=图层 X + 偏移 ${offset}=${x+offset}（示意单位）。`:`未绑定：图层在动，灯保持世界 X=${offset}。`;
 }
 ['light-layer-x','light-offset','light-bound'].forEach(id=>$(id).addEventListener('input',drawLight));
 $('light-play').onclick=()=>{lightPlaying=!lightPlaying;lastLight=performance.now();$('light-play').textContent=lightPlaying?'暂停跟随示意':'播放跟随示意';};drawLight();
 function drawGate(){
  const group=$('gate-group').checked,child=$('gate-child').checked,strength=+$('gate-strength').value,on=group&&child&&strength>0;
  const a=[194,208,235],b=[35,82,249];
  $('gate-swatch').style.backgroundColor=`rgb(${a.map((v,i)=>Math.round(v+(b[i]-v)*(on?strength:0))).join(',')})`;
  $('gate-swatch').style.color=on&&strength>.55?'white':'#13213a';
  $('gate-swatch').textContent=on?'处理示意':'原素材';
  $('gate-status').textContent=on?`已启用，混合强度 ${strength.toFixed(2)}`:!group?'大组关闭，未处理':!child?'子功能未勾选，未处理':'强度为 0，保留原画';
 }
 ['gate-group','gate-child','gate-strength'].forEach(id=>$(id).addEventListener('input',drawGate));drawGate();
 function drawBeats(){
  const bpm=clamp(+$('demo-bpm').value||120,30,300),ratio=+$('demo-ratio').value,min=clamp(+$('demo-minimum').value||0,0,4),offset=clamp(+$('demo-offset').value||0,0,2),interval=60/bpm*ratio;
  const ns='http://www.w3.org/2000/svg',lines=$('beat-lines');lines.replaceChildren();
  let last=-Infinity,count=0,total=0;
  for(let t=offset;t<=4+1e-7;t+=interval){
   const keep=t-last>=min-1e-7;if(keep){last=t;count++;}total++;
   const line=document.createElementNS(ns,'line');line.setAttribute('x1',50+t*150);line.setAttribute('x2',50+t*150);line.setAttribute('y1',keep?'30':'65');line.setAttribute('y2','115');line.setAttribute('stroke',keep?'#1645f5':'#afc3ff');line.setAttribute('stroke-width',keep?'3':'1');lines.appendChild(line);
  }
  $('beat-readout').textContent=`一拍 ${(60/bpm).toFixed(3)} 秒；比例间隔 ${interval.toFixed(3)} 秒。0～4 秒共 ${total} 个候选，保留 ${count} 个；这里只画参考线，不生成运动。`;
 }
 ['demo-bpm','demo-ratio','demo-minimum','demo-offset'].forEach(id=>$(id).addEventListener('input',drawBeats));drawBeats();
 function frame(now){
  if(keyPlaying){const t=Math.min(3,+$('key-time').value+Math.min(.1,(now-lastKey)/1000));$('key-time').value=t.toFixed(2);drawKeys();if(t>=3)stopKeys();}
  if(lightPlaying){lightPhase+=Math.min(.1,(now-lastLight)/1000);$('light-layer-x').value=Math.round(Math.sin(lightPhase)*170);drawLight();}
  lastKey=lastLight=now;requestAnimationFrame(frame);
 }
 requestAnimationFrame(frame);
 document.addEventListener('visibilitychange',()=>{if(document.hidden){stopKeys();lightPlaying=false;$('light-play').textContent='播放跟随示意';}});
 // No animation starts automatically, including under prefers-reduced-motion.
})();
