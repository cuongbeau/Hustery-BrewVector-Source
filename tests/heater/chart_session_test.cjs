const fs=require('fs'),vm=require('vm'),assert=require('assert/strict');
let state={online:true,uptime:16000000,mainBootId:1,mode:0,fault:0,raw:34,filtered:34,target:113,power:0,p:0,i:0,d:0,relay:0};
const nodes=new Map();let points=[],csv='';
const ctx={strokeStyle:'',clearRect(){points=[]},beginPath(){},moveTo(x,y){if(this.strokeStyle==='#ffaf54')points.push({x,y})},lineTo(x,y){if(this.strokeStyle==='#ffaf54')points.push({x,y})},stroke(){},strokeRect(){},fillText(){},setLineDash(){}};
function el(id){if(!nodes.has(id))nodes.set(id,{id,textContent:'',value:'',className:'',disabled:false,clientWidth:800,append(){},click(){},getContext:()=>ctx});return nodes.get(id);}
const c=vm.createContext({document:{getElementById:el,createElement:()=>el('download-link'),querySelectorAll:()=>[]},Date,Number,Math,JSON,Error,Promise,AbortController,setTimeout:()=>1,clearTimeout(){},Blob:class{constructor(parts){csv=parts.join('')}},URL:{createObjectURL:()=>'/csv',revokeObjectURL(){}},fetch:async url=>({ok:true,json:async()=>url==='/api/main/diagnostics'?{online:false}:{...state}})});
const source=fs.readFileSync('hmi/web/heater.js','utf8').replace('return {send,download,setActive};','return {send,download,setActive,poll,chartSessionRows,peek:()=>rows};');
vm.runInContext(source,c);const run=s=>vm.runInContext(s,c);
(async()=>{
 run('HeaterTab.setActive(true)');await run('HeaterTab.poll()');
 state.uptime+=500;await run('HeaterTab.poll()');
 Object.assign(state,{uptime:500000,mainBootId:2,filtered:120.4,raw:120.5,mode:1,ready:true});await run('HeaterTab.poll()');
 assert.match(el('heat_liveTemp').textContent,/120.4/);assert.equal(el('heat_liveState').textContent,'READY');
 assert.equal(run('HeaterTab.peek().length'),3,'CSV history survives reboot');
 assert.equal(points.length,1);assert(points.every(p=>p.x>=43&&p.x<=757&&p.y<40),'new hot sample must be visible on new chart timeline');
 state.uptime+=500;await run('HeaterTab.poll()');assert.equal(points.length,2);assert(points.every(p=>p.x>=43&&p.x<=757));
 state.mainBootId=3;await run('HeaterTab.poll()');assert.equal(points.length,1,'new boot at equal uptime must append and start new chart');
 run('HeaterTab.download()');assert.match(csv,/mainBootId/);assert.match(csv,/16000000/);assert.match(csv,/120.4/);
 Object.assign(state,{uptime:state.uptime+500,sensorDiagnosticsFresh:true,sensorPaused:true,overtempPending:true,overtempConfirmMs:500,medianTemperature:120,overtempEvents:1,overtempAt:350,overtempPeak:126,sensorFrame:0,sensorLastErrorFrame:0});
 await run('HeaterTab.poll()');
 assert.match(el('heat_alarm').textContent,/CHỜ XÁC NHẬN QUÁ NHIỆT/);
 assert.match(el('heat_sensorDiagnostic').textContent,/trung vị 3 mẫu 120.0/);
 assert.match(el('heat_sensorDiagnostic').textContent,/đỉnh thô 126.0/);
 run('HeaterTab.download()');assert.match(csv,/medianTemperature,overtempPending,overtempEvents/);
 state.medianTemperature=null;state.uptime+=500;await run('HeaterTab.poll()');
 assert.match(el('heat_sensorDiagnostic').textContent,/đã bỏ trung vị/);
 assert.doesNotMatch(el('heat_sensorDiagnostic').textContent,/trung vị 3 mẫu/);
 state.sensorDiagnosticsFresh=false;state.uptime+=500;await run('HeaterTab.poll()');
 assert.doesNotMatch(el('heat_alarm').textContent,/CHỜ XÁC NHẬN QUÁ NHIỆT/,'expired diagnostic must not report current confirmation');
 const cases=[
  [[{uptime:100},{uptime:200},{uptime:10},{uptime:20}],2],
  [[{uptime:100,mainBootId:1},{uptime:200,mainBootId:2}],1],
  [[{uptime:100,mainBootId:1},{uptime:200},{uptime:300,mainBootId:1}],3],
  [[{uptime:100,mainBootId:1},{uptime:10},{uptime:20,mainBootId:2}],2],
  [[{uptime:100,mainBootId:1},{uptime:200,mainBootId:2},{uptime:300,mainBootId:3}],1]
 ];
 for(const [rows,length] of cases){c.input=rows;assert.equal(run('HeaterTab.chartSessionRows(input).length'),length);}
 console.log('PASS: actual PID chart stays in bounds after reboot; boot-ID and uptime fallback; missing metadata; old CSV samples retained.');
})().catch(e=>{console.error(e);process.exitCode=1});
