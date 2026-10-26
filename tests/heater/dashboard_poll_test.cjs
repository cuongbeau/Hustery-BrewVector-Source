const fs=require('node:fs'),vm=require('node:vm'),assert=require('node:assert/strict');
const js=fs.readFileSync('hmi/src/web/web_js_dashboard.h','utf8').match(/R"JS\(([\s\S]*?)\)JS"/)[1];
function harness(){
  let now=0,next=1,profiles=0;
  const timers=new Map(),requests=[],listeners={},renders=[],wifi=[],nodes={};
  const document={hidden:false,addEventListener(name,fn){listeners[name]=fn;}};
  const ctx=vm.createContext({document,window:{scrollTo(){}},AbortController,Date:{now:()=>now},
    el(id){return nodes[id]??={disabled:false};},notifyUi(){},
    setTimeout(fn,ms){const id=next++;timers.set(id,{at:now+ms,fn});return id;},
    clearTimeout(id){timers.delete(id);},
    fetch(url,options={}){
      let resolve,reject;
      const promise=new Promise((yes,no)=>{resolve=yes;reject=no;});
      const req={url,options,resolve,reject,aborted:false};requests.push(req);
      if(options.signal)options.signal.addEventListener('abort',()=>{
        req.aborted=true;reject(new Error('aborted'));
      });
      return promise;
    },updateWifiStatus(ok){wifi.push(ok);},updateConnectionState(){}});
  vm.runInContext(js,ctx);
  Object.assign(ctx,{fetchDashProfiles(){profiles++;},updateStateFromLive(j){renders.push(j);},
    liveTimer:null,liveFailed:0});
  const run=s=>vm.runInContext(s,ctx);
  const drain=async()=>{for(let i=0;i<12;i++)await Promise.resolve();};
  async function advance(ms){
    const end=now+ms;
    for(;;){
      const due=[...timers].filter(([,t])=>t.at<=end).sort((a,b)=>a[1].at-b[1].at)[0];
      if(!due)break;
      now=due[1].at;timers.delete(due[0]);due[1].fn();await drain();
    }
    now=end;await drain();
  }
  return {run,advance,drain,requests,renders,wifi,ctx,document,listeners,
    profiles:()=>profiles,
    reply(req,data={},ok=true){req.resolve({ok,status:ok?200:503,json:async()=>data,text:async()=>'OK'});}};
}
(async()=>{
  // Active reads target 100 ms start-to-start, with a yield after slow responses.
  let fast=harness();fast.run('startLive()');await fast.advance(60);
  fast.reply(fast.requests[0],{state:'PROFILE_RUN'});await fast.drain();
  await fast.advance(39);assert.equal(fast.requests.length,1);
  await fast.advance(1);assert.equal(fast.requests.length,2);
  await fast.advance(400);assert.equal(fast.requests.length,2);
  fast.reply(fast.requests[1],{state:'PROFILE_RUN'});await fast.drain();
  await fast.advance(19);assert.equal(fast.requests.length,2);
  await fast.advance(1);assert.equal(fast.requests.length,3);
  fast.ctx.brewStreamBoot=7;fast.ctx.brewStreamSession=2;fast.ctx.brewStreamCursor=80;
  fast.reply(fast.requests[2],{streamMore:true});await fast.drain();await fast.advance(100);
  assert.equal(fast.requests[3].url,'/dash/live?boot=7&session=2&after=80');

  // A slow request never creates a backlog, including duplicate startup calls.
  let h=harness();h.run('startLive();startLive()');
  assert.equal(h.profiles(),1);assert.equal(h.requests.length,1);
  await h.advance(4000);assert.equal(h.requests.length,1);
  for(let i=0;i<16;i++)h.run('pollLive()');assert.equal(h.requests.length,1);
  h.reply(h.requests[0],{t:4});await h.drain();assert.equal(h.renders.length,1);
  await h.advance(249);assert.equal(h.requests.length,1);
  await h.advance(1);assert.equal(h.requests.length,2);

  // Commands cancel reads, bypass the read schedule, and are sent only once.
  h.run('sendDashCommand("/dash/start");sendDashCommand("/dash/start")');
  assert(h.requests[1].aborted);assert.equal(h.requests[2].url,'/dash/start');
  await h.drain();assert.equal(h.ctx.liveFailed,0);
  await h.advance(4000);assert.equal(h.requests.length,3);
  h.reply(h.requests[2]);await h.drain();await h.advance(450);
  assert.equal(h.requests[3].url,'/dash/live');
  assert.equal(h.requests.filter(r=>r.url==='/dash/start').length,1);

  // Commands that fail are never automatically resent; live reads resume.
  h.run('sendDashCommand("/dash/stop")');await h.drain();
  h.requests.at(-1).reject(new Error('network'));await h.drain();await h.advance(450);
  assert.equal(h.requests.at(-1).url,'/dash/live');
  assert.equal(h.requests.filter(r=>r.url==='/dash/stop').length,1);

  // Timeout aborts a stalled read and allows a new bounded read.
  h=harness();h.run('startLive()');await h.advance(8000);
  assert(h.requests[0].aborted);assert.equal(h.ctx.liveFailed,1);
  await h.advance(250);assert.equal(h.requests.length,2);
  h.reply(h.requests[1],{},false);await h.drain();assert.equal(h.ctx.liveFailed,2);
  await h.advance(250);h.reply(h.requests[2],{t:9});await h.drain();
  assert.equal(h.ctx.liveFailed,0);assert.equal(h.renders.length,1);

  // Hidden tabs read at 5 seconds; restoring visibility refreshes immediately.
  h=harness();h.document.hidden=true;h.run('startLive()');
  await h.advance(4999);assert.equal(h.requests.length,0);
  await h.advance(1);assert.equal(h.requests.length,1);
  h.reply(h.requests[0]);await h.drain();
  await h.advance(1000);assert.equal(h.requests.length,1);
  h.document.hidden=false;h.listeners.visibilitychange();await h.advance(0);
  assert.equal(h.requests.length,2);

  // A cancelled read whose body completes late cannot overwrite command state.
  h=harness();h.run('startLive()');let finishBody;
  h.requests[0].resolve({ok:true,json:()=>new Promise(resolve=>{finishBody=resolve;})});
  await h.drain();h.run('sendDashCommand("/dash/start")');
  finishBody({t:1});await h.drain();assert.equal(h.renders.length,0);
  assert.equal(h.ctx.liveFailed,0);
  console.log('PASS: bounded single live read, startup, command priority/no retry, recovery, hidden tab and late response.');
})().catch(e=>{console.error(e);process.exitCode=1;});
