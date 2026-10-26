const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const drain=async()=>{for(let i=0;i<24;i++)await Promise.resolve();};
function harness(){
  let now=0,serial=0;const timers=new Map(),requests=[];
  const env={AbortController,Promise,Number,Math,JSON,Error,encodeURIComponent,
    Date:class extends Date{static now(){return now;}},
    setTimeout(fn,ms){const id=++serial;timers.set(id,{fn,at:now+ms});return id;},
    clearTimeout(id){timers.delete(id);},
    fetch(url,options={}){return new Promise((resolve,reject)=>{
      const r={url,options,resolve,reject};requests.push(r);
      if(options.signal)options.signal.addEventListener('abort',()=>reject(new Error('aborted')));
    })},
  };
  const context=vm.createContext(env);
  return {context,requests,run:s=>vm.runInContext(s,context),
    async tick(ms){now+=ms;for(const [id,t]of [...timers])if(t.at<=now){timers.delete(id);t.fn();}await drain();},
    async reply(index,data,status=200){requests[index].resolve({ok:status<400,status,text:async()=>JSON.stringify(data),json:async()=>data});await drain();},
    timers,
  };
}
const core=fs.readFileSync('hmi/src/web/web_js_core.h','utf8').match(/R"JS\(([\s\S]*?)\)JS"/)[1];
function jobHarness(){const h=harness();h.run(core.slice(core.indexOf('  async function httpJSONRequest'),core.indexOf('  // ===== Tabs')));return h;}
function heaterHarness(){
  const h=harness(),elements=new Map();
  function el(id){id=id.startsWith('heat_')?id:'heat_'+id;if(!elements.has(id))elements.set(id,{id,textContent:'',className:'',value:'',disabled:false,append(){},getContext(){return {beginPath(){},moveTo(){},lineTo(){},stroke(){},clearRect(){},strokeRect(){},fillText(){},setLineDash(){}}},clientWidth:300});return elements.get(id);}
  h.context.document={hidden:false,getElementById:el,createElement:()=>({append(){}}),querySelectorAll:()=>[el('mut')]};
  const source=fs.readFileSync('hmi/web/heater.js','utf8')
    .replace('return {send,download,setActive};','return {send,download,setActive,poll};');
  h.run(source);h.run('const poll=HeaterTab.poll,send=HeaterTab.send;HeaterTab.setActive(true);poll()');h.el=el;return h;
}
(async()=>{
  {
    const h=jobHarness();let result;
    const work=h.run("httpJSONResult('/profile/save',{method:'POST',body:'{}'})").then(r=>result=r);
    await h.reply(0,{job:17},202);await h.tick(300);
    assert.equal(h.requests[1].url,'/api/hmi/job?id=17');
    await h.reply(1,{job:17},202);await h.tick(300);
    await h.reply(2,{ok:true,index:3});await work;
    assert.equal(result.index,3);assert.equal(h.requests.filter(r=>r.options.method==='POST').length,1);
  }
  {
    const h=jobHarness();let error;
    const work=h.run("httpJSONResult('/profile/save',{method:'POST'})").catch(e=>error=e);
    await h.tick(8000);await work;
    assert.match(error.message,/Không tự gửi lại/);assert(h.requests[0].options.signal.aborted);
    await h.reply(0,{job:99},202);await h.tick(1000);assert.equal(h.requests.length,1);
  }
  {
    const h=jobHarness();let error;
    const work=h.run("httpJSONResult('/profiles')").catch(e=>error=e);
    await h.reply(0,{error:'Main jobs busy'},409);await work;
    assert.equal(error.message,'Main jobs busy');assert.equal(h.requests.length,1);
  }
  {
    // AbortSignal.timeout is deliberately absent. A 4-second response still renders,
    // while the 2.5-second stale warning disables controls during the wait.
    const h=heaterHarness();await drain();
    h.run('poll();poll()');assert.equal(h.requests.length,1);
    await h.tick(2500);assert.match(h.el('alarm').textContent,/ĐANG CHỜ/);assert(h.el('mut').disabled);
    await h.tick(1500);await h.reply(0,{online:false});
    assert.match(h.el('alarm').textContent,/MẤT LIÊN LẠC/);
    await h.tick(500);assert.equal(h.requests.length,2);
    await h.tick(8000);assert(h.requests[1].options.signal.aborted);
    assert.match(h.el('alarm').textContent,/MẤT KẾT NỐI WEB/);
  }
  {
    const h=heaterHarness();let error;
    const command=h.run('send(0)').catch(e=>error=e);await drain();
    assert(h.requests[0].options.signal.aborted,'Command must preempt background GET');
    assert.equal(h.requests[1].options.method,'POST');
    await h.tick(8000);await command;
    assert.equal(error,undefined);assert.match(h.el('reply').textContent,/chưa xác nhận/);
    assert.equal(h.requests.filter(r=>r.options.method==='POST').length,1);
  }
  {
    const h=heaterHarness();await h.reply(0,{online:true,filtered:70});
    assert.match(h.el('alarm').textContent,/DỮ LIỆU WEB LỖI/);
    assert(h.el('mut').disabled,'Malformed response must not enable mutations');
  }
  {
    const h=harness();Object.assign(h.context,{TextDecoder,Uint8Array,window:{addEventListener(){}}});
    const source=fs.readFileSync('hmi/src/web/web_js_history.h','utf8').match(/R"JS\(([\s\S]*?)\)JS"/)[1];
    h.run(source);h.run('hmiHistGeneration=1');
    const bytes=Buffer.from('AưB\n'),chunks=[bytes.subarray(0,2),bytes.subarray(2)];let result;
    const work=h.run('histReadCsv(7,1)').then(v=>result=v);
    for(let i=0;i<chunks.length;i++){
      const part=chunks[i];
      h.requests[i].resolve({ok:true,headers:{get:()=>String(bytes.length)},arrayBuffer:async()=>part.buffer.slice(part.byteOffset,part.byteOffset+part.length)});
      await drain();await h.tick(20);
    }
    await work;assert.equal(result,'AưB\n','UTF-8 split across chunks must survive');
    assert.equal(h.requests[1].url,'/traces/download?id=7&offset=2');
    const cancelled=h.run('histReadCsv(8,0)');assert.equal(await cancelled,null);assert.equal(h.requests.length,2);
  }
  console.log('PASS: job polling sends POST once; request deadline/late result; heater startup, slow reads, stale controls, command priority and invalid data.');
})().catch(e=>{console.error(e);process.exitCode=1;});
