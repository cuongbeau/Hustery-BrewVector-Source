// File: src/ui/web_js_core.h
// JS helpers + Tabs + WiFi banner + các biến global dùng chung
// Refactor: 2026-04-20

#ifndef WEB_JS_CORE_H
#define WEB_JS_CORE_H

#include <Arduino.h>

static const char WEB_JS_CORE[] PROGMEM = R"JS(
  // ===== Helpers =====
  function el(id){ return document.getElementById(id); }
  function escHtml(value){
    return String(value==null?'':value).replace(/[&<>"']/g,function(ch){
      return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[ch];
    });
  }
  function notifyUi(message, ok){
    var toast=el('pf_toast');
    if(!toast)return;
    toast.textContent=message;
    toast.style.background=ok===false?'var(--danger)':(ok===true?'var(--ok)':'var(--accent)');
    toast.style.opacity='1';
    clearTimeout(toast._uiTimer);
    toast._uiTimer=setTimeout(function(){toast.style.opacity='0';},2600);
  }
  function fmtTime(ts){
    if(!ts) return "—";
    var d=new Date(ts);
    return d.toLocaleTimeString(undefined,{hour12:false});
  }
  async function httpJSONRequest(url, options){
    const controller=typeof AbortController==='function'?new AbortController():null;
    let timer;
    const timeout=new Promise((_,reject)=>{timer=setTimeout(()=>{if(controller)controller.abort();reject(new Error('Hết thời gian chờ; kết quả lệnh chưa xác nhận. Không tự gửi lại.'));},8000);});
    try{
      return await Promise.race([timeout,(async()=>{
        const r=await fetch(url,Object.assign({cache:'no-store'},options||{},controller?{signal:controller.signal}:{}));
        const txt=await r.text();let data;
        try{data=JSON.parse(txt||'{}');}catch(_){throw new Error('Invalid response');}
        if(!r.ok){const failure=new Error(data.error||('HTTP '+r.status));failure.data=data;throw failure;}
        return {status:r.status,data};
      })()]);
    }finally{clearTimeout(timer);}
  }
  async function httpJSONResult(url,options){
    let r=await httpJSONRequest(url,options);
    const started=Date.now();
    while(r.status===202 && r.data.job){
      if(Date.now()-started>60000)throw new Error('Main chưa xác nhận kết quả; không tự gửi lại lệnh.');
      const id=r.data.job;
      await new Promise(resolve=>setTimeout(resolve,300));
      r=await httpJSONRequest('/api/hmi/job?id='+encodeURIComponent(id));
    }
    return r.data;
  }
  function httpGetJSON(url,ok,err){httpJSONResult(url).then(ok).catch(e=>{if(err)err(e);});}
  function httpPostJSON(url,data,ok,err){
    httpJSONResult(url,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data||{})})
      .then(ok).catch(e=>{if(err)err(e);});
  }

  // ===== Tabs =====
  function switchTab(name){
    if(name!=="prof" && typeof profileLeaveProfiles==="function" && !profileLeaveProfiles()) return false;
    if(name!=="hist" && typeof pauseHmiHistory==="function")pauseHmiHistory();
    ["dash","prof","hist","cal","heat","conn","sys"].forEach(function(k){
      var tab = document.querySelector('.tab[data-tab="'+k+'"]');
      var page = el("page-"+k);
      if(!tab || !page) return;
      var act = (k===name);
      tab.classList.toggle("active", act);
      tab.setAttribute('aria-selected',act?'true':'false');
      tab.setAttribute('tabindex',act?'0':'-1');
      page.classList.toggle("active", act);
    });

    if(name==="cal"){
      if(!_statusTimer){
        refreshStatus();
        _statusTimer=setInterval(refreshStatus,1000);
      }
      loadPress();
      loadFlow();
      loadThermal();
    }else{
      if(_statusTimer){ clearInterval(_statusTimer); _statusTimer=null; }
    }

    if(name==="prof"){
      loadProfiles();
    }
    if(name==="hist" && typeof loadHmiHistory==="function"){
      loadHmiHistory();
    }
    if(typeof HeaterTab!=="undefined")HeaterTab.setActive(name==="heat");
    if(typeof ConnectionsTab!=="undefined")ConnectionsTab.setActive(name==="conn");
    if(typeof SystemInfoTab!=="undefined")SystemInfoTab.setActive(name==="sys");
    if(history && history.replaceState){
      history.replaceState(null,"",name==="dash"?location.pathname:(location.pathname+"#"+(name==="heat"?"heater":name==="conn"?"connections":name)));
    }
    return true;
  }

  document.addEventListener("click", function(e){
    var t = e.target.closest(".tab[data-tab]");
    if(!t) return;
    var name = t.getAttribute("data-tab");
    switchTab(name);
  });
  document.addEventListener('keydown',function(e){
    var current=e.target.closest('.tab[data-tab]');
    if(!current || (e.key!=='ArrowRight' && e.key!=='ArrowLeft'))return;
    var tabs=Array.prototype.slice.call(document.querySelectorAll('.tab[data-tab]'));
    var idx=tabs.indexOf(current)+(e.key==='ArrowRight'?1:-1);
    idx=(idx+tabs.length)%tabs.length;
    tabs[idx].focus();switchTab(tabs[idx].getAttribute('data-tab'));e.preventDefault();
  });

  // ===== WiFi banner =====
  function updateWifiStatus(ok){
    var dot = el("wifi_dot");
    var txt = el("wifi_text");
    var banner = el("offline_banner");
    if(ok){
      if(dot) dot.classList.add("ok");
      if(txt) txt.textContent = "Connected";
      if(el('web_connection')){el('web_connection').classList.add('ok');el('web_connection').classList.remove('bad');}
      if(banner) banner.classList.remove("show");
    }else{
      if(dot) dot.classList.remove("ok");
      if(txt) txt.textContent = "Disconnected";
      if(el('web_connection')){el('web_connection').classList.remove('ok');el('web_connection').classList.add('bad');}
      if(banner) banner.classList.add("show");
    }
  }

  // ===== Profile data for chart =====
  var profilePointsByName = {};
  var currentProfileName = "";

  // ===== Dashboard: main chart globals =====
  var chartCanvas = null, chartCtx = null;
  var shotSamples = [];
  var lastWasBrewing = false;
  var lastLiveTs = 0;
  var liveTimer = null;
  var liveFailed = 0;

  var seriesVisible = {
    pressure:true,
    target:true,
    flow:true,
    weight:true,
    temp:true
  };
)JS";

#endif // WEB_JS_CORE_H
