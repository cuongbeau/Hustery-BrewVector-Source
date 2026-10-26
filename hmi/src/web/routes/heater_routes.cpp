#include "web/hmi_web_internal.h"
#include "communication/main_link.h"
#include <ArduinoJson.h>
#include <Update.h>
#include "update/main_firmware_updater.h"
#include <esp_system.h>
using namespace HmiWebInternal;
namespace {
uint32_t nextTicket = 0;
void state() {
  if (!authorized()) return;
  StaticJsonDocument<2048> d;
  bool online = webMainLink && webMainLink->heaterFresh();
  d["online"] = online;
  if (webMainLink) {
    const auto& s = webMainLink->heaterStatus();
    d["ticket"] = s.ticket; d["result"] = static_cast<int>(s.result);
    d["mode"] = static_cast<int>(s.mode); d["fault"] = static_cast<int>(s.fault);
    d["uptime"] = s.uptime; d["sampleAge"] = s.sampleAge; d["idleRemaining"] = s.idleRemaining;
    d["raw"] = s.raw; d["filtered"] = s.filtered; d["power"] = s.power;
    d["p"] = s.p; d["i"] = s.i; d["d"] = s.d; d["relay"] = s.relay;
    d["valid"] = s.valid; d["idleExpired"] = s.idleExpired;
    d["target"] = s.config.target; d["ceiling"] = s.config.ceiling;
    d["kp"] = s.config.kp; d["ki"] = s.config.ki; d["kd"] = s.config.kd;
    d["waterMaxPower"] = s.config.waterMaxPower; d["activeMaxPower"] = s.activeMaxPower; d["maxPower"] = s.config.maxPower; d["idleMinutes"] = s.config.idleMinutes;
    d["autoSleep"] = s.config.autoSleep != 0;
    d["sleepDelaySeconds"] = s.config.sleepDelaySeconds;
    d["stableSeconds"] = s.config.stableSeconds;
    d["heaterW"] = s.config.heaterW; d["inletC"] = s.config.inletC;
    d["ffScale"] = s.config.ffScale; d["ffEnabled"] = s.config.ffEnabled != 0;
    d["ready"] = s.ready != 0;
    d["sensorDiagnosticsFresh"] = webMainLink->heaterSensorFresh();
    const auto& sensor = webMainLink->heaterSensorStatus();
    d["sensorTotal"] = sensor.total; d["sensorErrors"] = sensor.errors;
    d["sensorIssues"] = sensor.issues; d["sensorLastIssues"] = sensor.lastIssues;
    d["sensorSampledAt"] = sensor.sampledAt; d["sensorLastErrorAt"] = sensor.lastErrorAt;
    d["sensorFrame"] = sensor.frame; d["sensorLastErrorFrame"] = sensor.lastErrorFrame;
    d["sensorWindowSamples"] = sensor.windowSamples; d["sensorWindowErrors"] = sensor.windowErrors;
    d["sensorWindowReady"] = sensor.windowReady != 0;
    d["sensorPeakWindowSamples"] = sensor.peakWindowSamples; d["sensorPeakWindowErrors"] = sensor.peakWindowErrors;
    d["sensorConsecutiveErrors"] = sensor.consecutiveErrors; d["sensorMaxConsecutiveErrors"] = sensor.maxConsecutiveErrors;
    d["sensorEvaluatedWindows"] = sensor.evaluatedWindows;
    d["sensorWindowsAtOrAbove20"] = sensor.windowsAtOrAbove20;
    d["sensorControlSamples"] = sensor.controlSamples; d["sensorControlErrors"] = sensor.controlErrors;
    d["sensorControlCapacity"] = sensor.controlCapacity;
    d["sensorControlWarnErrors"] = sensor.controlWarnErrors;
    d["sensorControlPauseErrors"] = sensor.controlPauseErrors;
    d["sensorControlResumeErrors"] = sensor.controlResumeErrors;
    d["sensorControlTripErrors"] = sensor.controlTripErrors;
    d["sensorSamplePeriodMs"] = sensor.samplePeriodMs; d["sensorMaxGoodAgeMs"] = sensor.maxGoodAgeMs;
    d["sensorPauseGoodAgeMs"] = sensor.pauseGoodAgeMs;
    d["sensorPaused"] = sensor.paused != 0;
    d["sensorDensityPaused"] = sensor.densityPaused != 0;
    d["thermalFeedforward"] = sensor.thermalFeedforward;
    d["inletFlow"] = sensor.inletFlow; d["feedforwardKff"] = sensor.feedforwardKff;
    d["flowQualified"] = sensor.flowQualified != 0;
    d["inletValveOpen"] = sensor.inletValveOpen != 0;
    d["pidPeriodMs"] = sensor.pidPeriodMs;
    d["inletPressure"] = sensor.inletPressure; d["inletDuty"] = sensor.inletDuty;
  }
  String body; serializeJson(d, body); server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", body);
}
void command() {
  if (!authorized()) return;
  StaticJsonDocument<768> d;
  if (deserializeJson(d, server.arg("plain")) || !d["op"].is<int>()) { server.send(400, "text/plain", "Invalid request"); return; }
  const int op = d["op"].as<int>();
  if (op < 0 || op > 4) { server.send(400, "text/plain", "Invalid operation"); return; }
  if (Update.isRunning() || (op != 0 && MainFirmwareUpdater::busy())) { server.send(409, "text/plain", "OTA active"); return; }
  HeaterWire::Request r; r.op = static_cast<HeaterWire::Op>(op);
  if (r.op == HeaterWire::Op::Configure) {
    for (const char* key : {"target", "ceiling", "kp", "ki", "kd", "maxPower", "idleMinutes"})
      if (!d[key].is<float>()) { server.send(400, "text/plain", "Missing numeric configuration"); return; }
    int minutes = d["idleMinutes"].as<int>();
    if (minutes < 1 || minutes > 240 || d["idleMinutes"].as<float>() != minutes) { server.send(400, "text/plain", "Invalid timeout"); return; }
    r.config.target = d["target"]; r.config.ceiling = d["ceiling"];
    r.config.kp = d["kp"]; r.config.ki = d["ki"]; r.config.kd = d["kd"];
    r.config.maxPower = d["maxPower"]; r.config.idleMinutes = minutes;
    if (!d["autoSleep"].is<bool>() || !d["sleepDelaySeconds"].is<int>() ||
        !d["stableSeconds"].is<int>() || d["sleepDelaySeconds"].as<int>() < 0 ||
        d["sleepDelaySeconds"].as<int>() > 3600 || d["stableSeconds"].as<int>() < 0 ||
        d["stableSeconds"].as<int>() > 300) { server.send(400, "text/plain", "Invalid sleep/readiness settings"); return; }
    r.config.autoSleep = d["autoSleep"].as<bool>();
    r.config.sleepDelaySeconds = d["sleepDelaySeconds"].as<int>();
    r.config.stableSeconds = d["stableSeconds"].as<int>();
    if (!d["ffEnabled"].is<bool>() || !d["heaterW"].is<float>() ||
        !d["inletC"].is<float>() || !d["ffScale"].is<float>()) {
      server.send(400, "text/plain", "Missing thermal feedforward settings"); return;
    }
    if (!d["waterMaxPower"].is<float>()) { server.send(400, "text/plain", "Missing water power cap"); return; }
    r.config.waterMaxPower = d["waterMaxPower"];
    r.config.ffEnabled = d["ffEnabled"].as<bool>();
    r.config.heaterW = d["heaterW"]; r.config.inletC = d["inletC"]; r.config.ffScale = d["ffScale"];
  }
  if (r.op == HeaterWire::Op::Test) {
    if (!d["testPower"].is<float>() || !d["testSeconds"].is<int>() ||
        d["testSeconds"].as<int>() < 1 || d["testSeconds"].as<int>() > 60) { server.send(400, "text/plain", "Invalid test"); return; }
    r.testPower = d["testPower"]; r.testSeconds = d["testSeconds"];
  }
  if (!nextTicket) nextTicket = esp_random();
  r.ticket = ++nextTicket; if (!r.ticket) r.ticket = ++nextTicket;
  if (!webMainLink || !webMainLink->requestHeater(r)) { server.send(409, "text/plain", "Heater Main unavailable"); return; }
  server.send(202, "application/json", String("{\"ticket\":") + r.ticket + "}");
}
const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang="vi"><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Điều khiển nhiệt thân</title>
<style>body{font:16px system-ui;background:#101923;color:#edf3f7;max-width:900px;margin:24px auto;padding:0 16px}a{color:#81ccff}label{display:inline-block;margin:8px}input{display:block;width:110px;padding:8px}button{padding:12px;margin:6px;cursor:pointer}#off{background:#b21d25;color:white}#alarm{padding:16px;background:#773000}.danger{background:#b00020!important;color:white}canvas{width:100%;height:240px;background:#172533}pre{white-space:pre-wrap}</style>
<a href="/">Dashboard</a> · <a href="/firmware">OTA</a><h1>Nhiệt thân thermoblock</h1>
<p>Type K trên thân block. Chưa hiệu chuẩn nhiệt nước/offset họng pha. Khi cấp nguồn bình thường, tự Preheat nếu cảm biến và cấu hình hợp lệ. Sau OTA/lỗi reset: OFF.</p>
<p>Brew được mở khi nhiệt thô và nhiệt lọc thân block đạt ít nhất 100°C trong thời gian chờ đã cài (nếu mục tiêu dưới 100°C thì dùng mục tiêu). Đây là điều kiện cho phép chạy bơm, không xác nhận nhiệt nước pha. Mục tiêu PID và bảo vệ quá nhiệt vẫn giữ nguyên.</p>
<p>Khi lỗi cảm biến vượt giới hạn, heater khóa OFF. Số nhiệt hiển thị không tự xóa khóa lỗi. Ngưỡng ngắt bằng 0 nghĩa là chưa cấu hình, không phải lỗi cảm biến.</p>
<div id="alarm" role="alert">Đang chờ trạng thái Main…</div><pre id="live"></pre>
<pre id="appliedConfig">Chờ cấu hình Main…</pre><pre id="pidDetails">Chờ dữ liệu PID…</pre>
<pre id="sensorDiagnostic">Chờ chẩn đoán cảm biến…</pre>
<pre id="sensorWindow">Chờ thống kê cửa sổ 5 giây…</pre>
<pre id="sensorPolicy">Chờ chính sách cảm biến từ Main…</pre>
<p>Mẫu lỗi dưới giới hạn: giữ nhiệt tốt gần nhất; PID vẫn tính theo chu kỳ 1 giây. Quá thời gian tạm dừng: SSR lệnh OFF, ngừng tích phân PID; mẫu tốt trở lại trước khi khóa sẽ tự tiếp tục nếu các điều kiện hợp lệ. Không tích phân bù thời gian đã dừng. Khi đã khóa, cần mẫu mới hợp lệ và bộ đếm lỗi dưới ngưỡng để Xóa lỗi; heater vẫn OFF. Ngưỡng 20% phía trên chỉ để theo dõi, không phải ngưỡng khóa. Khởi động lại Main bắt đầu phiên đo mới; Xóa lỗi không đặt lại thống kê.</p>
<button id="off" onclick="send(0)">TẮT GIA NHIỆT</button><button class="mut" onclick="send(2)">Preheat / bật PID</button><button class="mut" onclick="send(4)">Xóa lỗi (giữ OFF)</button>
<p>Thông số chỉ đổi khi heater OFF và máy ở Ready. Giới hạn nhiệt phải được xác định phù hợp phần cứng trước khi bật tải.</p>
<div id="fields"></div><label><input id="autoSleep" type="checkbox" checked>Bật tự nghỉ</label>
<label><input id="ffEnabled" type="checkbox">Bật bù nhiệt khi có dòng nước</label><button class="mut" onclick="send(1)">Lưu cấu hình</button>
<p><b>Công suất heater:</b> giá trị danh định ghi trên heater. <b>Nước vào:</b> nhiệt độ ước tính của nước trong bình. <b>Hệ số bù:</b> 1,00 dùng toàn bộ mô hình; 0,50 dùng một nửa. Flow được ước tính từ duty bơm và áp suất, không phải flowmeter. Bù nhiệt chỉ hoạt động khi đang chạy nước, van mở, bơm hoạt động và dữ liệu còn mới. Tổng PID + bù nhiệt chịu trần khi chạy nước; khi dừng hoặc mất điều kiện, hệ thống trở về trần giữ nhiệt ngay.</p>
<p>Hết thời gian chờ + độ trễ thêm sẽ tắt heater. Bỏ chọn tự nghỉ để giữ nhiệt liên tục. Bộ đếm tính lại sau hoạt động bơm và lần đầu Preheat đạt Ready; không nhận sleep/wakeup từ bo zin.</p>
<p>Đo đáp ứng có người giám sát, tự kết thúc sau thời gian bên dưới.</p>
<label>Công suất thử %<input id="testPower" type="number" value="10" min="1" max="100"></label>
<label>Thời gian thử s<input id="testSeconds" type="number" value="10" min="1" max="60"></label>
<button class="mut" onclick="send(3)">Chạy thử giới hạn</button><pre id="reply"></pre>
<canvas id="plot"></canvas><p>Nhiệt thân: cam · Mục tiêu: trắng · Công suất: xanh (0–100%)</p>
<button onclick="download()">Tải CSV phiên Web</button><p>Log chỉ ghi khi trang đang mở, tối đa 7.200 mẫu; lưu CSV trước khi đóng trang.</p>
<script>
const specs={target:['Nhiệt thân °C',30,130],ceiling:['Ngắt quá nhiệt °C',35,150],kp:['Kp %/°C',0,100],ki:['Ki %/(°C·s)',0,10],kd:['Kd %·s/°C',0,100],maxPower:['Trần giữ nhiệt %',1,100],waterMaxPower:['Trần khi chạy nước %',1,100],idleMinutes:['Chờ nghỉ (phút)',1,240],sleepDelaySeconds:['Trễ tắt thêm (giây)',0,3600],stableSeconds:['Chờ mở Brew (giây)',0,300],heaterW:['Heater W',100,4000],inletC:['Nước vào °C',0,60],ffScale:['Hệ số bù (0–1)',0,1]};
const $=id=>document.getElementById(id);for(const [k,v] of Object.entries(specs)){const l=document.createElement('label');l.textContent=v[0];const i=document.createElement('input');Object.assign(i,{id:k,type:'number',min:v[1],max:v[2],step:['idleMinutes','sleepDelaySeconds','stableSeconds'].includes(k)?'1':'0.01'});l.append(i);$('fields').append(l);}
let initial=false,last=null,rows=[],pending=false,online=false,polling=false,pollTimer=null,pollController=null,commandEpoch=0;
const modes=['OFF','PID','THỬ CÔNG SUẤT','LỖI'];const faults=['','LỖI CẢM BIẾN','MẪU NHIỆT QUÁ HẠN','QUÁ NHIỆT — NGẮT NGUỒN MÁY','VÒNG LẶP MAIN BỊ TREO'];
function controls(){document.querySelectorAll('.mut').forEach(b=>b.disabled=pending||!online);}
function sensorIssues(mask){const names=[[1,'OC: chip báo hở đầu dò'],[2,'SCG: chip báo chạm GND'],[4,'SCV: chip báo chạm VCC'],[8,'Khung SPI không hợp lệ'],[16,'Nhiệt ngoài dải -20…500°C'],[32,'Không đọc được bus']];return names.filter(([bit])=>mask&bit).map(([,label])=>label).join(' + ')||'Hợp lệ';}
function renderWindow(s){
 const e=$('sensorWindow');
 if(!s.online||!s.sensorDiagnosticsFresh||!Number.isFinite(s.sensorWindowSamples)){e.textContent='Thống kê 5 giây chưa có hoặc quá hạn; cần Main và HMI hỗ trợ bản mới.';return;}
 const ratio=(errors,total)=>total?`${(100*errors/total).toFixed(1)}% (${errors}/${total} mẫu)`:'Chưa có mẫu';
 const ready=s.sensorWindowReady&&s.sensorWindowSamples>0;
 const label=!ready?'Chưa đủ 5 giây liên tục hoặc đọc bị gián đoạn':s.sensorWindowErrors*5<s.sensorWindowSamples?'Dưới 20%':'ĐẠT / VƯỢT 20%';
 e.textContent=`5 giây gần nhất: ${ratio(s.sensorWindowErrors,s.sensorWindowSamples)} — ${label}\nChuỗi lỗi hiện tại: ${s.sensorConsecutiveErrors} mẫu | Dài nhất từ boot: ${s.sensorMaxConsecutiveErrors} mẫu\nTỷ lệ cao nhất đã ghi nhận trong cửa sổ đủ 5 giây: ${ratio(s.sensorPeakWindowErrors,s.sensorPeakWindowSamples)}\nCửa sổ đạt / vượt 20%: ${s.sensorWindowsAtOrAbove20} / ${s.sensorEvaluatedWindows} lần đánh giá\nMain đếm từng mẫu dù Web đóng. Các cửa sổ chồng lấn, đánh giá sau mỗi mẫu; không phải các đợt lỗi riêng. Với 20 mẫu, 3 lỗi = 15%; 4 lỗi = 20%.`;
}
function renderPolicy(s){
 const e=$('sensorPolicy');
 if(!s.online||!s.sensorDiagnosticsFresh||!s.sensorControlCapacity){e.textContent='Chính sách cảm biến chưa có hoặc quá hạn; cần Main và HMI hỗ trợ bản mới.';return;}
 e.textContent=`Bộ đếm điều khiển: ${s.sensorControlErrors} lỗi / ${s.sensorControlSamples} mẫu gần nhất (tối đa ${s.sensorControlCapacity})\nKhóa khi đủ ${s.sensorControlTripErrors} lỗi; đọc mỗi ${s.sensorSamplePeriodMs} ms. Quá ${s.sensorPauseGoodAgeMs} ms không có mẫu tốt: tạm dừng gia nhiệt. Quá ${s.sensorMaxGoodAgeMs} ms: ngắt và khóa.\nCửa sổ điều khiển đếm mẫu, có thể dài hơn 5 giây nếu đọc trễ.`;
}
function renderPid(s){
 const e=$('pidDetails');
 if(!s.online||!s.sensorDiagnosticsFresh||!s.pidPeriodMs){e.textContent='Dữ liệu PID chưa có hoặc quá hạn; cần Main và HMI bản mới.';return;}
 e.textContent=`PID của BrewVector · Chu kỳ ${s.pidPeriodMs} ms\nBù nhiệt: ${s.feedforwardKff>0?'BẬT':'TẮT'} | FF ${(Number.isFinite(s.thermalFeedforward)?s.thermalFeedforward.toFixed(2):'—')}%\nFlow đầu vào: ${(Number.isFinite(s.inletFlow)?s.inletFlow.toFixed(2):'—')} ml/s | ${s.flowQualified?'ước tính từ duty bơm và áp suất':'chưa đủ điều kiện hoặc ngoài vùng mô hình'}\n${s.sensorPaused?'Tạm dừng cảm biến — công suất 0%.':'Tổng P + I + D + bù nhiệt được giới hạn bởi trần công suất đã cài.'}`;
}function isSensorPaused(s){return s.online&&!s.fault&&(s.mode===1||s.mode===2)&&s.sensorDiagnosticsFresh&&s.sensorPaused;}
function renderSensor(s){renderPid(s);renderWindow(s);renderPolicy(s);const e=$('sensorDiagnostic');if(!s.online||!s.sensorDiagnosticsFresh){e.textContent='Chẩn đoán cảm biến chưa có hoặc đã quá hạn; cần Main có hỗ trợ.';return;}const hex=n=>'0x'+(n>>>0).toString(16).padStart(8,'0').toUpperCase();e.textContent=`Mẫu lỗi / tổng: ${s.sensorErrors} / ${s.sensorTotal}\nMẫu hiện tại: ${sensorIssues(s.sensorIssues)} | ${hex(s.sensorFrame)}\nLỗi gần nhất: ${s.sensorErrors?sensorIssues(s.sensorLastIssues):'Chưa ghi nhận'} | ${hex(s.sensorLastErrorFrame)} | Main ${s.sensorLastErrorAt} ms\nBộ đếm từ lúc Main khởi động; Xóa lỗi không xóa lịch sử này. Mã chip là dữ liệu quan sát, chưa kết luận phần cứng hỏng.`;}
async function heaterRequest(options={},controller=null){
 controller=controller||(typeof AbortController==='function'?new AbortController():null);
 let timer;
 const timeout=new Promise((_,reject)=>{timer=setTimeout(()=>{if(controller)controller.abort();reject(Error('Hết thời gian chờ Web; chưa xác nhận kết quả.'));},8000);});
 try{return await Promise.race([timeout,(async()=>{const r=await fetch('/api/heater',{cache:'no-store',...options,...(controller?{signal:controller.signal}:{})});if(!r.ok)throw Error('HTTP '+r.status+': '+(r.text?await r.text():''));return r.json();})()]);}
 finally{clearTimeout(timer);}
}
async function getState(controller=null){return heaterRequest({},controller);}
async function send(op){if(op!==0&&pending)return;if(op!==0&&!online){$('reply').textContent='Chưa có dữ liệu Main mới; chờ kết nối.';return;}const epoch=++commandEpoch;pending=true;if(pollController)pollController.abort();controls();try{const b={op};if(op===1)for(const k in specs){const v=Number($(k).value);if(!$(k).value||!Number.isFinite(v))throw Error('Điền đủ thông số');b[k]=v;}if(op===1){b.autoSleep=$('autoSleep').checked;b.ffEnabled=$('ffEnabled').checked;}if(op===3){b.testPower=Number($('testPower').value);b.testSeconds=Number($('testSeconds').value);}const result=await heaterRequest({method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});if(epoch!==commandEpoch)return;const t=result.ticket;$('reply').textContent='Chờ Main xác nhận #'+t;for(let n=0;n<8;n++){await new Promise(r=>setTimeout(r,250));if(epoch!==commandEpoch)return;const s=await getState();if(epoch!==commandEpoch)return;if(s.online&&s.ticket===t){$('reply').textContent=s.result===3&&op===4?'Chưa xóa được: cần mẫu nhiệt hợp lệ, còn mới, dưới ngưỡng ngắt và bộ đếm lỗi dưới giới hạn.':s.result===3&&(op===2||op===3)&&s.ceiling<=0?'Chưa cấu hình ngưỡng ngắt quá nhiệt. Heater vẫn OFF.':['Main đã thực hiện','Thông số không hợp lệ','Main bận / cần OFF và Ready','Chưa an toàn hoặc lỗi lưu cấu hình'][s.result];return;}}throw Error('Chưa xác nhận kết quả; không tự gửi lại.');}catch(e){if(epoch===commandEpoch)$('reply').textContent=e.message;}finally{if(epoch===commandEpoch){pending=false;controls();}}}
function draw(){const c=$('plot'),x=c.getContext('2d');c.width=Math.max(300,c.clientWidth);c.height=240;const pts=rows.slice(-600);if(!pts.length)return;for(const [key,color,scale] of [['filtered','#ffaf54',160],['target','#fff',160],['power','#62d9b0',100]]){x.strokeStyle=color;x.beginPath();pts.forEach((p,i)=>{const a=i*c.width/Math.max(1,pts.length-1),b=235-p[key]/scale*225;i?x.lineTo(a,b):x.moveTo(a,b);});x.stroke();}}
async function poll(){
 if(polling)return;clearTimeout(pollTimer);
 if(pending){pollTimer=setTimeout(poll,500);return;}
 polling=true;controls();let gotState=false;
 pollController=typeof AbortController==='function'?new AbortController():null;
 const stale=setTimeout(()=>{online=false;controls();$('alarm').className='danger';$('alarm').textContent='ĐANG CHỜ DỮ LIỆU WEB — KHÔNG XÁC NHẬN HEATER ĐÃ TẮT';},2500);
 try{const s=await getState(pollController);if(pending)return;gotState=true;
 if(!s || typeof s.online!=='boolean')throw Error('Phản hồi không hợp lệ');
 if(s.online && ['filtered','raw','target','power','p','i','d'].some(k=>!Number.isFinite(s[k])))throw Error('Thiếu dữ liệu nhiệt/PID');
 online=s.online;renderSensor(s);const bad=!s.online||s.fault; $('alarm').className=bad?'danger':'';$('alarm').textContent=!s.online?'MẤT LIÊN LẠC / CHƯA HỖ TRỢ — KHÔNG XÁC NHẬN HEATER ĐÃ TẮT':s.fault===1&&s.valid&&s.sampleAge<=(s.sensorDiagnosticsFresh?s.sensorPauseGoodAgeMs:1000)?'ĐÃ KHÓA DO LỖI CẢM BIẾN — hiện có mẫu hợp lệ; cần bộ đếm dưới ngưỡng và Xóa lỗi, heater vẫn OFF':s.fault?faults[s.fault]:isSensorPaused(s)?'TẠM DỪNG — chờ mẫu nhiệt tốt; SSR lệnh OFF; có thể tự tiếp tục':s.idleExpired?'Đã tắt do không hoạt động':s.mode===1?(s.ready?'READY — PID':'PREHEATING — PID'):modes[s.mode];if(s.online){$('appliedConfig').textContent=`Main đã nhận: bù nhiệt ${s.ffEnabled?'BẬT':'TẮT'} | Trần giữ nhiệt ${s.maxPower}% | Trần khi chạy nước ${s.waterMaxPower}% | Trần đang áp dụng ${s.activeMaxPower}%`;if(!initial){for(const k in specs)$(k).value=s[k];$('autoSleep').checked=s.autoSleep;$('ffEnabled').checked=s.ffEnabled;initial=true;}$('live').textContent=`Thân: ${s.filtered.toFixed(2)} °C | Thô: ${s.raw.toFixed(2)} °C | Mục tiêu: ${s.target} °C\nCông suất: ${s.power.toFixed(1)}% | SSR lệnh: ${s.relay?'ON':'OFF'} | Idle còn: ${s.autoSleep?Math.ceil(s.idleRemaining/1000)+' giây':'Tự nghỉ đã tắt'}\nP ${s.p.toFixed(2)} / I ${s.i.toFixed(2)} / D ${s.d.toFixed(2)} | Tuổi mẫu tốt ${s.sampleAge} ms`;if(last!==s.uptime){last=s.uptime;rows.push({...s,localTime:new Date().toISOString()});if(rows.length>7200)rows.shift();draw();}}}catch(e){if(!pending){online=false;renderSensor({online:false});$('alarm').className='danger';$('alarm').textContent=(gotState?'DỮ LIỆU / HIỂN THỊ WEB LỖI':'MẤT KẾT NỐI WEB')+' — KHÔNG XÁC NHẬN HEATER ĐÃ TẮT';$('live').textContent=e.message;}}finally{clearTimeout(stale);pollController=null;polling=false;controls();pollTimer=setTimeout(poll,document.hidden?5000:500);}}
function download(){const keys=['localTime','uptime','raw','filtered','target','power','p','i','d','relay','mode','fault','sampleAge','idleRemaining','sensorDiagnosticsFresh','sensorSampledAt','sensorErrors','sensorTotal','sensorIssues','sensorLastIssues','sensorWindowReady','sensorWindowSamples','sensorWindowErrors','sensorPeakWindowSamples','sensorPeakWindowErrors','sensorConsecutiveErrors','sensorMaxConsecutiveErrors','sensorEvaluatedWindows','sensorWindowsAtOrAbove20','sensorControlSamples','sensorControlErrors','sensorControlCapacity','sensorControlTripErrors','sensorSamplePeriodMs','sensorMaxGoodAgeMs','sensorPauseGoodAgeMs','sensorPaused','thermalFeedforward','inletFlow','feedforwardKff','flowQualified','inletValveOpen','pidPeriodMs','heaterW','inletC','ffScale','ffEnabled','waterMaxPower','activeMaxPower','inletPressure','inletDuty'];const blob=new Blob([keys.join(',')+'\n'+rows.map(s=>keys.map(k=>s[k]).join(',')).join('\n')],{type:'text/csv'});const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='heater-'+Date.now()+'.csv';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000);}poll();
</script></html>)HTML";
}
namespace HmiWebInternal {
void registerHeaterRoutes() {
  server.on("/heater", HTTP_GET, [](){
    if(!authorized())return;
    server.sendHeader("Location", "/#heater", true);
    server.send(302, "text/plain", "PID heater moved to the main interface");
  });
  server.on("/api/heater", HTTP_GET, state);
  server.on("/api/heater", HTTP_POST, command);
}
}
