const fs=require('fs'),vm=require('vm'),assert=require('assert/strict');
const source=fs.readFileSync('hmi/web/heater.js','utf8');
const functions=source.slice(source.indexOf('let mainDiagPending='),source.indexOf('function controls()'));
let data={online:true,build:'test',bootId:2,bootCount:3,uptimeMs:5000,resetReason:6,freeHeap:123,minFreeHeap:100,largestFreeBlock:80,adcSingleShot:true,adcPollMs:3,adcMeanSamples:7,faultCount:1,faultSaved:true,lastFault:{bootId:1,atMs:15000,fault:1,sampleAgeMs:4500,windowErrors:18,windowSamples:20,sensorIssues:4,rawC:113,filteredC:112}};
const element={textContent:''};let now=10000,calls=0,fail=false;
const context=vm.createContext({$:()=>element,Date:class extends Date{static now(){return now}},AbortController,setTimeout,clearTimeout,faults:['','LỖI CẢM BIẾN'],sensorIssues:mask=>'issues '+mask,fetch:async url=>{calls++;assert.equal(url,'/api/main/diagnostics');if(fail)throw Error('network');return {ok:true,json:async()=>data}}});
vm.runInContext(functions,context);
const refresh=()=>vm.runInContext('refreshMainDiagnostics()',context);
(async()=>{
 await refresh();assert.match(element.textContent,/Task watchdog/);assert.match(element.textContent,/trước khi Main reset/);assert.match(element.textContent,/18\/20/);assert.match(element.textContent,/Đã lưu vào bộ nhớ/);
 await refresh();assert.equal(calls,1,'diagnostics is throttled independently from heater polling');
 now+=2001;data={...data,online:false};await refresh();assert.match(element.textContent,/quá hạn/);assert.doesNotMatch(element.textContent,/Task watchdog/,'stale reset reason must not look current');
 now+=2001;data={...data,online:true,faultCount:0,lastFault:undefined};await refresh();assert.match(element.textContent,/Chưa ghi nhận lỗi/);
 now+=2001;fail=true;await refresh();assert.match(element.textContent,/network/);
 now+=2001;fail=false;data={...data,lastFault:{bootId:2,atMs:50,fault:1,sampleAgeMs:0,windowErrors:18,windowSamples:20,sensorIssues:2,rawC:80,filteredC:79},faultCount:1,faultSaved:false};await refresh();assert.match(element.textContent,/Đang giữ trong RTC/);assert.doesNotMatch(element.textContent,/trước khi Main reset/);
 console.log('PASS: reset reason, retained fault boot/time, stale/error handling, polling throttle and unsaved-event disclosure.');
})().catch(e=>{console.error(e);process.exitCode=1});
