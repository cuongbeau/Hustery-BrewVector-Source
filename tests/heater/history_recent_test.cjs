const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const source=fs.readFileSync('hmi/src/web/web_js_history.h','utf8').match(/R"JS\(([\s\S]*?)\)JS"/)[1];
const nodes={},requests=[],profileRequests=[];let success,loads=0;
const ctx=vm.createContext({window:{addEventListener(){}},TextDecoder,Uint8Array,AbortController,
 setTimeout,clearTimeout,el(id){return nodes[id]??={innerHTML:'',textContent:''};},
 httpGetJSON(url,ok){assert.equal(url,'/traces/list');loads++;success=ok;},
 fetch(url,options){
   if(url==='/profiles'){profileRequests.push(url);return Promise.resolve({ok:true,json:async()=>({ok:true,profiles:[{index:2,label:'TEST & SHOT',phaseCount:1}]})});}
   if(url==='/profile/detail?index=2'){profileRequests.push(url);return Promise.resolve({ok:true,json:async()=>({ok:true,profile:{label:'TEST & SHOT',phases:[{name:'Preinfusion'},{name:'Extraction'}]}})});}
   return new Promise((resolve,reject)=>{
   requests.push({url,options,resolve});options.signal.addEventListener('abort',()=>reject(Error('aborted')));
 });}});
vm.runInContext(source,ctx);
const csv='time_s,output_weight_g,scale_weight_g\n10,2,2\n# stop_elapsed_s,10.4\n# stop_scale_weight_g,3.5\n# stop_model_yield_g,4\n# stop_flags,2\n';
let parsed=ctx.histParseCsv(csv),final=ctx.histFinalValues(parsed);
assert.equal(final.duration,10.4);assert.equal(final.weight,3.5);assert.equal(final.delta,.5);
final=ctx.histFinalValues(ctx.histParseCsv(csv.replace('stop_flags,2','stop_flags,0')));
assert.equal(final.weight,null);assert.equal(final.delta,null);
assert.equal(ctx.histFinalValues(ctx.histParseCsv('time_s,output_weight_g\n8,4')).weight,null,'Legacy trace cannot establish valid stop yield');
assert.equal(ctx.histFmt(null),'—');
const phaseCsv='# profile,Test & shot\n'+
 'time_s,step,mode,target_command,pressure_filtered_bar,controller_target_bar,pump_flow_ml_s,water_pumped_ml,scale_weight_g,rx_gap\n'+
 '0,0,0,0,1,2,2,0,0,0\n1,0,0,0,2,2,2,2,0,0\n2,1,1,4,4,5,3,3,1,0\n3,1,1,4,5,5,3,6,2,0\n'+
 '# stop_elapsed_s,3.5\n# stop_scale_weight_g,2.5\n# stop_model_yield_g,3\n# stop_flags,2\n# stop_reason,profile_complete\n# profile_exit_reason,target_reached\n';
const analyzed=ctx.histParseCsv(phaseCsv),phases=ctx.histPhases(analyzed);
assert.equal(phases.length,2);
assert.equal(phases[0].duration,2);
assert.equal(phases[1].duration,1.5);
assert.equal(phases[0].pressure,1.5);
assert.equal(phases[1].water,4);
assert.equal(phases[1].target,4);
assert.equal(phases[1].mode,1);
assert.equal(phases[1].reason,'target_reached');
const named=ctx.histParseCsv(phaseCsv+'# step_name,0,Preinfusion\n# step_name,1,Extraction\n');
assert.equal(named.namesSource,'trace');
assert.equal(ctx.histPhaseLabel(named,ctx.histPhases(named)[1]),'Bước 2 · Extraction');
assert(ctx.histProfileNameMatches('A very long profile name','A very long profile nam'));
assert(ctx.histWarnings(analyzed).some(w=>w.includes('khoảng mẫu')),'Sparse fixture should report trace gaps');
assert.equal(ctx.histParseCsv('time_s,pump_flow_ml_s\n0,\n1,2').rows[0].pump_flow_ml_s,null,'Blank CSV values are unknown, not zero');
assert.equal(ctx.histPumpedAmount([{time_s:0,pump_flow_ml_s:2},{time_s:0.5,pump_flow_ml_s:2}]),1,'Old traces integrate pump flow when counter is absent');
assert(ctx.histWarnings(ctx.histParseCsv(phaseCsv.replace('stop_flags,2','stop_flags,0'))).some(w=>w.includes('cân hợp lệ')));
const noop=()=>{},dashes=[],fills=[],draw={setTransform:noop,clearRect:noop,fillRect(...a){fills.push(a);},beginPath:noop,moveTo:noop,lineTo:noop,stroke:noop,fillText:noop,setLineDash(a){dashes.push(a.join(','));}};
nodes.hist_chart={getBoundingClientRect:()=>({left:0,width:800,height:300}),getContext:()=>draw};
const strip={style:{},clientWidth:710,innerHTML:''};
const phaseButtons=Array.from({length:2},()=>({active:false,classList:{toggle(){}}}));
nodes.hist_detail={innerHTML:'',querySelector:sel=>sel==='.hist-phase-strip'?strip:null,
 querySelectorAll:sel=>sel==='.hist-phase-strip button'?phaseButtons:[]};
