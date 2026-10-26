const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const source = fs.readFileSync('hmi/web/profiles.js', 'utf8');
const core = fs.readFileSync('hmi/src/web/web_js_core.h', 'utf8');
const context = {
  TextEncoder,
  crypto: { randomUUID: () => 'profile-id' },
  document: { getElementById: () => null },
  window: { addEventListener() {}, innerWidth: 1024, matchMedia: () => ({ matches: false }) },
  confirm: () => true,
};
vm.createContext(context);
vm.runInContext(source, context);

context.profileDirty = true;
context.profileDraft = { label: 'Unsaved' };
context.selectedIdx = 4;
context.profilePhase = 2;
context.confirm = () => false;
assert.equal(context.profileLeaveProfiles(), false);
assert.equal(context.profileDraft.label, 'Unsaved');
assert.equal(context.profileDirty, true);

context.confirm = () => true;
assert.equal(context.profileLeaveProfiles(), true);
assert.equal(context.profileDraft, null);
assert.equal(context.profileDirty, false);
assert.equal(context.selectedIdx, -1);
assert.equal(context.profilePhase, 0);

context.profileBusy = true;
assert.equal(context.profileLeaveProfiles(), false);
assert.match(core, /name!=="prof" && typeof profileLeaveProfiles==="function" && !profileLeaveProfiles\(\)/);

console.log('PASS: unsaved profile changes block tab navigation and clear only after confirmation.');
