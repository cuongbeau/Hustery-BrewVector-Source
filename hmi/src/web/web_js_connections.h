#pragma once
#include <Arduino.h>
// Generated from hmi/web/connections.js
static const char WEB_JS_CONNECTIONS[] PROGMEM = R"JS(
const ConnectionsTab=(()=>{
 let active=false,timer=0,busy=false,last=null;
 const setText=(id,text,cls)=>{const n=el(id);if(!n)return;n.textContent=text;n.className=cls||'';};
 function signal(rssi){return rssi>=-60?'Tốt':rssi>=-75?'Trung bình':'Yếu';}
 function render(s){
  last=s;
  setText('conn_wifi_state',s.wifi.connected?'ONLINE':'OFFLINE',s.wifi.connected?'status-ok':'status-bad');
  setText('conn_main_state',!s.main.online?'OFFLINE':s.main.compatible?'CONNECTED':'PROTOCOL ERROR',s.main.online&&s.main.compatible?'status-ok':s.main.online?'status-warn':'status-bad');
  setText('conn_scale_state',s.scale.scanning?'SCANNING':s.scale.connected?'CONNECTED':'OFFLINE',s.scale.connected?'status-ok':s.scale.scanning?'status-warn':'status-bad');
  setText('conn_wifi_detail',s.wifi.connected?`${s.wifi.ssid} · ${s.wifi.ip} · ${s.wifi.rssi} dBm (${signal(s.wifi.rssi)})`:`${s.wifi.status} · AP ${s.wifi.apName} (${s.wifi.apIp})`);
  const auto=el('conn_wifi_auto');if(auto&&document.activeElement!==auto)auto.checked=!!s.wifi.autoConnect;
  setText('conn_scale_detail',s.scale.connected?`${s.scale.name||'BLE Scale'} · ${s.scale.mac||'--'} · ${Number(s.scale.weight||0).toFixed(1)} g · ${s.scale.stable?'ổn định':'đang thay đổi'}${s.scale.battery===null?'':' · pin '+s.scale.battery+'%'}`:s.scale.savedMac?`Đã lưu ${s.scale.savedMac}; đang chờ kết nối.`:'Chưa ghép nối cân.');
  const box=el('conn_scale_devices');if(!box)return;
  box.innerHTML=s.scale.devices.length?s.scale.devices.map(d=>`<div class="device-row"><div><b>${escHtml(d.name||'BLE Scale')}</b><small>${escHtml(d.mac)}</small></div><span>${d.rssi} dBm</span><button onclick="ConnectionsTab.pair('${escHtml(d.mac)}')">Ghép nối</button></div>`).join(''):'<div class="small">'+(s.scale.scanning?'Đang quét cân xung quanh…':'Chưa tìm thấy cân.')+'</div>';
 }
 async function refresh(){
  if(!active)return;
  try{const s=await httpJSONResult('/api/connections');render(s);}catch(e){setText('conn_main_state','KHÔNG ĐỌC ĐƯỢC','status-bad');}
  clearTimeout(timer);if(active)timer=setTimeout(refresh,1000);
 }
 async function action(op,data={}){
  if(busy)return;busy=true;
  try{const r=await httpJSONResult('/api/connections/action',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({op,...data})});return r;}
  catch(e){throw e;}finally{busy=false;}
 }
 async function scanWifi(){
  const btn=el('conn_wifi_scan'),msg=el('conn_wifi_msg');btn.disabled=true;msg.textContent='Đang quét Wi-Fi…';
  try{let result=await httpJSONResult('/sys/wifi_scan');while(result.scanning){await new Promise(r=>setTimeout(r,700));result=await httpJSONResult('/sys/wifi_scan?poll=1');}
   const list=el('conn_wifi_list');list.innerHTML='<option value="">-- Chọn mạng --</option>';
   result.networks.forEach(n=>{const o=document.createElement('option');o.value=n.ssid;o.textContent=`${n.ssid} (${n.rssi} dBm${n.enc?' · khóa':''})`;list.appendChild(o);});msg.textContent=result.networks.length?'Đã cập nhật danh sách mạng.':'Không tìm thấy mạng.';
  }catch(e){msg.textContent=e.message||'Quét Wi-Fi thất bại.';}finally{btn.disabled=false;}
 }
 async function saveWifi(){const ssid=el('conn_wifi_ssid').value.trim(),pass=el('conn_wifi_pass').value,msg=el('conn_wifi_msg');if(!ssid){msg.textContent='Cần nhập SSID.';return;}msg.textContent='Đang lưu và kết nối…';try{await httpJSONResult('/sys/wifi_save',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({ssid,pass})});msg.textContent='Đã lưu. Kết nối web có thể đổi sang IP mới.';}catch(e){msg.textContent=e.message||'Không lưu được Wi-Fi.';}}
 async function forgetWifi(){if(!confirm('Quên mạng Wi-Fi đã lưu? Kết nối hiện tại sẽ bị ngắt.'))return;try{await action('wifi_forget');setText('conn_wifi_msg','Đã quên mạng; dùng AP dự phòng để kết nối lại.');}catch(e){setText('conn_wifi_msg',e.message);}}
 async function scanScale(){try{await action('scale_scan');setText('conn_scale_msg','Đang quét cân trong 5 giây…');refresh();}catch(e){setText('conn_scale_msg',e.message);}}
 async function pair(mac){try{await action('scale_pair',{mac});setText('conn_scale_msg','Đã lưu cân; đang kết nối…');refresh();}catch(e){setText('conn_scale_msg',e.message);}}
 async function forgetScale(){if(!confirm('Quên cân BLE đã lưu?'))return;try{await action('scale_forget');setText('conn_scale_msg','Đã quên cân.');refresh();}catch(e){setText('conn_scale_msg',e.message);}}
 async function tare(){try{await action('scale_tare');setText('conn_scale_msg','Đã gửi lệnh Tare.');}catch(e){setText('conn_scale_msg',e.message);}}
 function setActive(value){active=!!value;clearTimeout(timer);if(active)refresh();}
 document.addEventListener('change',e=>{if(e.target.id==='conn_wifi_list'&&e.target.value)el('conn_wifi_ssid').value=e.target.value;if(e.target.id==='conn_wifi_auto')action('wifi_auto',{enabled:e.target.checked}).catch(x=>setText('conn_wifi_msg',x.message));});
 return {setActive,scanWifi,saveWifi,forgetWifi,scanScale,pair,forgetScale,tare};
})();