ctx.fixture=analyzed;
vm.runInContext('hmiHistCurrent=Object.assign({id:12},fixture)',ctx);
ctx.histRenderDetail();
assert(nodes.hist_detail.innerHTML.includes('Phân tích theo bước'));
assert(nodes.hist_detail.innerHTML.includes('target_reached'));
assert(!nodes.hist_detail.innerHTML.includes('Test & shot'),'Profile must be escaped in details');
ctx.namedFixture=Object.assign({id:12},named);
vm.runInContext('hmiHistCurrent=namedFixture',ctx);
ctx.histRenderDetail();
assert(nodes.hist_detail.innerHTML.includes('Bước 2 · Extraction'));
assert(strip.innerHTML.includes('Bước 2 · Extraction'));
assert(strip.innerHTML.includes('left:57.14'),'Phase strip must align with plotted time');
assert.equal(fills.length,0,'No phase is shaded until selected');
const beforeFocus=dashes.length;
ctx.histFocusPhase(1);
assert.equal(ctx.hmiHistSelectedPhase,1);
assert.equal(fills.length,1,'Clicking a phase shades its chart region');
assert(!dashes.slice(beforeFocus).includes('4,4'),'Selecting a phase alone does not show the hover guide');
ctx.histChartMove({clientX:300});
assert(dashes.includes('4,4'),'Hovering the chart shows a vertical guide');
ctx.histChartClick({clientX:100});
assert.equal(ctx.hmiHistSelectedPhase,0,'Clicking the chart selects its phase');
ctx.compareFixture=Object.assign({id:11},analyzed);
vm.runInContext('hmiHistCompare=compareFixture;hmiHistCompareId=11',ctx);
ctx.histRenderDetail();
assert(nodes.hist_detail.innerHTML.includes('Δ so với shot'));
vm.runInContext('hmiHistCurrent=null;hmiHistCompare=null;hmiHistCompareId=0',ctx);
let startup;
const tabs=[];
const profileSource=fs.readFileSync('hmi/web/profiles.js','utf8');
const startupContext={window:{addEventListener(name,fn){startup=fn;}},location:{hash:'#history'},profileShell(){},switchTab(name){tabs.push(name);},startLive(){}};
vm.runInNewContext(profileSource.slice(profileSource.lastIndexOf("window.addEventListener('load',")),startupContext);
startup();assert.equal(tabs.at(-1),'hist');
startupContext.location.hash='';startup();assert.equal(tabs.at(-1),'dash');
ctx.histRenderDetail=()=>{};
(async()=>{
 ctx.loadHmiHistory();ctx.loadHmiHistory();assert.equal(loads,1);
 success({traces:Array.from({length:15},(_,i)=>({id:i+1,profile:'<demo>',duration:20,peak:9}))});
 assert.equal(ctx.hmiHistTraces.length,10);assert.equal(ctx.hmiHistTraces[0].id,15);
 assert.equal(requests.length,0,'List must not automatically download CSV');
 assert(!nodes.hist_list.innerHTML.includes('<demo>'),'Profile text must be escaped');
 ctx.loadHmiHistory();assert.equal(loads,1,'Re-entering tab keeps list');
 ctx.loadHmiHistory(true);assert.equal(loads,2,'Explicit refresh reloads');
 success({traces:[{id:15,profile:'Demo',duration:20,peak:9}]});
 ctx.histSelect(15);assert.equal(requests.length,1);
 ctx.histSelect(15);assert.equal(requests.length,1,'Repeated selection does not overlap');
 ctx.pauseHmiHistory();assert(requests[0].options.signal.aborted);
 await new Promise(resolve=>setImmediate(resolve));assert.equal(ctx.hmiHistCurrent,null);
 ctx.histSelect(15);assert.equal(requests.length,2,'Interrupted read can resume by selection');
 ctx.pauseHmiHistory();await new Promise(resolve=>setImmediate(resolve));
 ctx.fixture=Object.assign({id:15},analyzed);
 vm.runInContext('hmiHistCurrent=fixture;hmiHistTraces.push({id:14,profile:"Other"})',ctx);
 ctx.histSetCompare('14');assert.equal(requests.length,3,'Comparison loads the selected second trace');
 const bytes=Uint8Array.from(Buffer.from(phaseCsv));
 requests[2].resolve({ok:true,headers:{get(name){assert.equal(name,'X-Trace-Total');return String(bytes.length);}},arrayBuffer:async()=>bytes.buffer});
 await new Promise(resolve=>setTimeout(resolve,30));
 assert.equal(ctx.hmiHistCompare.id,14);
 assert.equal(ctx.hmiHistCompare.rows.length,4);
 ctx.histSetCompare('0');assert.equal(ctx.hmiHistCompare,null);
 ctx.fixture=Object.assign({id:15},analyzed,{stepNames:[],namesSource:null,namesTried:false});
 vm.runInContext('hmiHistCurrent=fixture',ctx);
 ctx.histResolveProfileNames(ctx.fixture);
 await new Promise(resolve=>setImmediate(resolve));
 assert.equal(ctx.fixture.namesSource,'current');
 assert.equal(ctx.fixture.stepNames[1],'Extraction');
 assert.deepEqual(profileRequests,['/profiles','/profile/detail?index=2']);
 console.log('PASS: history lifecycle, phase names, profile fallback, chart cursor, comparison and data warnings.');
})().catch(e=>{console.error(e);process.exitCode=1;});
