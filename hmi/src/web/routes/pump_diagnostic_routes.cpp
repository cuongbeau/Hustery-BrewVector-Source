#include "web/hmi_web_internal.h"
#include <ArduinoJson.h>
#include "communication/main_link.h"

using namespace HmiWebInternal;

namespace {
uint32_t resetTicket = 0;
uint32_t resetSentMs = 0;
const char* resetResult = "none";

void refreshResetResult() {
  if (strcmp(resetResult, "pending") != 0) return;
  if (!webMainLink || !webMainLink->connected() ||
      webMainLink->lastCommand() != HmiProtocol::Command::ResetStoppedPsm) {
    resetResult = "unknown";
    return;
  }
  switch (webMainLink->commandState()) {
    case MainCommandState::Accepted: resetResult = "applied"; break;
    case MainCommandState::Rejected:
      resetResult = webMainLink->commandResult() ==
              HmiProtocol::CommandResult::InvalidCommand
          ? "unsupported" : "rejected";
      break;
    case MainCommandState::TimedOut: resetResult = "unknown"; break;
    default:
      if (millis() - resetSentMs > 3000U) resetResult = "unknown";
      break;
  }
}

void sendResetStatus(int code) {
  JsonDocument doc;
  doc["ticket"] = resetTicket;
  doc["state"] = resetResult;
  doc["hmi_uptime_ms"] = millis();
  String body;
  serializeJson(doc, body);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

void requestReset() {
  refreshResetResult();
  if (strcmp(resetResult, "pending") == 0) {
    sendResetStatus(409);
    return;
  }
  if (!webMainLink || !webMainLink->connected() ||
      !webMainLink->compatible() ||
      !webMainLink->sendCommand(HmiProtocol::Command::ResetStoppedPsm)) {
    server.send(409, "application/json",
                "{\"state\":\"not_sent\",\"error\":\"Main disconnected or busy\"}");
    return;
  }
  ++resetTicket;
  resetSentMs = millis();
  resetResult = "pending";
  sendResetStatus(202); // Queued, NOT confirmation that the PSM was reset.
}

const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="vi"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Chẩn đoán bơm</title><style>
body{font:16px system-ui;background:#15171b;color:#eee;margin:24px auto;padding:0 20px;max-width:620px;line-height:1.6}
a{color:#83caff}button{padding:12px 18px;font:inherit;cursor:pointer}pre{white-space:pre-wrap;font:14px system-ui}
</style><a href="/">Về Dashboard</a> · <a href="/diagnostics/pressure">Ghi ADC nhanh</a><h1>Reset PSM khi bơm dừng</h1>
<p>Phép thử chẩn đoán tiếng bơm dồn. Chỉ thực hiện ở màn hình chờ, công tắc pha và nước nóng đã tắt. Bơm sẽ không tự chạy sau reset.</p>
<ol><li>Lưu CSV Manual 50% free-flow đang dồn.</li><li>Dừng bơm, về màn hình chờ rồi nhấn nút bên dưới một lần.</li><li>Khi Main báo đã thực hiện, đợi 2 giây rồi tự chạy lại Manual 50% free-flow và lưu CSV cùng kết quả nghe.</li></ol>
<button id="reset">Reset PSM một lần</button><pre id="status" role="status">Chưa gửi yêu cầu.</pre>
<p>Không reboot Main, không đổi profile hoặc calibration. Kết quả này chưa chứng minh nguyên nhân lỗi pha. Lưu mã yêu cầu cùng hai số shot; trạng thái yêu cầu chỉ giữ trong RAM của HMI.</p>
<script>
const button=document.getElementById('reset'),statusNode=document.getElementById('status');
const labels={pending:'Đang chờ Main thực hiện...',applied:'Main đã reset PSM; bơm vẫn tắt.',rejected:'Main từ chối: cần về màn hình chờ và tắt pha/nước nóng.',unsupported:'Firmware Main chưa hỗ trợ thao tác này.',unknown:'Không xác định đã thực hiện hay chưa. Không tự gửi lại.',none:'Chưa gửi yêu cầu.',not_sent:'Chưa gửi được: Main mất kết nối hoặc đang bận.'};
function show(s){statusNode.textContent=(s.ticket?'Yêu cầu #'+s.ticket+' — ':'')+(labels[s.state]||s.state);}
button.onclick=async()=>{
 button.disabled=true;
 try{
  let response=await fetch('/api/pump/reset-stopped',{method:'POST'}),s=await response.json();show(s);
  if(!response.ok)return;
  const ticket=s.ticket;
  for(let n=0;n<16&&s.state==='pending';n++){
   await new Promise(resolve=>setTimeout(resolve,250));
   response=await fetch('/api/pump/reset-stopped',{cache:'no-store'});
   if(!response.ok)throw new Error('status');
   s=await response.json();
   if(s.ticket!==ticket){show({state:'unknown',ticket});return;}show(s);
  }
  if(s.state==='pending')show({state:'unknown',ticket});
 }catch(e){show({state:'unknown'});}finally{button.disabled=false;}
};
</script></html>)HTML";
}

namespace HmiWebInternal {
void registerPumpDiagnosticRoutes() {
  server.on("/diagnostics/pump", HTTP_GET, []() {
    server.send_P(200, "text/html; charset=utf-8", PAGE);
  });
  server.on("/api/pump/reset-stopped", HTTP_POST, requestReset);
  server.on("/api/pump/reset-stopped", HTTP_GET, []() {
    refreshResetResult();
    sendResetStatus(200);
  });
}
}
