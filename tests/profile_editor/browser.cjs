// Local-only regression harness. Never contacts the espresso machine.
const fs=require('node:fs'),path=require('node:path'),http=require('node:http'),assert=require('node:assert/strict');
const root=path.resolve(__dirname,'../..');
const read=f=>fs.readFileSync(path.join(root,f),'utf8');
const phase=(name,duration,pump,transition={type:'instant'})=>({name,phase:name==='Pre-infusion'?'preinfusion':'brew',duration,valve:1,pump,transition,targets:[]});
const phases=[phase('Pre-infusion',5,{target:'pressure',pressure:3,flow:0}),phase('Soak',3,0),phase('Rise',4,{target:'pressure',pressure:9,flow:0},{type:'linear',target:'time',duration:4,adaptive:false}),phase('Extract',20,{target:'pressure',pressure:9,flow:0}),phase('Finish',8,{target:'pressure',pressure:6,flow:0},{type:'linear',target:'time',duration:8,adaptive:false})];
let profiles=['First recipe','9 Bar Soft Finish','Third recipe'].map((label,i)=>({id:'recipe-'+i,label,type:'pro',description:'Ngấm nhẹ · Giữ 9 bar · Hạ áp cuối shot',temperature:0,phases:structuredClone(phases)}));
let rejectSave=false;
const shell=read('hmi/src/web/web_html.h').split('R"HTML(')[1].split(')HTML"')[0];
const js=read('hmi/src/web/web_js_profiles.h').split('R"JS(')[1].split(')JS"')[0];
const html=shell+`function switchTab(){document.querySelectorAll('.page').forEach(e=>{e.classList.toggle('active',e.id==='page-prof');e.style.display=e.id==='page-prof'?'block':'none'});document.querySelectorAll('[data-tab]').forEach(e=>{e.classList.toggle('active',e.dataset.tab==='prof');e.setAttribute('aria-selected',e.dataset.tab==='prof')});}function startLive(){}function httpGetJSON(url,ok,bad){fetch(url).then(r=>r.json()).then(ok).catch(bad)}function httpPostJSON(url,body,ok,bad){fetch(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}).then(r=>r.json()).then(ok).catch(bad)}\n`+js+'</script></body></html>';
const server=http.createServer(async(req,res)=>{res.setHeader('Content-Type','application/json');if(req.url==='/profiles')res.end(JSON.stringify({ok:true,profiles}));else if(req.url==='/profile/save'){let body='';for await(const c of req)body+=c;const x=JSON.parse(body);if(!rejectSave)profiles[x.index]=x.profile;res.end(JSON.stringify({ok:!rejectSave}));}else if(req.url==='/profile/delete'){let body='';for await(const c of req)body+=c;profiles.splice(JSON.parse(body).index,1);res.end('{"ok":true}');}else{res.setHeader('Content-Type','text/html; charset=utf-8');res.end(html);}});
const port=process.argv.includes('--serve')?8766:8767;
server.listen(port,'127.0.0.1',async()=>{
 if(process.argv.includes('--serve')){console.log('Preview http://127.0.0.1:8766 (stub Main only)');return;}
 let browser;
 try{
 const {chromium}=require(process.env.PLAYWRIGHT_MODULE||'playwright');browser=await chromium.launch({channel:'msedge',headless:true});const page=await browser.newPage({viewport:{width:1536,height:1024}});
 const errors=[];page.on('pageerror',e=>errors.push(e.message));page.on('dialog',d=>d.accept());
 await page.goto('http://127.0.0.1:'+port);await page.locator('#profile_list .profile-row').nth(1).getByText('Chỉnh sửa',{exact:true}).click();await page.getByRole('button',{name:'4 Extract',exact:false}).click();
 const out=path.join(root,'tmp/profile-editor-qa');fs.mkdirSync(out,{recursive:true});await page.evaluate(()=>document.getElementById('page-prof').scrollTop=0);await page.screenshot({path:path.join(out,'desktop.png')});
 await page.locator('#profile_name').fill('Only second renamed');await page.locator('#profile_name').press('Tab');
 assert.equal(await page.evaluate(()=>cacheProfiles[1].label),'9 Bar Soft Finish');
 await page.locator('#profile_save').click();await page.locator('#profile_library').waitFor({state:'visible'});
 assert.deepEqual(profiles.map(p=>p.label),['First recipe','Only second renamed','Third recipe']);
 await page.locator('#profile_list .profile-row').nth(1).getByText('Chỉnh sửa',{exact:true}).click();
 await page.locator('#profile_phase_name').fill('Infusion');await page.locator('#profile_phase_name').press('Tab');
 await page.locator('#profile_phase_list .profile-menu summary').first().click();await page.locator('#profile_phase_list .profile-menu-items').first().getByText('Nhân bản',{exact:true}).click();
 assert.equal(await page.evaluate(()=>profileDraft.phases.length),6);
 await page.locator('#profile_phase_name').fill('Independent');await page.locator('#profile_phase_name').press('Tab');assert.equal(await page.evaluate(()=>profileDraft.phases[0].name),'Infusion');
 await page.getByRole('button',{name:'＋ Điều kiện',exact:true}).click();await page.locator('#profile_tv_0').fill('26');await page.locator('#profile_tv_0').press('Tab');assert.equal(await page.evaluate(()=>profileDraft.phases[profilePhase].targets[0].value),26);
 await page.getByRole('button',{name:'Flow',exact:true}).click();await page.locator('#profile_value').fill('2.5');await page.locator('#profile_value').press('Tab');await page.locator('#profile_advanced>summary').click();await page.locator('#profile_source').selectOption('entry');assert.equal(await page.evaluate(()=>profileDraft.phases[profilePhase].pump.flow),-1);await page.locator('#profile_limit_mode').selectOption('custom');await page.locator('#profile_limit').fill('8');await page.locator('#profile_limit').press('Tab');assert.equal(await page.evaluate(()=>profileDraft.phases[profilePhase].pump.pressure),8);await page.locator('#profile_limit_mode').selectOption('entry');assert.equal(await page.evaluate(()=>profileDraft.phases[profilePhase].pump.pressure),-1);
 await page.getByRole('button',{name:'Hủy thay đổi',exact:true}).click();assert.equal(await page.evaluate(()=>profileDraft.phases.length),5);
 rejectSave=true;await page.locator('#profile_name').fill('Rejected draft');await page.locator('#profile_save').click();await page.getByText('Main từ chối lưu.',{exact:true}).waitFor();assert.equal(await page.evaluate(()=>profileDraft.label),'Rejected draft');rejectSave=false;
 await page.getByRole('button',{name:'Hủy thay đổi',exact:true}).click();
 const [download]=await Promise.all([page.waitForEvent('download'),page.evaluate(()=>exportProfile())]);await download.saveAs(path.join(out,'export.json'));assert.deepEqual(JSON.parse(fs.readFileSync(path.join(out,'export.json'),'utf8')),profiles[1]);
 await page.evaluate(()=>closeEditor());const chooserPromise=page.waitForEvent('filechooser');await page.getByRole('button',{name:'Nhập JSON',exact:true}).click();await (await chooserPromise).setFiles(path.join(out,'export.json'));await page.locator('#profile_name').waitFor();assert.notEqual(await page.evaluate(()=>profileDraft.id),profiles[1].id);assert.equal(await page.evaluate(()=>profileDraft.label),profiles[1].label);
 await page.evaluate(()=>closeEditor());await page.evaluate(()=>openEditor(1));await page.evaluate(()=>profilePick(3));
 for(const [name,width,height] of [['ipad',820,1180],['mobile',390,844]]){await page.setViewportSize({width,height});await page.screenshot({path:path.join(out,name+'.png'),fullPage:true});assert.equal(await page.evaluate(()=>document.querySelector('#profiles_workspace').scrollWidth<=document.querySelector('#profiles_workspace').clientWidth+1),true);}
 assert.equal(await page.evaluate(()=>profilePreviewSegments([{duration:5,pump:{target:'pressure',pressure:-1,flow:0},transition:{}}],'pressure')[0].points.length),0);
 assert.deepEqual(errors,[]);console.log('PASS rename isolation, clone isolation, phase edits, yield condition, Adaptive, cancel, rejected save, JSON export/import, responsive widths, unknown preview.');
 }catch(e){console.error(e);process.exitCode=1;}finally{if(browser)await browser.close();server.close();}
});
