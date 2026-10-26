// Run: node tests/psm_reset/web_diagnostic_test.cjs (no board or firmware build).
const fs = require('node:fs');
const vm = require('node:vm');
const assert = require('node:assert/strict');
const path = require('node:path');
const root = path.resolve(__dirname, '../..');
const page = fs.readFileSync(path.join(root, 'hmi/src/web/routes/pump_diagnostic_routes.cpp'), 'utf8');
const script = page.match(/<script>([\s\S]*?)<\/script>/)[1];
const system = fs.readFileSync(path.join(root, 'hmi/src/web/routes/dashboard_routes.cpp'), 'utf8');
new vm.Script(system.match(/R"JS\(([\s\S]*?)\)JS"/)[1]);

async function scenario(name, replies, expected, expectedCalls) {
  const button = {disabled: false};
  const status = {textContent: ''};
  const calls = [];
  const context = {
    document: {getElementById: id => id === 'reset' ? button : status},
    setTimeout: callback => callback(),
    fetch: async (url, options = {}) => {
      assert.equal(button.disabled, true, 'button locked during request');
      calls.push(options.method || 'GET');
      const next = replies.shift();
      if (next instanceof Error) throw next;
      assert.ok(next, 'unexpected network call');
      return {ok: next.ok !== false, json: async () => next};
    }
  };
  vm.runInNewContext(script, context);
  assert.equal(calls.length, 0, 'loading page must not reset pump');
  await button.onclick();
  assert.ok(status.textContent.includes(expected), `${name}: ${status.textContent}`);
  assert.equal(button.disabled, false);
  assert.equal(calls.length, expectedCalls);
  assert.equal(calls.filter(x => x === 'POST').length, 1, 'never auto-retry reset');
  console.log('PASS', name);
}

(async () => {
  await scenario('ACK after pending', [{ticket:1,state:'pending'},{ticket:1,state:'applied'}], 'Main đã reset', 2);
  await scenario('Main rejects active machine', [{ticket:2,state:'pending'},{ticket:2,state:'rejected'}], 'Main từ chối', 2);
  await scenario('old Main firmware', [{ticket:3,state:'pending'},{ticket:3,state:'unsupported'}], 'chưa hỗ trợ', 2);
  await scenario('HTTP busy is not success', [{ok:false,state:'not_sent'}], 'Chưa gửi được', 1);
  await scenario('connection lost after POST', [{ticket:4,state:'pending'},new Error('offline')], 'Không xác định', 2);
  await scenario('another browser changed ticket', [{ticket:5,state:'pending'},{ticket:6,state:'applied'}], 'Không xác định', 2);
  await scenario('bounded wait', Array.from({length:17},()=>({ticket:7,state:'pending'})), 'Không xác định', 17);
  console.log('PASS System JS syntax');
})().catch(error => { console.error(error); process.exitCode=1; });
