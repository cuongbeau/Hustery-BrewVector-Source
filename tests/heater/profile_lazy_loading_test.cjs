const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const nodes = {
  profiles_workspace: { style: {}, innerHTML: '' },
  profile_list: { innerHTML: '' },
  profile_status: { textContent: '' },
};
const calls = [];
const summary = {
  ok: true,
  profiles: [{
    index: 0,
    runtimeIndex: 2,
    label: 'Adaptive',
    type: 'pro',
    phaseCount: 6,
    selected: true,
  }],
};
const detail = {
  ok: true,
  index: 0,
  profile: {
    id: 'adaptive',
    label: 'Adaptive',
    type: 'pro',
    description: 'Loaded only when needed',
    phases: [{ name: 'Brew' }],
  },
};
const context = {
  console,
  profileStyles: '',
  document: { getElementById: id => nodes[id] || null },
  window: { addEventListener() {}, innerWidth: 1024 },
  confirm: () => true,
  httpGetJSON(url, ok) {
    calls.push(url);
    ok(url.startsWith('/profile/detail') ? detail : summary);
  },
};
vm.createContext(context);
vm.runInContext(fs.readFileSync('hmi/web/profiles.js', 'utf8'), context);

context.loadProfiles();
assert.deepEqual(calls, ['/profiles']);
assert.equal(context.cacheProfiles[0]._loaded, false);
assert.equal(context.cacheProfiles[0].phaseCount, 6);

let loaded;
context.profileLoadDetail(0, profile => { loaded = profile; });
assert.deepEqual(calls, ['/profiles', '/profile/detail?index=0']);
assert.equal(loaded._loaded, true);
assert.equal(loaded.description, 'Loaded only when needed');

context.profileLoadDetail(0, () => {});
context.loadProfiles();
assert.equal(calls.length, 2, 'RAM/browser cache should avoid repeated UART requests');

const clean = context.profileCanonical(loaded);
assert.equal(clean._loaded, undefined);
assert.equal(clean.runtimeIndex, undefined);
assert.equal(clean.phases.length, 1);

context.loadProfiles(true);
assert.equal(calls.at(-1), '/profiles', 'Tải lại must force a fresh summary request');

console.log('PASS: profile list is summary-only; detail is lazy-loaded, cached and force-refreshable.');
