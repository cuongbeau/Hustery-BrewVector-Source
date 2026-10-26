const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync('hmi/web/heater.js','utf8')
 .replace('return {send,download,setActive};','return {send,download,setActive,poll,sensorIssues};');
let state={online:true,ticket:0,result:0,mode:0,fault:0,uptime:1,sampleAge:0,idleRemaining:0,raw:80,filtered:80,power:0,p:0,i:0,d:0,relay:0,valid:1,idleExpired:0,target:115,ceiling:125,kp:2,ki:.05,kd:0,maxPower:30,waterMaxPower:50,activeMaxPower:30,idleMinutes:30,autoSleep:true,sleepDelaySeconds:20,stableSeconds:10,ready:false,heaterW:1300,inletC:25,ffScale:0.25,ffEnabled:false};
let posts=[],fail=false,ack=true,replyResult=0,downloads=0,blob;
const elements=new Map();
function elem(id=''){let value='';const e={id,get value(){return value},set value(v){value=String(v)},textContent:'',disabled:false,clientWidth:800,className:'',append(c){if(c.id)elements.set(c.id,c)},getContext(){return {beginPath(){},moveTo(){},lineTo(){},stroke(){},clearRect(){},strokeRect(){},fillText(){},setLineDash(){}}},click(){downloads++}};return e;}
function get(id){id=id.startsWith('heat_')?id:'heat_'+id;if(!elements.has(id))elements.set(id,elem(id));return elements.get(id)}
const buttons=[elem(),elem()];
const context=vm.createContext({document:{getElementById:get,createElement:()=>elem(),querySelectorAll:()=>buttons},Blob:class{constructor(parts){blob=parts.join('')}},URL:{createObjectURL:()=>'/blob',revokeObjectURL(){}},Date,Number,Math,JSON,Error,Promise,AbortSignal:{timeout:()=>null},setTimeout(fn,ms){if(ms===250)queueMicrotask(fn);},fetch:async(url,options={})=>{
 if(fail)throw Error('network');
 if(options.method==='POST'){const body=JSON.parse(options.body);posts.push(body);const ticket=posts.length;if(ack){state={...state,ticket,result:replyResult};}return {ok:true,json:async()=>({ticket})};}
 return {ok:true,json:async()=>({...state})};
}});
context.clearTimeout=()=>{};
context.AbortController=AbortController;
// Exercise explicit polls below; startup scheduling has its own transport test.
vm.runInContext(source,context);
vm.runInContext('const poll=HeaterTab.poll,send=HeaterTab.send,download=HeaterTab.download,sensorIssues=HeaterTab.sensorIssues;',context);
const run=s=>vm.runInContext(s,context);
(async()=>{
 run('HeaterTab.setActive(true)');
 await run('poll()');assert.equal(get('target').value,'115');assert.equal(get('alarm').textContent,'OFF');
 assert.match(get('pidDetails').textContent,/chưa có/);
 assert.match(get('sensorDiagnostic').textContent,/chưa có/);
 assert.match(get('sensorWindow').textContent,/chưa có/);assert.match(get('sensorPolicy').textContent,/chưa có/);
 Object.assign(state,{sensorDiagnosticsFresh:true,sensorControlSamples:10,sensorControlErrors:3,sensorControlCapacity:20,sensorControlWarnErrors:4,sensorControlPauseErrors:14,sensorControlResumeErrors:8,sensorControlTripErrors:18,sensorSamplePeriodMs:250,sensorMaxGoodAgeMs:5000,sensorPauseGoodAgeMs:1500,sensorPaused:false,sensorDensityPaused:false,sensorErrors:3,sensorTotal:10,sensorIssues:0,sensorLastIssues:2,sensorFrame:0x1900000,sensorLastErrorFrame:0x1910002,sensorLastErrorAt:1000});
 await run('poll()');assert.match(get('sensorDiagnostic').textContent,/3 \/ 10 \(30.0%\)/);assert.match(get('sensorDiagnostic').textContent,/SCG/);assert.match(get('sensorDiagnostic').textContent,/0x01910002/);
 Object.assign(state,{sensorWindowReady:false,sensorWindowSamples:3,sensorWindowErrors:1,sensorConsecutiveErrors:0,sensorMaxConsecutiveErrors:1,sensorPeakWindowSamples:0,sensorPeakWindowErrors:0,sensorEvaluatedWindows:0,sensorWindowsAtOrAbove20:0});
 await run('poll()');assert.match(get('sensorWindow').textContent,/Chưa đủ 5 giây/);
 Object.assign(state,{sensorWindowReady:true,sensorWindowSamples:20,sensorWindowErrors:3,sensorPeakWindowSamples:20,sensorPeakWindowErrors:3,sensorEvaluatedWindows:4});
 await run('poll()');assert.match(get('sensorWindow').textContent,/15.0%.*Bình thường/);
 state.sensorWindowErrors=4;await run('poll()');assert.match(get('sensorWindow').textContent,/20.0%.*Cảnh báo từ 20%/);
 Object.assign(state,{sensorWindowSamples:20,sensorWindowErrors:4});await run('poll()');assert.match(get('sensorWindow').textContent,/20.0%.*Cảnh báo từ 20%/);
 state.sensorWindowReady=false;await run('poll()');assert.match(get('sensorWindow').textContent,/gián đoạn/);
 state.sensorIssues=8;await run('poll()');assert.match(get('sensorDiagnostic').textContent,/Khung SPI/);
 assert.match(run('sensorIssues(5)'),/OC.*SCV/);
 assert.match(run('sensorIssues(32)'),/bus/);
 state.sensorDiagnosticsFresh=false;await run('poll()');assert.match(get('sensorDiagnostic').textContent,/quá hạn/);
 await run('send(1)');assert.equal(posts[0].ceiling,125);assert.match(get('reply').textContent,/đã thực hiện/);
 assert.equal(posts[0].waterMaxPower,50);assert.match(get('applied').textContent,/bù nhiệt TẮT.*trần đang dùng 30%/);assert.equal(posts[0].ffEnabled,false);assert.equal(posts[0].heaterW,1300);assert.equal(posts[0].ffScale,0.25);assert.equal(posts[0].autoSleep,true);assert.equal(posts[0].sleepDelaySeconds,20);assert.equal(posts[0].stableSeconds,10);
 get('autoSleep').checked=false;await run('send(1)');assert.equal(posts.at(-1).autoSleep,false);
 state.mode=1;await run('poll()');assert.match(get('alarm').textContent,/PREHEAT/);
 Object.assign(state,{sensorDiagnosticsFresh:true,thermalFeedforward:0,inletFlow:0,feedforwardKff:0,flowQualified:false,inletValveOpen:false,pidPeriodMs:1000});
 await run('poll()');assert.match(get('pidDetails').textContent,/BrewVector/);assert.match(get('pidDetails').textContent,/TẮT/);assert.match(get('pidDetails').textContent,/chưa đủ điều kiện/);
 state.sensorDiagnosticsFresh=true;state.valid=0;state.sampleAge=750;await run('poll()');
 assert.match(get('alarm').textContent,/PREHEAT/);assert.match(get('diagBadge').textContent,/Có mẫu lỗi/);assert.notEqual(get('alarm').className,'heater-alarm danger');
 assert.match(get('sensorPolicy').textContent,/3 lỗi \/ 10 mẫu/);assert.match(get('sensorPolicy').textContent,/tạm dừng SSR 14\/20/);assert.match(get('sensorPolicy').textContent,/khóa 18\/20/);assert.match(get('sensorPolicy').textContent,/1500 ms/);
 state.sensorControlSamples=20;state.sensorControlErrors=12;state.valid=1;state.sampleAge=0;await run('poll()');
 assert.match(get('alarm').textContent,/PREHEAT/);assert.equal(get('alarm').className,'heater-alarm');assert.match(get('diagBadge').textContent,/Có mẫu lỗi/);assert.match(get('sensorPolicy').textContent,/CẢNH BÁO/);
 state.sensorControlErrors=14;state.sensorPaused=true;state.sensorDensityPaused=true;await run('poll()');
 assert.match(get('pidDetails').textContent,/Tạm dừng cảm biến/);
 assert.match(get('alarm').textContent,/HEATER TẠM DỪNG/);assert.equal(get('alarm').className,'heater-alarm warning');
 state.fault=2;await run('poll()');assert.equal(get('alarm').textContent,'MẪU NHIỆT QUÁ HẠN');assert.equal(get('alarm').className,'heater-alarm danger');
 state.fault=0;state.sensorPaused=false;state.sensorDensityPaused=false;state.sensorControlErrors=3;state.valid=1;state.sampleAge=0;
 state.ready=true;state.autoSleep=false;await run('poll()');assert.match(get('alarm').textContent,/READY/);assert.match(get('liveState').textContent,/READY/);
 replyResult=3;await run('send(2)');assert.match(get('reply').textContent,/Chưa an toàn/);
 replyResult=0;ack=false;const n=posts.length;await run('send(2)');assert.equal(posts.length,n+1);assert.match(get('reply').textContent,/không tự gửi lại/);
 ack=true;await run('send(0)');assert.equal(posts.at(-1).op,0);
 state.fault=1;state.valid=1;state.sampleAge=129;await run('poll()');assert.match(get('alarm').textContent,/LỖI CẢM BIẾN · HEATER OFF/);
 state.valid=0;await run('poll()');assert.equal(get('alarm').textContent,'LỖI CẢM BIẾN');
 state.ceiling=0;replyResult=3;await run('send(2)');assert.match(get('reply').textContent,/Chưa cấu hình ngưỡng/);
 await run('send(4)');assert.match(get('reply').textContent,/cần mẫu nhiệt hợp lệ/);
 state.fault=3;await run('poll()');assert.match(get('alarm').textContent,/QUÁ NHIỆT/);assert.equal(get('alarm').className,'heater-alarm danger');
 fail=true;await run('poll()');assert.match(get('alarm').textContent,/MẤT KẾT NỐI WEB/);assert(buttons.every(b=>b.disabled));
 fail=false;state.online=false;await run('poll()');assert.match(get('alarm').textContent,/CHƯA XÁC NHẬN/);
 assert.match(get('sensorWindow').textContent,/quá hạn/);assert.match(get('sensorPolicy').textContent,/quá hạn/);
 assert.match(get('pidDetails').textContent,/quá hạn/);
 state.online=true;state.sensorDiagnosticsFresh=true;state.uptime++;await run('poll()');
 run('download()');assert.equal(downloads,1);assert.match(blob,/localTime,uptime,raw,filtered,target,power/);
 assert.match(blob,/sensorWindowReady,sensorWindowSamples,sensorWindowErrors/);
 assert.match(blob,/sensorConsecutiveErrors,sensorMaxConsecutiveErrors/);assert.match(blob,/sensorControlSamples,sensorControlErrors/);assert.match(blob,/sensorPauseGoodAgeMs,sensorPaused/);
 assert.match(blob,/thermalFeedforward,inletFlow,feedforwardKff,flowQualified,inletValveOpen,pidPeriodMs/);
 console.log('PASS: actual Web script config, ACK/rejection/timeout without retry, OFF, fault/offline, CSV.');
})().catch(e=>{console.error(e);process.exitCode=1});
