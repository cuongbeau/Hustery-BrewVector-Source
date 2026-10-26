const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const file=fs.readFileSync('hmi/src/web/web_js_dashboard.h','utf8');
const js=file.match(/R"JS\(([\s\S]*?)\)JS"/)[1];
const section=js.slice(js.indexOf('    var startBtn = el("btn_dash_start");'),js.indexOf('    var bsp = el("boiler_state_pill");'));
const nodes={};
function el(id){return nodes[id]??={disabled:false,textContent:'',style:{},attrs:{},classList:{toggle(){},add(){},remove(){}},setAttribute(k,v){this.attrs[k]=String(v)},getAttribute(k){return this.attrs[k]}};}
let commands=[];
const ctx=vm.createContext({document:{addEventListener(){}},el,console});
vm.runInContext(js,ctx);
vm.runInContext('sendDashCommand=(url,label)=>commands.push({url,label});var lastLiveState="IDLE";',Object.assign(ctx,{commands}));
function render(j,brew=false,flush=false){Object.assign(ctx,{j,isBrewing:brew,isFlush:flush});vm.runInContext(section,ctx);}
render({heaterFresh:true,heaterMode:0,heaterReady:false});
assert.equal(el('btn_dash_start_text').textContent,'PREHEAT');assert(!el('btn_dash_start').disabled);
vm.runInContext('dashStart()',ctx);assert.equal(commands[0].url,'/dash/start');assert.match(commands[0].label,/Preheat/);
render({heaterFresh:true,heaterMode:1,heaterReady:false});assert(el('btn_dash_start').disabled);
vm.runInContext('setDashCommandPending(false);dashStart()',ctx);assert(el('btn_dash_start').disabled);assert.equal(commands.length,1);
render({heaterFresh:true,heaterMode:1,heaterReady:true});assert.equal(el('btn_dash_start_text').textContent,'START BREW');
vm.runInContext('dashStart()',ctx);assert.equal(commands.length,2);
render({heaterFresh:false},true);assert(!el('btn_dash_start').disabled);vm.runInContext('dashStart()',ctx);assert.equal(commands.at(-1).url,'/dash/stop');
render({heaterFresh:false});assert(el('btn_dash_start').disabled);
console.log('PASS: actual dashboard Preheat/warming/Ready/offline/Stop and pending guard.');