const SystemInfoTab=(()=>{
 let active=false,timer=0;
 function box(label,value,cls=''){return `<div><span>${label}</span><b class="${cls}">${value}</b></div>`;}
 async function refresh(){if(!active)return;try{const s=await httpJSONResult('/api/hmi/diagnostics');el('system_health').innerHTML=box('HMI',`UP ${Math.floor(s.uptimeMs/1000)} s · Heap ${Math.round(s.freeHeap/1024)} KB`,'status-ok')+box('Main',s.mainOnline?'CONNECTED':'OFFLINE',s.mainOnline?'status-ok':'status-bad')+box('TF storage',s.tfStatus||'UNKNOWN',s.tfStatus==='OK'?'status-ok':'status-warn');el('system_diagnostics').textContent=JSON.stringify(s,null,2);}catch(e){el('system_diagnostics').textContent=e.message||'Không đọc được thông tin hệ thống.';}clearTimeout(timer);if(active)timer=setTimeout(refresh,5000);}
 async function restart(){if(!confirm('Khởi động lại HMI ngay? Kết nối web sẽ gián đoạn trong vài giây.'))return;clearTimeout(timer);active=false;try{await httpJSONResult('/api/hmi/restart',{method:'POST'});alert('HMI đang khởi động lại. Tải lại trang sau vài giây.');}catch(e){alert(e.message||'Không gửi được lệnh khởi động lại HMI.');}}
 function setActive(value){active=!!value;clearTimeout(timer);if(active)refresh();}
 return {setActive,restart};
})();

)JS";
