#include "web/hmi_web_internal.h"
#include "communication/main_link.h"
#include <ArduinoJson.h>
using namespace HmiWebInternal;
namespace {
AdcProbeWire::Request pending{};
uint32_t sentMs = 0;
void failure(int code, const char* reason) {
  JsonDocument doc; doc["error"] = reason;
  String body; serializeJson(doc, body); server.send(code, "application/json", body);
}
void submit() {
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) || !doc["request_id"].is<uint32_t>() ||
      !doc["run_id"].is<uint32_t>() || !doc["op"].is<uint8_t>() ||
      doc["op"].as<uint8_t>() > 2 || doc["request_id"].as<uint32_t>() == 0) {
    failure(400, "Invalid request"); return;
  }
  if (!webMainLink || !webMainLink->connected() || !webMainLink->supportsAdcProbe()) {
    failure(503, "Main chưa hỗ trợ kiểm tra ADC hoặc mất kết nối"); return;
  }
  if (pending.requestId && webMainLink->adcProbeReply().requestId != pending.requestId && millis() - sentMs < 2000U) {
    failure(409, "Đang chờ yêu cầu trước"); return;
  }
  AdcProbeWire::Request req{doc["request_id"], doc["run_id"],
      static_cast<AdcProbeWire::Op>(doc["op"].as<uint8_t>()), AdcProbeWire::VERSION};
  if (!webMainLink->requestAdcProbe(req)) { failure(503, "Không gửi được"); return; }
  pending = req; sentMs = millis();
  server.send(202, "application/json", "{\"pending\":true}");
}
void status() {
  server.sendHeader("Cache-Control", "no-store");
  if (!pending.requestId || server.arg("id") != String(pending.requestId)) { failure(409, "Yêu cầu đã thay đổi"); return; }
  if (!webMainLink || !webMainLink->connected()) { failure(503, "Main mất kết nối"); return; }
  const auto& r = webMainLink->adcProbeReply();
  if (r.requestId != pending.requestId) {
    if (millis() - sentMs >= 2000U) failure(408, "Main chưa trả lời");
    else server.send(202, "application/json", "{\"pending\":true}");
    return;
  }
  JsonDocument doc;
  doc["request_id"] = r.requestId; doc["run_id"] = r.runId; doc["version"] = r.version;
  doc["state"] = static_cast<uint8_t>(r.state); doc["error"] = static_cast<uint8_t>(r.error);
  doc["result"] = static_cast<uint8_t>(r.result); doc["pump_held"] = bool(r.pumpHeld);
  doc["started_ms"] = r.startedMs; doc["max_poll_gap_us"] = r.maxPollGapUs;
  doc["saved_config"] = r.savedConfig; doc["restored_config"] = r.restoredConfig;
  JsonArray samples = doc["samples"].to<JsonArray>();
  for (uint8_t i = 0; i < r.count; ++i) {
    auto row = samples.add<JsonArray>(); const auto& s = r.samples[i];
    row.add(s.rateCode); row.add(s.readyUs); row.add(s.lastBusyUs); row.add(s.raw); row.add(s.lowNibble);
  }
  String body; serializeJson(doc, body); server.send(200, "application/json", body);
}
const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="vi"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Kiểm tra ADC</title>
<style>body{font:16px system-ui;max-width:850px;margin:24px auto;padding:0 18px;background:#15171b;color:#eee;line-height:1.6}a{color:#83caff}button{font:inherit;padding:12px;margin:5px}table{border-collapse:collapse;width:100%}td,th{padding:8px;border-bottom:1px solid #555;text-align:left}pre{white-space:pre-wrap}</style>
<a href="/diagnostics/pressure">Về chẩn đoán áp</a><h1>Kiểm tra ADC khi bơm dừng</h1>
<p>Về màn hình chờ, tắt pha/nước nóng, hủy trạng thái chuẩn bị ghi ADC nhanh nếu có. Bấm kiểm tra một lần; thường mất khoảng 1–2 giây. Không cần chạy bơm.</p>
<p>Phép thử giữ áp hiển thị cũ và khóa bơm tạm thời. Nếu bắt đầu pha, Main hủy thử và khôi phục ADC trước khi mở khóa bơm. Không thay calibration.</p>
<button id="start">Kiểm tra ADC</button><button id="cancel">Hủy / thử khôi phục</button><button id="download" disabled>Tải kết quả JSON</button>
<pre id="status">Đang đọc trạng thái...</pre><p id="interpretation"></p>
<table><thead><tr><th>Mã tốc độ</th><th>ADS1115 dự kiến</th><th>ADS1015 dự kiến</th><th>Đo trung vị</th><th>Mẫu có bit thấp ≠ 0</th></tr></thead><tbody id="rows"></tbody></table>
<p>Thời gian đo gồm ghi thanh ghi, I²C và nhịp kiểm tra khoảng 1 ms; không đủ phân giải để so chính xác các tốc độ nhanh nhất. Kết luận chỉ mô tả hành vi, không chứng nhận chip chính hãng. Kết quả giữ trong RAM đến lần thử mới hoặc reboot.</p>
<script>
const endpoint='/api/pressure/adc-probe', el=id=>document.getElementById(id);
let busy=false, last=null;
const names=['Chưa kiểm tra','Đang chờ khóa bơm','Đang đo','Đang khôi phục ADC','Hoàn tất','Đã hủy, đã khôi phục','Thử lỗi, bơm đã được mở khóa','Khôi phục lỗi — bơm vẫn khóa; bấm thử khôi phục hoặc reboot Main'];
const causes=['','Đã bấm hủy','Máy rời trạng thái chờ hoặc bật pha/nước nóng','Quá thời gian','Lỗi I²C','Thanh ghi cấu hình không khớp','Không nhận được xác nhận khóa bơm'];
const delays={0:[125,7.8125],2:[31.25,2.0408],4:[7.8125,.625],7:[1.1628,.303]};
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
const id=()=>crypto.getRandomValues(new Uint32Array(1))[0]||1;
function median(a){a=[...a].sort((a,b)=>a-b);return (a[Math.floor((a.length-1)/2)]+a[Math.floor(a.length/2)])/2;}
function interpret(s){
 if(s.state!==4||s.samples.length!==24)return 'Chưa có phép đo hoàn tất để đối chiếu.';
 if(s.max_poll_gap_us>5000)return 'Nhịp kiểm tra bị trễ; chưa phân loại, hãy giữ file kết quả.';
 const a=median(s.samples.filter(r=>r[0]===0).map(r=>r[1]));
 const b=median(s.samples.filter(r=>r[0]===2).map(r=>r[1]));
 const bits=s.samples.some(r=>r[4]!==0);
 if(a>=70000&&a<=180000&&b>=20000&&b<=50000)return 'Thời gian gần ADS1115. '+(bits?'Có hoạt động ở bốn bit thấp.':'Bốn bit thấp vẫn bằng 0: còn điểm bất thường cần đối chiếu.');
 if(a>=4000&&a<=20000&&b>=1000&&b<=6000)return 'Thời gian gần ADS1015; '+(bits?'bit thấp không khớp định dạng ADS1015 thường.':'bit thấp cũng phù hợp định dạng 12-bit.');
 return 'Thời gian chưa khớp rõ hai loại. Giữ JSON để phân tích, chưa kết luận loại chip.';
}
function show(s){last=s;el('status').textContent=names[s.state]+(s.error?' — '+causes[s.error]:'')+'; mã '+s.run_id+'; '+s.samples.length+'/24 mẫu';
 el('interpretation').textContent=interpret(s);
 el('rows').innerHTML=Object.entries(delays).map(([code,d])=>{const rows=s.samples.filter(r=>r[0]===+code);return '<tr><td>'+code+'</td><td>'+d[0].toFixed(3)+' ms</td><td>'+d[1].toFixed(3)+' ms</td><td>'+(rows.length?(median(rows.map(r=>r[1]))/1000).toFixed(3)+' ms':'—')+'</td><td>'+rows.filter(r=>r[4]!==0).length+'/'+rows.length+'</td></tr>';}).join('');
}
async function rpc(op,run=0){
 const request={request_id:id(),run_id:run,op};
 // Retries preserve both IDs, so losing an HTTP reply cannot start two tests.
 for(let attempt=0;attempt<3;attempt++){
  try{
   let r=await fetch(endpoint,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(request)});
   if(!r.ok)throw Error((await r.json()).error||'Lỗi gửi');
   for(let n=0;n<25;n++){
    await sleep(100);r=await fetch(endpoint+'?id='+request.request_id,{cache:'no-store'});if(r.status===202)continue;
    const s=await r.json();if(!r.ok)throw Error(s.error||'Lỗi đọc');
    if(s.request_id!==request.request_id||s.version!==1||!Array.isArray(s.samples))throw Error('Sai kết quả');
    if(s.result)throw Error('Main từ chối ('+s.result+'): cần về chờ, tắt pha/nước nóng và hủy chuẩn bị ghi ADC nhanh.');
    if(op!==0&&s.run_id!==run)throw Error('Phiên thử đã thay đổi');return s;
   }
   throw Error('Hết thời gian chờ Main');
  }catch(e){if(attempt===2)throw e;await sleep(800);}
 }
}
async function action(fn){if(busy)return;busy=true;el('start').disabled=el('cancel').disabled=true;
 try{await fn();}catch(e){el('status').textContent=e.message;}
 finally{busy=false;el('cancel').disabled=false;el('start').disabled=!!last&&[1,2,3,7].includes(last.state);el('download').disabled=!last||!last.run_id;}}
el('start').onclick=()=>action(async()=>show(await rpc(1,id())));
el('cancel').onclick=()=>action(async()=>{if(last&&last.run_id)show(await rpc(2,last.run_id));});
el('download').onclick=()=>{if(!last)return;const url=URL.createObjectURL(new Blob([JSON.stringify(last,null,2)],{type:'application/json'}));const a=document.createElement('a');a.href=url;a.download='brewvector_adc_probe_'+last.run_id+'.json';document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);};
async function refresh(){await action(async()=>show(await rpc(0)));}refresh();setInterval(refresh,750);
</script></html>)HTML";
}
namespace HmiWebInternal {
void registerAdcProbeRoutes() {
  server.on("/diagnostics/adc", HTTP_GET, [](){server.send_P(200,"text/html; charset=utf-8",PAGE);});
  server.on("/api/pressure/adc-probe", HTTP_POST, submit);
  server.on("/api/pressure/adc-probe", HTTP_GET, status);
}
}
