#include "web/hmi_web_internal.h"
#include "communication/main_link.h"
#include <ArduinoJson.h>

using namespace HmiWebInternal;
using namespace PressureCaptureWire;
namespace {
Request pending{};
uint32_t sentMs = 0;

void error(int code, const char* text) {
  JsonDocument doc;
  doc["error"] = text;
  String body;
  serializeJson(doc, body);
  server.send(code, "application/json", body);
}
void submit() {
  if (!authorized()) return;
  JsonDocument doc;
  if (deserializeJson(doc, server.arg("plain")) ||
      !doc["request_id"].is<uint32_t>() || !doc["capture_id"].is<uint32_t>() ||
      !doc["op"].is<uint8_t>() || !doc["offset"].is<uint16_t>()) {
    error(400, "Invalid request"); return;
  }
  Request req{doc["request_id"], doc["capture_id"], doc["offset"],
      static_cast<Op>(doc["op"].as<uint8_t>()), VERSION};
  if (!req.requestId || req.op > Op::Clear) { error(400, "Invalid operation"); return; }
  if (!webMainLink || !webMainLink->connected() || !webMainLink->supportsPressureCapture()) {
    error(503, "Main disconnected or pressure capture unsupported"); return;
  }
  if (pending.requestId && webMainLink->pressureCaptureReply().requestId != pending.requestId &&
      millis() - sentMs < 2000U) {
    if (req.requestId != pending.requestId) { error(409, "Another request is pending"); return; }
  } else {
    if (!webMainLink->requestPressureCapture(req)) { error(503, "Cannot send request"); return; }
    pending = req;
    sentMs = millis();
  }
  server.send(202, "application/json", "{\"pending\":true}");
}
void fetchReply() {
  if (!authorized()) return;
  server.sendHeader("Cache-Control", "no-store");
  const String id = server.arg("id");
  if (!pending.requestId || id != String(pending.requestId)) { error(409, "Request replaced; retry status"); return; }
  if (!webMainLink || !webMainLink->connected()) { error(503, "Main disconnected"); return; }
  const auto& r = webMainLink->pressureCaptureReply();
  if (r.requestId != pending.requestId) {
    if (millis() - sentMs >= 2000U) error(408, "Main reply timeout");
    else server.send(202, "application/json", "{\"pending\":true}");
    return;
  }
  JsonDocument doc;
  doc["request_id"] = r.requestId;
  doc["capture_id"] = r.captureId;
  doc["op"] = static_cast<uint8_t>(r.op);
  doc["version"] = r.version;
  doc["status"] = static_cast<uint8_t>(r.status);
  doc["result"] = static_cast<uint8_t>(r.result);
  doc["end"] = static_cast<uint8_t>(r.end);
  doc["fast_active"] = r.fastActive != 0;
  doc["total"] = r.total;
  doc["offset"] = r.offset;
  doc["started_at_ms"] = r.startedAtMs;
  doc["busy_polls"] = r.busyPolls;
  doc["pressure_offset"] = r.pressureOffset;
  doc["pressure_scale"] = r.pressureScale;
  doc["poll_ms"] = POLL_MS;
  doc["publish_ms"] = PUBLISH_MS;
  doc["mean_window"] = MEAN_WINDOW;
  doc["duration_ms"] = DURATION_MS;
  doc["capacity"] = CAPACITY;
  JsonArray samples = doc["rows"].to<JsonArray>();
  for (uint8_t i = 0; i < r.count; ++i) {
    const auto& s = r.samples[i];
    JsonArray row = samples.add<JsonArray>();
    row.add(s.readUs); row.add(s.raw); row.add(s.meanSamples); row.add(s.flags);
    row.add(s.meanRaw); row.add(s.controllerFilteredBar); row.add(s.pumpPercent);
  }
  String body;
  serializeJson(doc, body);
  server.send(200, "application/json", body);
}

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="vi"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Chẩn đoán lấy mẫu áp suất</title><style>
body{font:16px system-ui;background:#15171b;color:#eee;margin:24px auto;padding:0 20px;max-width:720px;line-height:1.6}
a{color:#83caff}button{padding:12px 16px;margin:5px;font:inherit;cursor:pointer}pre{white-space:pre-wrap;font:15px system-ui}
</style><a href="/">Về Dashboard</a> · <a href="/diagnostics/adc">Kiểm tra ADC khi bơm dừng</a><h1>Ghi ADC nhanh</h1>
<p>Phép thử sóng áp chậm: áp trung bình chỉ được ghi để đối chiếu, chưa đưa vào điều khiển.</p>
<ol><li>Chọn profile OFF, về màn hình chờ, tắt pha/nước nóng. Bấm <b>Chuẩn bị ghi</b> và chờ Main báo sẵn sàng.</li>
<li>Tự bắt đầu pha trong 60 giây. Main ghi 15 giây đầu của shot; chạy khoảng 20 giây rồi tự dừng như bình thường.</li>
<li>Tắt pha/nước nóng. Bấm <b>Tải CSV ADC</b> sau khi trạng thái cho phép. Giữ thêm CSV shot thường và ghi lại quan sát kim đồng hồ.</li></ol>
<p>Lệnh trên trang này không bật hoặc dừng bơm. Hủy ghi chỉ hủy dữ liệu đang thu. Lịch ADC nhanh trở về bình thường khi shot kết thúc. Dữ liệu chỉ giữ trong RAM Main, mất khi reboot; lần ghi mới thay bản cũ.</p>
<button id="arm">Chuẩn bị ghi</button><button id="cancel">Hủy ghi</button>
<button id="download" disabled>Tải CSV ADC</button><button id="clear">Xóa bản ghi / trả RAM</button>
<pre id="status" role="status">Đang đọc trạng thái...</pre><pre id="progress"></pre>
<script>
const endpoint='/api/pressure/capture', node=document.getElementById('status'), progress=document.getElementById('progress');
const states=['Chưa có bản ghi','Đã chuẩn bị — tự bắt đầu pha trong 60 giây','Đang ghi ADC','Ghi xong','Đã hủy ghi','Hết thời gian chờ'];
const errors=['','Main đang bận: về màn hình chờ, tắt pha/nước nóng','Yêu cầu không hợp lệ','Không đủ RAM Main','ADC chưa sẵn sàng hoặc chế độ chưa hỗ trợ','Bản ghi đã bị thay hoặc Main reboot'];
let busy=false, latest=null;
const sleep=ms=>new Promise(resolve=>setTimeout(resolve,ms));
function randomId(){return crypto.getRandomValues(new Uint32Array(1))[0]||1;}
function show(s){latest=s;node.textContent=states[s.status]+'; '+s.total+' mẫu; mã '+s.capture_id+(s.fast_active?' — hãy dừng shot trước khi tải.':'');
document.getElementById('download').disabled=busy||!s.total||s.fast_active||s.status===1||s.status===2;}
async function rpc(op,capture=0,offset=0){
 const req={request_id:randomId(),capture_id:capture,offset,op};
 for(let attempt=0;attempt<3;attempt++){
  try{
   let response=await fetch(endpoint,{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(req)});
   if(!response.ok)throw Error((await response.json()).error||'Gửi yêu cầu thất bại');
   for(let n=0;n<24;n++){
    await sleep(100);response=await fetch(endpoint+'?id='+req.request_id,{cache:'no-store'});
    if(response.status===202)continue;
    const s=await response.json();if(!response.ok)throw Error(s.error||'Lỗi đọc kết quả');
    if(s.request_id!==req.request_id||s.op!==op||s.version!==1)throw Error('Sai mã kết quả');
    if(s.result)throw Error(errors[s.result]||'Main từ chối');
    if(op!==0&&op!==4&&s.capture_id!==capture)throw Error('Bản ghi đã thay đổi');
    return s;
   }
   throw Error('Hết thời gian chờ Main');
  }catch(e){if(attempt===2)throw e;await sleep(200);}
 }
}
async function action(fn){
 if(busy)return;busy=true;document.querySelectorAll('button').forEach(b=>b.disabled=true);
 try{await fn();}catch(e){node.textContent=e.message+'; dữ liệu chưa tải không bị xóa. Có thể thử lại.';}
 finally{busy=false;document.querySelectorAll('button').forEach(b=>b.disabled=false);
 document.getElementById('download').disabled=!latest||!latest.total||latest.fast_active||latest.status===1||latest.status===2;}
}
document.getElementById('arm').onclick=()=>action(async()=>{progress.textContent='';show(await rpc(1,randomId()));});
document.getElementById('cancel').onclick=()=>action(async()=>{if(latest&&latest.capture_id)show(await rpc(3,latest.capture_id));});
document.getElementById('clear').onclick=()=>action(async()=>{if(latest&&latest.capture_id)show(await rpc(4,latest.capture_id));});
document.getElementById('download').onclick=()=>action(async()=>{
 const m=await rpc(0);show(m);if(!m.total||m.fast_active||m.status===1||m.status===2)throw Error('Hãy dừng shot rồi tải');
 const csv=['# pressure_capture '+JSON.stringify(m,(k,v)=>k==='rows'?undefined:v),
 'read_us,raw_counts,mean_samples,published_30ms,mean_counts,controller_filtered_bar,pump_percent,raw_bar_signed,mean_bar_signed'];
 for(let offset=0;offset<m.total;){
  const r=await rpc(2,m.capture_id,offset);
  if(r.offset!==offset||r.total!==m.total||r.started_at_ms!==m.started_at_ms||!r.rows.length||offset+r.rows.length>m.total)throw Error('Đứt hoặc thay bản ghi; tải lại từ đầu');
  for(const row of r.rows){
   const [us,raw,n,flags,mean,filtered,power]=row;
   csv.push([us,raw,n,flags&1,mean,filtered,power,(raw*0.000125-m.pressure_offset)*m.pressure_scale,(mean*0.000125-m.pressure_offset)*m.pressure_scale].join(','));
  }
  offset+=r.rows.length;progress.textContent='Đang tải '+offset+'/'+m.total+' mẫu...';
 }
 const url=URL.createObjectURL(new Blob([csv.join('\n')+'\n'],{type:'text/csv;charset=utf-8'}));
 const a=document.createElement('a');a.href=url;a.download='brewvector_adc_'+m.capture_id+'.csv';document.body.appendChild(a);a.click();a.remove();setTimeout(()=>URL.revokeObjectURL(url),1000);
 progress.textContent='Đã tải '+m.total+' mẫu. Bản gốc vẫn trong RAM Main.';
});
async function refresh(){if(!busy)await action(async()=>show(await rpc(0)));}
refresh();setInterval(refresh,2000);
</script></html>)HTML";
}
namespace HmiWebInternal {
void registerPressureDiagnosticRoutes() {
  server.on("/diagnostics/pressure", HTTP_GET, [](){
    if (!authorized()) return;
    server.send_P(200, "text/html; charset=utf-8", PAGE);
  });
  server.on("/api/pressure/capture", HTTP_POST, submit);
  server.on("/api/pressure/capture", HTTP_GET, fetchReply);
}
}
