const fs = require('node:fs'), vm = require('node:vm'), assert = require('node:assert/strict');
const js = fs.readFileSync('hmi/src/web/web_js_calib.h','utf8').match(/R"JS\(([\s\S]*?)\)JS"/)[1];
const elements = {}, requests = [];
const ctx = {Number, document:{hidden:false}, el(id){return elements[id] ||= {value:'',disabled:false,textContent:''};},
  httpGetJSON(url,ok,err){requests.push({url,ok,err});},
  httpPostJSON(url,data,ok,err){requests.push({url,data,ok,err});}};
vm.createContext(ctx); vm.runInContext(js,ctx);
ctx.saveThermal(); assert.equal(requests.length,0,'Cannot save an unloaded default');
ctx.loadThermal(); ctx.loadThermal(); assert.equal(requests.length,1,'Single flight');
requests.pop().ok({ok:true,offset_c:24.5});
assert.equal(elements.thermal_offset.value,'24.5');
for(const bad of ['', ' ', 'NaN','Infinity','-1','61']) {
  elements.thermal_offset.value=bad; ctx.saveThermal(); assert.equal(requests.length,0,'Reject '+bad);
}
elements.thermal_offset.value='0'; ctx.saveThermal();
let r=requests.pop(); assert.equal(r.data.offset_c,0); assert.equal(r.data.expected_offset_c,24.5);
assert.equal(elements.thermal_save.disabled,true); ctx.saveThermal(); assert.equal(requests.length,0);
r.ok({ok:true,offset_c:0}); assert.equal(elements.thermal_offset.value,'0.0');
elements.thermal_offset.value='60'; ctx.saveThermal();
r=requests.pop(); assert.equal(r.data.expected_offset_c,0);
r.err(new Error('Offset changed; reload before saving'));
assert.match(elements.thermal_msg.textContent,/Offset changed/);
ctx.saveThermal(); assert.equal(requests.length,0,'Conflict cannot silently retry');
ctx.loadThermal(); requests.pop().ok({ok:true,offset_c:22});
elements.thermal_offset.value='23'; ctx.saveThermal(); requests.pop().err(new Error('timeout'));
ctx.saveThermal(); assert.equal(requests.length,0,'Unknown save result requires reload');
ctx.loadThermal(); requests.pop().ok({ok:true,offset_c:23});
assert.equal(elements.thermal_offset.value,'23.0','Reload reconciles a save with lost ACK');
ctx.loadThermal(); requests.pop().ok({ok:true,offset_c:null});
assert.equal(elements.thermal_save.disabled,true,'Invalid reply cannot enable saving');
console.log('PASS: thermal Web load, bounds, zero, CAS baseline, single flight, conflict and timeout reconciliation.');
