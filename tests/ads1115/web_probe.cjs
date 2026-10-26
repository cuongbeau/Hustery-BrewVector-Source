// Run the actual page script: no duplicated classification implementation.
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path'),assert=require('node:assert/strict');
const {webcrypto}=require('node:crypto');
const source=fs.readFileSync(path.join(__dirname,'../../hmi/src/web/routes/adc_probe_routes.cpp'),'utf8');
const script=source.match(/<script>([\s\S]*?)<\/script>/)[1];
const nodes=Object.fromEntries(['start','cancel','download','status','interpretation','rows'].map(k=>[k,{textContent:'',innerHTML:'',disabled:false}]));
let meta={run_id:0,version:1,state:0,error:0,result:0,pump_held:false,max_poll_gap_us:1000,samples:[]},reply;
let starts=0,loseAck=true;const blobs=[];
const response=(status,data)=>({status,ok:status>=200&&status<300,json:async()=>data});
const c=vm.createContext({console,Blob,crypto:webcrypto,setTimeout:fn=>queueMicrotask(fn),setInterval:()=>0,
 document:{getElementById:id=>nodes[id],createElement:()=>({click(){},remove(){}}),body:{appendChild(){}}},
 URL:{createObjectURL:b=>{blobs.push(b);return 'blob:x';},revokeObjectURL(){}},
 fetch:async(url,options={})=>{
  if(options.method==='POST'){
   const r=JSON.parse(options.body);
   if(r.op===1&&r.run_id!==meta.run_id){starts++;meta={...meta,run_id:r.run_id,state:1,pump_held:true};}
   if(r.op===2)meta={...meta,state:5,pump_held:false,error:1};
   reply={...meta,request_id:r.request_id};
   if(r.op===1&&loseAck){loseAck=false;return response(503,{error:'Lost ACK'});}
   return response(202,{pending:true});
  }
  return response(200,reply);
 }
});
function samples(times,bits){return [0,2,4,7].flatMap((code,i)=>Array.from({length:6},()=>[code,times[i],times[i]-1000,1232+bits,bits]));}
(async()=>{
 vm.runInContext(script,c);await new Promise(setImmediate);
 await nodes.start.onclick();assert.equal(starts,1);assert.equal(nodes.start.disabled,true);
 meta={...meta,state:4,pump_held:false,samples:samples([126000,32000,9000,2000],7)};
 await vm.runInContext('refresh()',c);assert.match(nodes.interpretation.textContent,/gần ADS1115/);assert.match(nodes.interpretation.textContent,/Có hoạt động/);
 meta.samples=samples([9000,3000,1000,1000],0);
 await vm.runInContext('refresh()',c);assert.match(nodes.interpretation.textContent,/gần ADS1015/);
 meta.max_poll_gap_us=9000;await vm.runInContext('refresh()',c);assert.match(nodes.interpretation.textContent,/bị trễ/);
 meta.state=7;meta.pump_held=true;await vm.runInContext('refresh()',c);
 assert.equal(nodes.start.disabled,true);assert.equal(nodes.cancel.disabled,false);assert.match(nodes.status.textContent,/bơm vẫn khóa/);
 nodes.download.onclick();const json=JSON.parse(await blobs[0].text());assert.equal(json.state,7);assert.equal(json.samples.length,24);
 await nodes.cancel.onclick();assert.equal(nodes.start.disabled,false);
 console.log('PASS: real probe page idempotent start retry, type hints, jitter guard, restore-failure UI and JSON export');
})().catch(e=>{console.error(e);process.exitCode=1;});
