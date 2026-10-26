// Executes the actual embedded page script with a fake UART/HTTP peer and DOM.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const {webcrypto} = require('node:crypto');
const source = fs.readFileSync(path.join(__dirname, '../../hmi/src/web/routes/pressure_diagnostic_routes.cpp'), 'utf8');
const script = source.match(/<script>([\s\S]*?)<\/script>/)[1];
const buttons = ['arm', 'cancel', 'download', 'clear'];
const nodes = Object.fromEntries([...buttons, 'status', 'progress'].map(id => [id, {disabled: false, textContent: ''}]));
let meta = {capture_id:0, version:1, status:0, result:0, end:0, fast_active:false, total:0,
  started_at_ms:100, busy_polls:0, pressure_offset:0.5, pressure_scale:5, poll_ms:3,
  publish_ms:30, mean_window:10, duration_ms:15000, capacity:5120};
let reply, corrupt = false, dropArm = false, duplicateArms = 0, armExecutions = 0;
const blobs = [], offsets = [], ids = [];
const response = (status, data) => ({ok:status>=200&&status<300, status, json:async()=>data});
const context = vm.createContext({console, Blob, crypto:webcrypto,
  setTimeout:fn=>{queueMicrotask(fn);return 1;}, setInterval:()=>1,
  URL:{createObjectURL:b=>{blobs.push(b);return 'blob:test';},revokeObjectURL:()=>{}},
  document:{getElementById:id=>nodes[id],querySelectorAll:()=>buttons.map(id=>nodes[id]),
    body:{appendChild:()=>{}},createElement:()=>({click(){},remove(){}})},
  fetch:async(url, options={})=>{
    if(options.method==='POST'){
      const r=JSON.parse(options.body);ids.push(r.request_id);
      if(r.op===1){
        if(meta.capture_id!==r.capture_id){armExecutions++;meta={...meta,capture_id:r.capture_id,status:1,total:0};}
        else duplicateArms++;
      }
      if(r.op===3)meta={...meta,status:4};
      if(r.op===4)meta={...meta,capture_id:0,total:0,status:0};
      reply={...meta,request_id:r.request_id,op:r.op,offset:r.offset,rows:[]};
      if(r.op===2){
        offsets.push(r.offset);
        if(meta.fast_active)reply.result=1;
        else if(r.capture_id!==meta.capture_id)reply.result=5;
        else for(let i=r.offset;i<Math.min(r.offset+20,meta.total);i++)reply.rows.push([i*3000,8000,Math.min(i+1,10),i%10===0?1:0,8000,2,100]);
        if(corrupt)reply.total++;
      }
      if(r.op===1&&dropArm){dropArm=false;return response(503,{error:'Lost HTTP acknowledgement'});}
      return response(202,{pending:true});
    }
    return response(200,reply);
  }
});
(async()=>{
  vm.runInContext(script,context);
  await new Promise(setImmediate);
  assert.equal(nodes.download.disabled,true);
  dropArm=true;
  await nodes.arm.onclick();
  assert.equal(armExecutions,1,'Lost acknowledgement must not create a second capture');
  assert.equal(duplicateArms,1);
  assert.equal(ids.at(-1),ids.at(-2),'Retry must preserve request ID');
  meta={...meta,status:3,fast_active:true,total:45};
  await vm.runInContext('refresh()',context);
  assert.equal(nodes.download.disabled,true,'Cannot download while fast acquisition is still active');
  meta={...meta,fast_active:false};
  await vm.runInContext('refresh()',context);
  assert.equal(nodes.download.disabled,false);
  await nodes.download.onclick();
  assert.deepEqual(offsets,[0,20,40]);
  assert.equal(blobs.length,1);
  const csv=await blobs[0].text();
  assert.equal(csv.trim().split('\n').length,47,'Metadata + header + all 45 samples');
  assert.match(csv,/raw_bar_signed,mean_bar_signed/);
  assert.equal(csv.trim().split('\n')[2].split(',').at(-1),'2.5');
  corrupt=true;
  await nodes.download.onclick();
  assert.equal(blobs.length,1,'Changed/truncated capture must never download partial CSV');
  assert.match(nodes.status.textContent,/Đứt hoặc thay bản ghi/);
  corrupt=false;
  await nodes.clear.onclick();
  assert.equal(nodes.download.disabled,true);
  console.log('PASS: real page retry IDs, fast-active guard, ordered chunks, calibrated CSV, mixed-capture rejection and clear');
})().catch(e=>{console.error(e);process.exitCode=1;});
