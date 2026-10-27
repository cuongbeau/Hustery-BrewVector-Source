const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');

const context={window:{addEventListener(){},matchMedia:()=>({matches:false})}};
vm.createContext(context);
vm.runInContext(fs.readFileSync('hmi/web/profiles-preview.js','utf8'),context);

const profile=JSON.parse(fs.readFileSync('profiles/Adaptive_v2_BrewVector_38g.json','utf8'));
const model=context.profilePreviewCurves(profile.phases);
assert.equal(model.total,57);
assert.equal(model.segments[0].flow.role,'target');
assert.equal(model.segments[0].flow.value,8);
assert.equal(model.segments[0].pressure.role,null);
assert.equal(model.segments[4].pressure.role,'target');
assert.equal(model.segments[4].pressure.value,9.5);
assert.equal(model.segments[4].flow.role,'limit');
assert.equal(model.segments[4].flow.value,3.5);
assert.equal(model.segments[5].flow.role,'target');
assert.equal(model.segments[5].flow.adaptive,true);
assert.equal(model.segments[5].flow.entry,3.5);
assert.equal(model.segments[5].flow.value,3.5);
assert.equal(model.segments[5].pressure.role,'limit');
assert.equal(model.segments[5].pressure.value,9.5);

const power=context.profilePreviewCurves([{name:'Power',duration:3,pump:42,targets:[]}]);
assert.equal(power.segments[0].power,42);
assert.equal(power.segments[0].pressure,null);
assert.equal(power.segments[0].flow,null);

const switched=context.profilePreviewCurves([
 {name:'P1',duration:2,pump:{target:'pressure',pressure:3,flow:0},transition:{type:'instant'},targets:[]},
 {name:'Power',duration:2,pump:40,transition:{type:'instant'},targets:[]},
 {name:'P2',duration:2,pump:{target:'pressure',pressure:9,flow:0},transition:{type:'linear',duration:1},targets:[]}
]);
assert.equal(switched.segments[2].pressure.entry,0);
assert.equal(switched.segments[2].pressure.points[0][1],0);

const unfinished=context.profileTargetPoints(0,5,0,9,{type:'linear',duration:30});
assert.equal(unfinished.at(-1)[0],5);
assert.equal(unfinished.at(-1)[1],1.5,'a 30 s ramp must not be compressed into a 5 s phase');
const implicit=context.profileTargetPoints(0,5,0,9,{type:'linear',duration:0});
assert.equal(implicit[0][1],0,'zero transition length uses phase duration, not an instant jump');
assert.equal(implicit.at(-1)[1],9);
console.log('PASS: Profile preview preserves target/limit semantics and does not compress ramps longer than phase duration.');
