const {chromium}=require('playwright');
const fs=require('node:fs');
const path=require('node:path');
const assert=require('node:assert/strict');
const {pathToFileURL}=require('node:url');
(async()=>{
 const root=path.resolve(__dirname,'../../..'),out=path.join(root,'work/publish-v341'),manual=path.join(root,'docs/v3.4.1/manual/index.html');
 fs.mkdirSync(out,{recursive:true});
 const browser=await chromium.launch({executablePath:process.env.PLAYWRIGHT_EXECUTABLE_PATH||'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe',headless:true});
 const context=await browser.newContext({viewport:{width:1440,height:1000},offline:true,reducedMotion:'reduce'});
 const page=await context.newPage(),errors=[],remote=[],checks=[];
 page.on('pageerror',e=>errors.push(String(e)));
 page.on('request',r=>{if(/^https?:/.test(r.url()))remote.push(r.url());});
 async function check(name,fn){await fn();checks.push({name,status:'PASS'});}
 try {
 await page.goto(pathToFileURL(manual).href,{waitUntil:'load'});
 await check('95 entries, 13 groups, 474 parameter rows',async()=>{
  assert.equal(await page.locator('.fx-entry').count(),95);
  assert.equal(await page.locator('.fx-group').count(),13);
  assert.equal(await page.locator('.fx-entry tbody tr').count(),474);
 });
 await check('fixed manual has no editable text or network stylesheet',async()=>{
  assert.equal(await page.locator('[contenteditable="true"]').count(),0);
  assert.equal(await page.locator('link[href^="http"],script[src^="http"]').count(),0);
 });
 await check('all screenshot images decode offline',async()=>{
  const decoded=await page.evaluate(async()=>{const all=[...document.querySelectorAll('.manual-shot img')];await Promise.all(all.map(i=>{i.loading='eager';return i.decode();}));return all.filter(i=>i.naturalWidth>0).length;});assert.equal(decoded,111);
 });
 await page.screenshot({path:path.join(out,'manual-desktop.png')});
 await check('TOC and quick links reach valid anchors',async()=>{
  const missing=await page.evaluate(()=>[...document.querySelectorAll('a[href^="#"]')].filter(a=>a.hash.length>1&&!document.getElementById(decodeURIComponent(a.hash.slice(1)))).map(a=>a.hash));assert.deepEqual(missing,[]);
  await page.locator('.quick-links a').nth(1).click();assert.equal(new URL(page.url()).hash,'#manual-timeline');
 });
 await check('FX search/group filters and expand/collapse',async()=>{
  await page.locator('#fx-search').fill('FX02.2');assert.equal(await page.locator('.fx-entry:visible').count(),1);
  assert.match(await page.locator('.fx-entry:visible summary').innerText(),/SMAA/);
  await page.locator('#fx-search').fill('');await page.locator('#fx-group').selectOption('1');assert.equal(await page.locator('.fx-entry:visible').count(),9);
  await page.locator('#fx-expand').click();assert.equal(await page.locator('.fx-entry[open]:visible').count(),9);
  await page.locator('#fx-collapse').click();assert.equal(await page.locator('.fx-entry[open]').count(),0);
  await page.locator('#fx-search').fill('不存在的效果999');assert.equal(await page.locator('.fx-entry:visible').count(),0);
  await page.locator('#fx-search').fill('');await page.locator('#fx-group').selectOption('2');await page.locator('#FX02-2').evaluate(el=>el.open=true);
 });
 await page.locator('#fx-search').scrollIntoViewIfNeeded();await page.screenshot({path:path.join(out,'manual-catalog.png')});
 await check('real screenshot zoom dialog opens and Escape closes',async()=>{
  await page.locator('#FX02-2 .zoom-shot').click();assert.equal(await page.locator('#shot-dialog').evaluate(el=>el.open),true);await page.keyboard.press('Escape');assert.equal(await page.locator('#shot-dialog').evaluate(el=>el.open),false);
 });
 await check('parallax depth relationship changes',async()=>{
  await page.locator('#cam-x').fill('150');const before=await page.locator('#far-layer').getAttribute('transform');
  assert.notEqual(before,await page.locator('#near-layer').getAttribute('transform'));
  await page.locator('#same-depth').check();assert.equal(await page.locator('#far-layer').getAttribute('transform'),await page.locator('#near-layer').getAttribute('transform'));
 });
 await check('keyframe interpolation, Bezier controls, play/pause/reset',async()=>{
  await page.locator('#key-time').fill('1.5');assert.match(await page.locator('#key-readout').innerText(),/X=50.00/);
  await page.locator('#ease-select').selectOption('in');assert.match(await page.locator('#key-readout').innerText(),/X=25.00/);
  await page.locator('#ease-select').selectOption('hold');assert.match(await page.locator('#key-readout').innerText(),/X=0.00/);
  await page.locator('#ease-select').selectOption('bezier');await page.locator('#bez-x1').fill('85');await page.locator('#bez-x2').fill('20');
  assert.ok(+(await page.locator('#bez-x1').inputValue())<=+(await page.locator('#bez-x2').inputValue()));
  await page.locator('#key-reset').click();await page.locator('#key-play').click();await page.waitForTimeout(250);await page.locator('#key-play').click();assert.ok(+(await page.locator('#key-time').inputValue())>0);
  const paused=await page.locator('#key-time').inputValue();await page.waitForTimeout(120);assert.equal(await page.locator('#key-time').inputValue(),paused);
  await page.locator('#key-reset').click();assert.equal(+(await page.locator('#key-time').inputValue()),0);
 });
 await page.locator('#keyframe-demo').scrollIntoViewIfNeeded();await page.screenshot({path:path.join(out,'manual-keyframes.png')});
 await check('bound light follows animated layer; unbound stays fixed',async()=>{
  await page.locator('#light-layer-x').fill('100');const bound=await page.locator('#light-marker').getAttribute('transform');await page.locator('#light-bound').uncheck();assert.notEqual(await page.locator('#light-marker').getAttribute('transform'),bound);
  const fixed=await page.locator('#light-marker').getAttribute('transform');await page.locator('#light-layer-x').fill('-100');assert.equal(await page.locator('#light-marker').getAttribute('transform'),fixed);
 });
 await check('FX dual gate and strength zero',async()=>{
  await page.locator('#gate-group').check();assert.match(await page.locator('#gate-status').innerText(),/子功能未勾选/);await page.locator('#gate-child').check();assert.match(await page.locator('#gate-status').innerText(),/已启用/);await page.locator('#gate-strength').fill('0');assert.match(await page.locator('#gate-status').innerText(),/保留原画/);
 });
 await check('BPM/ratio/minimum spacing candidate count',async()=>{
  assert.equal(await page.locator('#beat-lines line').count(),9);await page.locator('#demo-ratio').selectOption('0.5');assert.equal(await page.locator('#beat-lines line').count(),17);
  await page.locator('#demo-minimum').fill('0.5');assert.match(await page.locator('#beat-readout').innerText(),/保留 9 个/);
 });
 await check('desktop page has no horizontal overflow',async()=>assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)));
 await page.setViewportSize({width:390,height:844});await page.goto(pathToFileURL(manual).href,{waitUntil:'load'});
 await check('mobile no horizontal page overflow, TOC drawer works',async()=>{
  assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1));
  await page.locator('#toc-toggle').click();assert.equal(await page.locator('#toc-toggle').getAttribute('aria-expanded'),'true');
  await page.locator('#toc-panel .toc-item').nth(6).click();assert.equal(await page.locator('#toc-toggle').getAttribute('aria-expanded'),'false');
 });
 await page.goto(pathToFileURL(manual).href,{waitUntil:'load'});await page.screenshot({path:path.join(out,'manual-mobile.png')});
 await page.locator('#fx-search').fill('烛光');await page.locator('#fx-expand').click();await page.locator('.fx-entry:visible').scrollIntoViewIfNeeded();await page.screenshot({path:path.join(out,'manual-mobile-effect.png')});
 await check('mobile effect table scroll stays inside its container',async()=>assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth+1)));
 assert.deepEqual(errors,[]);assert.deepEqual(remote,[]);checks.push({name:'zero JS errors, zero HTTP requests while fully offline',status:'PASS'});
 fs.writeFileSync(path.join(out,'manual-qa.json'),JSON.stringify({status:'PASS',browser:await browser.version(),checks,errors,remote,viewports:['1440x1000','390x844'],note:'Browser/manual checks only; no new OBS runtime or performance test.'},null,2));
 console.log(JSON.stringify({status:'PASS',checks:checks.length,errors,remote}));
 }catch(e){await page.screenshot({path:path.join(out,'manual-failure.png')});fs.writeFileSync(path.join(out,'manual-qa.json'),JSON.stringify({status:'FAIL',error:String(e),checks,errors,remote},null,2));throw e;}
 finally{await browser.close();}
})().catch(e=>{console.error(e);process.exitCode=1;});
