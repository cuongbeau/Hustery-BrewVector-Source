const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const js=fs.readFileSync('hmi/src/web/web_js_dashboard.h','utf8').match(/R"JS\(([\s\S]*?)\)JS"/)[1];
const frames=[],draws=[];
const ctx=vm.createContext({document:{hidden:false,addEventListener(){}},shotSamples:[],
  requestAnimationFrame(fn){frames.push(fn);return frames.length;}});
vm.runInContext(js,ctx);
ctx.drawChart=(t,s)=>draws.push({t,s});
const row=(seq,t)=>[seq,t,seq,4,2,seq,90,0];
const merge=(samples,extra={})=>ctx.mergeBrewStream({streamBoot:1,streamSession:1,samples,...extra});
merge([row(1,0),row(2,.05),row(3,.1)]);
merge([row(2,.05),row(3,.1),row(4,.15)]);
assert.equal(ctx.shotSamples.length,4);assert.equal(ctx.brewStreamCursor,4);
const raw=JSON.stringify(ctx.shotSamples);
const visible=ctx.brewVisibleSamples(.075);
assert.equal(visible.length,3);assert.equal(visible[2].p,2.5);
assert.equal(JSON.stringify(ctx.shotSamples),raw);
assert.equal(ctx.brewVisibleSamples(9).at(-1).t,.15);
ctx.brewAnimating=true;ctx.startBrewAnimation();ctx.startBrewAnimation();assert.equal(frames.length,1);
frames.shift()(100);frames.shift()(150);
assert(draws.every(d=>d.t<=.15));
ctx.document.hidden=true;frames.shift()(200);assert.equal(frames.length,0);
merge([row(40,2)],{streamGap:true});assert.equal(ctx.shotSamples.length,1);
merge([row(41,0)],{streamSession:2});assert.equal(ctx.shotSamples.length,1);assert.equal(ctx.shotSamples[0].t,0);
merge([[42,.05,null,4,2,4,90,0]],{streamSession:2});assert.equal(ctx.brewStreamCursor,42);
merge([row(43,.1)],{streamSession:2});assert.equal(ctx.shotSamples.length,1);
// Execute the actual summary transition code: a final multi-page batch must be
// fully consumed before the result is frozen, even if the next state is Ready.
vm.runInContext(js.slice(js.indexOf('  function updateStateFromLive(j){'),js.indexOf('    var isIdleMode = summaryLatched;'))+'}',ctx);
let captures=0;
Object.assign(ctx,{shotWasActive:true,summaryLatched:false,lastShotSnapshot:null,
  captureShotSnapshot(){captures++;return null;},renderShotSummary(){}});
ctx.updateStateFromLive({state:'READY',streamBoot:1,streamSession:2,samples:[row(44,.15)],streamMore:true});
assert.equal(captures,0);ctx.shotWasActive=false;
ctx.updateStateFromLive({state:'READY',streamBoot:1,streamSession:2,samples:[row(45,.2)],streamMore:false});
assert.equal(captures,1);assert.equal(ctx.shotSamples.at(-1).t,.2);
console.log('PASS: ordered batches, duplicate suppression, session/gap recovery, invalid sample recovery, bounded animation and immutable raw measurements.');
