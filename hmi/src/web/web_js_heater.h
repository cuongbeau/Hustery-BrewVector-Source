#pragma once
#include <Arduino.h>
// Generated from hmi/web/heater.js
static const char WEB_JS_HEATER[] PROGMEM = R"JS(
const HeaterTab=(()=>{
const specs={
 target:{label:'Nhiệt thân mục tiêu °C',min:30,max:130,step:.1,group:'temperature',help:'Setpoint trên thân thermoblock; không phải nhiệt nước ra.'},
 ceiling:{label:'Ngắt quá nhiệt °C',min:35,max:150,step:.1,group:'temperature',help:'Khóa heater khi cảm biến đạt ngưỡng này; phải cao hơn mục tiêu ít nhất 5°C.'},
 stableSeconds:{label:'Chờ mở Brew (giây)',min:0,max:300,step:1,group:'temperature',help:'Thời gian nhiệt phải đạt ngưỡng Ready trước khi cho phép Brew.'},
 kp:{label:'Kp · %/°C',min:0,max:100,step:.01,group:'pid',help:'Phản ứng tức thời theo sai lệch nhiệt độ; quá cao dễ dao động.'},
 ki:{label:'Ki · %/(°C·s)',min:0,max:10,step:.01,group:'pid',help:'Bù sai lệch kéo dài; quá cao dễ tích lũy và vượt nhiệt.'},
 kd:{label:'Kd · %·s/°C',min:0,max:100,step:.01,group:'pid',help:'Hãm theo tốc độ thay đổi nhiệt; quá cao dễ phản ứng với nhiễu.'},
 maxPower:{label:'Trần giữ nhiệt %',min:1,max:100,step:.1,group:'power',help:'Giới hạn công suất khi Preheat và giữ nhiệt thông thường.'},
 waterMaxPower:{label:'Trần khi chạy nước %',min:1,max:100,step:.1,group:'power',help:'Giới hạn tổng PID + bù nhiệt khi dòng nước đủ điều kiện.'},
 idleMinutes:{label:'Tự nghỉ sau (phút)',min:1,max:240,step:1,group:'power',help:'Thời gian không chạy nước trước khi tự tắt heater.'},
 sleepDelaySeconds:{label:'Trễ tắt thêm (giây)',min:0,max:3600,step:1,group:'power',help:'Khoảng chờ cộng thêm sau thời gian tự nghỉ.'},
 heaterW:{label:'Công suất heater W',min:100,max:4000,step:10,group:'water',help:'Công suất danh định ghi trên heater; dùng để đổi watt nhiệt thành phần trăm.'},
 inletC:{label:'Nhiệt độ nước vào °C',min:0,max:60,step:.1,group:'water',help:'Nhiệt độ ước tính của nước trong bình trước khi vào thermoblock.'},
 ffScale:{label:'Hệ số bù 0–1',min:0,max:1,step:.01,group:'water',help:'1,00 dùng toàn bộ mô hình; 0,50 dùng một nửa; 0 tắt phần bù.'}
};
const groups=[['temperature','Nhiệt độ & an toàn'],['pid','Hệ số PID'],['power','Công suất & tự nghỉ'],['water','Bù nhiệt khi có dòng nước']];
const $=id=>document.getElementById('heat_'+id);
for(const [group,title] of groups){
 const card=document.createElement('section');card.className='heater-field-group';
 const heading=document.createElement('h4');heading.textContent=title;card.append(heading);
 const grid=document.createElement('div');grid.className='heater-field-group-grid';card.append(grid);
 for(const [k,v] of Object.entries(specs).filter(([,s])=>s.group===group)){
  const label=document.createElement('label');label.textContent=v.label;
  const input=document.createElement('input');Object.assign(input,{id:'heat_'+k,type:'number',min:v.min,max:v.max,step:String(v.step)});label.append(input);
  const help=document.createElement('small');help.className='heater-field-help';help.textContent=v.help;label.append(help);grid.append(label);
 }
 $('fields').append(card);
}
let active=false,initial=false,last=null,rows=[],pending=false,online=false,polling=false,pollTimer=null,pollController=null,commandEpoch=0;
const modes=['OFF','PID','THỬ CÔNG SUẤT','LỖI'];const faults=['','LỖI CẢM BIẾN','MẪU NHIỆT QUÁ HẠN','QUÁ NHIỆT — NGẮT NGUỒN MÁY','VÒNG LẶP MAIN BỊ TREO'];
function controls(){document.querySelectorAll('.heat-mut').forEach(b=>b.disabled=pending||!online);}
function sensorIssues(mask){const names=[[1,'OC: chip báo hở đầu dò'],[2,'SCG: chip báo chạm GND'],[4,'SCV: chip báo chạm VCC'],[8,'Khung SPI không hợp lệ'],[16,'Nhiệt ngoài dải -20…500°C'],[32,'Không đọc được bus']];return names.filter(([bit])=>mask&bit).map(([,label])=>label).join(' + ')||'Hợp lệ';}
function renderWindow(s){
 const e=$('sensorWindow');
 if(!s.online||!s.sensorDiagnosticsFresh||!Number.isFinite(s.sensorWindowSamples)){e.textContent='Thống kê 5 giây chưa có hoặc quá hạn; cần Main và HMI hỗ trợ bản mới.';return;}
 const ratio=(errors,total)=>total?`${(100*errors/total).toFixed(1)}% (${errors}/${total} mẫu)`:'Chưa có mẫu';
 const ready=s.sensorWindowReady&&s.sensorWindowSamples>0;
 const label=!ready?'Chưa đủ 5 giây liên tục hoặc đọc bị gián đoạn':s.sensorWindowErrors*5<s.sensorWindowSamples?'Bình thường':'Cảnh báo từ 20%';
 e.textContent=`5 giây gần nhất: ${ratio(s.sensorWindowErrors,s.sensorWindowSamples)} — ${label}\nChuỗi lỗi hiện tại: ${s.sensorConsecutiveErrors} mẫu | Dài nhất từ boot: ${s.sensorMaxConsecutiveErrors} mẫu\nĐỉnh lỗi kể từ khi Main khởi động: ${ratio(s.sensorPeakWindowErrors,s.sensorPeakWindowSamples)}\nCửa sổ đạt / vượt 20%: ${s.sensorWindowsAtOrAbove20} / ${s.sensorEvaluatedWindows} lần đánh giá\nĐây là thống kê chẩn đoán; chính sách điều khiển dùng bộ đếm 20 mẫu riêng ở bên dưới.`;
}
function renderPolicy(s){
 const e=$('sensorPolicy');
 if(!s.online||!s.sensorDiagnosticsFresh||!s.sensorControlCapacity){e.textContent='Chính sách cảm biến chưa có hoặc quá hạn; cần Main và HMI hỗ trợ bản mới.';return;}
 const state=s.sensorControlErrors>=s.sensorControlTripErrors?'KHÓA':s.sensorControlErrors>=s.sensorControlPauseErrors?'TẠM DỪNG':s.sensorControlErrors>=s.sensorControlWarnErrors?'CẢNH BÁO':'BÌNH THƯỜNG';
 e.textContent=`Bộ đếm điều khiển: ${s.sensorControlErrors} lỗi / ${s.sensorControlSamples} mẫu — ${state}\nCảnh báo ${s.sensorControlWarnErrors}/${s.sensorControlCapacity}; tạm dừng SSR ${s.sensorControlPauseErrors}/${s.sensorControlCapacity}; tự chạy lại khi còn tối đa ${s.sensorControlResumeErrors}/${s.sensorControlCapacity}; khóa ${s.sensorControlTripErrors}/${s.sensorControlCapacity}.\nĐọc mỗi ${s.sensorSamplePeriodMs} ms. Quá ${s.sensorPauseGoodAgeMs} ms không có mẫu tốt: tạm dừng. Quá ${s.sensorMaxGoodAgeMs} ms: khóa.`;
}
function renderPid(s){
 const e=$('pidDetails');
 if(!s.online||!s.sensorDiagnosticsFresh||!s.pidPeriodMs){e.textContent='Dữ liệu PID chưa có hoặc quá hạn; cần Main và HMI bản mới.';return;}
 e.textContent=`PID của BrewVector · Chu kỳ ${s.pidPeriodMs} ms\nBù nhiệt: ${s.feedforwardKff>0?'BẬT':'TẮT'} | FF ${(Number.isFinite(s.thermalFeedforward)?s.thermalFeedforward.toFixed(2):'—')}%\nFlow đầu vào: ${(Number.isFinite(s.inletFlow)?s.inletFlow.toFixed(2):'—')} ml/s | ${s.flowQualified?'ước tính từ duty bơm và áp suất':'chưa đủ điều kiện hoặc ngoài vùng mô hình'}\n${s.sensorPaused?'Tạm dừng cảm biến — công suất 0%.':'Tổng P + I + D + bù nhiệt được giới hạn bởi trần công suất đã cài.'}`;
}function isSensorPaused(s){return s.online&&!s.fault&&(s.mode===1||s.mode===2)&&s.sensorDiagnosticsFresh&&s.sensorPaused;}
function renderSensor(s){
 renderPid(s);renderWindow(s);renderPolicy(s);
 const badge=$('diagBadge'),warn=s.online&&s.sensorDiagnosticsFresh&&((s.sensorControlErrors||0)>=(s.sensorControlWarnErrors||Infinity)||!s.valid);
 badge.textContent=s.fault?'Lỗi đã khóa':s.sensorPaused?'Heater tạm dừng':warn?'Có mẫu lỗi':'';
 badge.className='heater-diagnostic-badge'+(badge.textContent?' visible':'');
 const e=$('sensorDiagnostic');if(!s.online||!s.sensorDiagnosticsFresh){e.textContent='Chẩn đoán cảm biến chưa có hoặc đã quá hạn; cần Main có hỗ trợ.';return;}
 const hex=n=>'0x'+(n>>>0).toString(16).padStart(8,'0').toUpperCase(),totalRatio=s.sensorTotal?(100*s.sensorErrors/s.sensorTotal).toFixed(1)+'%':'—';
 e.textContent=`Mẫu lỗi / tổng: ${s.sensorErrors} / ${s.sensorTotal} (${totalRatio})\nMẫu hiện tại: ${sensorIssues(s.sensorIssues)} | ${hex(s.sensorFrame)}\nLỗi gần nhất: ${s.sensorErrors?sensorIssues(s.sensorLastIssues):'Chưa ghi nhận'} | ${hex(s.sensorLastErrorFrame)} | Main ${s.sensorLastErrorAt} ms\nBộ đếm từ lúc Main khởi động; Xóa lỗi không xóa lịch sử này. Mã chip là dữ liệu quan sát, chưa kết luận phần cứng hỏng.`;
}
async function heaterRequest(options={},controller=null){
 controller=controller||(typeof AbortController==='function'?new AbortController():null);
 let timer;
 const timeout=new Promise((_,reject)=>{timer=setTimeout(()=>{if(controller)controller.abort();reject(Error('Hết thời gian chờ Web; chưa xác nhận kết quả.'));},8000);});
 try{return await Promise.race([timeout,(async()=>{const r=await fetch('/api/heater',{cache:'no-store',...options,...(controller?{signal:controller.signal}:{})});if(!r.ok)throw Error('HTTP '+r.status+': '+(r.text?await r.text():''));return r.json();})()]);}
 finally{clearTimeout(timer);}
}
async function getState(controller=null){return heaterRequest({},controller);}
async function send(op){if(op!==0&&pending)return;if(op!==0&&!online){$('reply').textContent='Chưa có dữ liệu Main mới; chờ kết nối.';return;}const epoch=++commandEpoch;pending=true;if(pollController)pollController.abort();controls();try{const b={op};if(op===1)for(const k in specs){const v=Number($(k).value);if(!$(k).value||!Number.isFinite(v))throw Error('Điền đủ thông số');b[k]=v;}if(op===1){b.autoSleep=$('autoSleep').checked;b.ffEnabled=$('ffEnabled').checked;}if(op===3){b.testPower=Number($('testPower').value);b.testSeconds=Number($('testSeconds').value);}const result=await heaterRequest({method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});if(epoch!==commandEpoch)return;const t=result.ticket;$('reply').textContent='Chờ Main xác nhận #'+t;for(let n=0;n<8;n++){await new Promise(r=>setTimeout(r,250));if(epoch!==commandEpoch)return;const s=await getState();if(epoch!==commandEpoch)return;if(s.online&&s.ticket===t){$('reply').textContent=s.result===3&&op===4?'Chưa xóa được: cần mẫu nhiệt hợp lệ, còn mới, dưới ngưỡng ngắt và bộ đếm lỗi dưới giới hạn.':s.result===3&&(op===2||op===3)&&s.ceiling<=0?'Chưa cấu hình ngưỡng ngắt quá nhiệt. Heater vẫn OFF.':['Main đã thực hiện','Thông số không hợp lệ','Main bận / cần OFF và Ready','Chưa an toàn hoặc lỗi lưu cấu hình'][s.result];return;}}throw Error('Chưa xác nhận kết quả; không tự gửi lại.');}catch(e){if(epoch===commandEpoch)$('reply').textContent=e.message;}finally{if(epoch===commandEpoch){pending=false;controls();}}}
function renderLive(s,paused){
 const finite=(v,d=1)=>Number.isFinite(v)?v.toFixed(d):'—';
 const state=s.fault?faults[s.fault]:paused?'Tạm dừng':s.idleExpired?'Tự nghỉ':s.mode===1?(s.ready?'READY':'PREHEAT'):modes[s.mode];
 const pid=(Number(s.p)||0)+(Number(s.i)||0)+(Number(s.d)||0),flowOk=s.flowQualified&&Number.isFinite(s.inletFlow);
 $('liveTemp').textContent=finite(s.filtered)+' °C';$('liveTarget').textContent=finite(s.target)+' °C';
 $('livePower').textContent=finite(s.power)+'%';$('liveSsr').textContent=s.relay?'ON':'OFF';$('liveState').textContent=state;
 $('liveFlow').textContent=flowOk?finite(s.inletFlow,2)+' ml/s · ước tính':'— ml/s';
 $('livePid').textContent=finite(pid)+'%';$('liveFf').textContent=(Number.isFinite(s.thermalFeedforward)?'+':'')+finite(s.thermalFeedforward)+'%';
 $('liveTotal').textContent=finite(s.power)+'% / '+finite(s.activeMaxPower)+'%';
 $('legendTemp').textContent=finite(s.filtered)+' °C';$('legendTarget').textContent=finite(s.target)+' °C';$('legendPower').textContent=finite(s.power)+'%';
}
function draw(){
 const c=$('plot'),x=c.getContext('2d'),w=Math.max(300,c.clientWidth),h=240;c.width=w;c.height=h;
 const left=43,right=43,top=16,bottom=31,pw=w-left-right,ph=h-top-bottom,pts=rows.slice(-600);
 x.clearRect(0,0,w,h);x.font='10px sans-serif';x.lineWidth=1;
 const tempY=v=>top+(130-v)/(130-20)*ph,powerY=v=>top+(100-v)/100*ph;
 x.strokeStyle='#263543';x.fillStyle='#83909d';x.textBaseline='middle';
 for(let t=20;t<=120;t+=20){const y=tempY(t);x.beginPath();x.moveTo(left,y);x.lineTo(w-right,y);x.stroke();x.textAlign='right';x.fillText(String(t),left-7,y);}
 for(let p=0;p<=100;p+=20){const y=powerY(p);x.textAlign='left';x.fillText(String(p),w-right+7,y);}
 x.strokeStyle='#526170';x.strokeRect(left,top,pw,ph);x.fillStyle='#a6b1bc';x.textBaseline='top';x.textAlign='left';x.fillText('Nhiệt °C',left,2);x.textAlign='right';x.fillText('Công suất %',w-right,2);
 const start=pts.length?Number(pts[0].uptime)||0:0,end=pts.length?Number(pts[pts.length-1].uptime)||start+1000:start+1000,span=Math.max(1000,end-start);
 x.fillStyle='#83909d';x.textBaseline='top';
 for(let n=0;n<=4;n++){const px=left+pw*n/4;x.strokeStyle='#263543';x.beginPath();x.moveTo(px,top);x.lineTo(px,top+ph);x.stroke();x.textAlign=n===0?'left':n===4?'right':'center';x.fillText(Math.round(span*n/4000)+'s',px,top+ph+6);}
 if(!pts.length)return;
 const series=[['filtered','#ffaf54',tempY,false],['target','#ffffff',tempY,true],['power','#62d9b0',powerY,false]];
 for(const [key,color,yOf,dashed] of series){x.strokeStyle=color;x.lineWidth=2;if(x.setLineDash)x.setLineDash(dashed?[6,5]:[]);x.beginPath();let drew=false;for(const p of pts){if(!Number.isFinite(p[key]))continue;const px=left+(Number(p.uptime)-start)/span*pw,py=yOf(p[key]);drew?x.lineTo(px,py):x.moveTo(px,py);drew=true;}if(drew)x.stroke();}
 if(x.setLineDash)x.setLineDash([]);
}
async function poll(){
 if(!active)return;
 if(polling)return;clearTimeout(pollTimer);
 if(pending){pollTimer=setTimeout(poll,500);return;}
 polling=true;controls();let gotState=false;
 pollController=typeof AbortController==='function'?new AbortController():null;
 const stale=setTimeout(()=>{online=false;controls();$('alarm').className='heater-alarm danger';$('alarm').textContent='ĐANG CHỜ DỮ LIỆU WEB — KHÔNG XÁC NHẬN HEATER ĐÃ TẮT';},2500);
 try{const s=await getState(pollController);if(pending)return;gotState=true;
 if(!s || typeof s.online!=='boolean')throw Error('Phản hồi không hợp lệ');
 if(s.online && ['filtered','raw','target','power','p','i','d'].some(k=>!Number.isFinite(s[k])))throw Error('Thiếu dữ liệu nhiệt/PID');
 online=s.online;renderSensor(s);const paused=isSensorPaused(s),bad=!s.online||s.fault;
 $('alarm').className=bad?'heater-alarm danger':paused?'heater-alarm warning':'heater-alarm';
 $('alarm').textContent=!s.online?'MẤT LIÊN LẠC — HEATER CHƯA XÁC NHẬN OFF':s.fault===1&&s.valid&&s.sampleAge<=(s.sensorDiagnosticsFresh?s.sensorPauseGoodAgeMs:1000)?'LỖI CẢM BIẾN · HEATER OFF':s.fault?faults[s.fault]:paused?'HEATER TẠM DỪNG':s.idleExpired?'TỰ NGHỈ':s.mode===1?(s.ready?'READY · PID':'PREHEAT · PID'):modes[s.mode];
 if(s.online){
  renderLive(s,paused);
  const applied=$('applied');if(applied)applied.textContent=`Main đã nhận: bù nhiệt ${s.ffEnabled?'BẬT':'TẮT'} · trần giữ ${s.maxPower}% · trần khi chạy nước ${s.waterMaxPower}% · trần đang dùng ${s.activeMaxPower}%`;
  if(!initial){for(const k in specs)$(k).value=s[k];$('autoSleep').checked=s.autoSleep;$('ffEnabled').checked=s.ffEnabled;initial=true;}
  if(last!==s.uptime){last=s.uptime;rows.push({...s,localTime:new Date().toISOString()});if(rows.length>7200)rows.shift();draw();}
 }
 }catch(e){if(active&&!pending){online=false;renderSensor({online:false});$('alarm').className='heater-alarm danger';$('alarm').textContent=(gotState?'DỮ LIỆU WEB LỖI':'MẤT KẾT NỐI WEB')+' — HEATER CHƯA XÁC NHẬN OFF';$('liveState').textContent=e.message;}}
 finally{clearTimeout(stale);pollController=null;polling=false;controls();if(active)pollTimer=setTimeout(poll,document.hidden?5000:500);}
}
function setActive(value){active=!!value;clearTimeout(pollTimer);if(!active){if(pollController)pollController.abort();return;}if(!polling)pollTimer=setTimeout(poll,0);}
function download(){const keys=['localTime','uptime','raw','filtered','target','power','p','i','d','relay','mode','fault','sampleAge','idleRemaining','sensorDiagnosticsFresh','sensorSampledAt','sensorErrors','sensorTotal','sensorIssues','sensorLastIssues','sensorWindowReady','sensorWindowSamples','sensorWindowErrors','sensorPeakWindowSamples','sensorPeakWindowErrors','sensorConsecutiveErrors','sensorMaxConsecutiveErrors','sensorEvaluatedWindows','sensorWindowsAtOrAbove20','sensorControlSamples','sensorControlErrors','sensorControlCapacity','sensorControlWarnErrors','sensorControlPauseErrors','sensorControlResumeErrors','sensorControlTripErrors','sensorSamplePeriodMs','sensorMaxGoodAgeMs','sensorPauseGoodAgeMs','sensorPaused','sensorDensityPaused','thermalFeedforward','inletFlow','feedforwardKff','flowQualified','inletValveOpen','pidPeriodMs','heaterW','inletC','ffScale','ffEnabled','waterMaxPower','activeMaxPower','inletPressure','inletDuty'];const blob=new Blob([keys.join(',')+'\n'+rows.map(s=>keys.map(k=>s[k]).join(',')).join('\n')],{type:'text/csv'});const a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='heater-'+Date.now()+'.csv';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000);}return {send,download,setActive};
})();

)JS";
