// File: web_js_calib.h
#ifndef WEB_JS_CALIB_H
#define WEB_JS_CALIB_H

#include <Arduino.h>

static const char WEB_JS_CALIB[] PROGMEM = R"JS(
  // Thermal display calibration is independent of heater PID/FF.
  var _thermalBase=null, _thermalBusy=false;
  function thermalControls(busy, loaded){
    _thermalBusy=busy;
    el('thermal_offset').disabled=busy || !loaded;
    el('thermal_save').disabled=busy || !loaded;
    el('thermal_reload').disabled=busy;
  }
  function thermalFailure(r){
    _thermalBase=null;
    thermalControls(false,false);
    el('thermal_msg').textContent=(r && (r.error || r.message) ? (r.error || r.message) : 'Không xác nhận được kết quả')+' — bấm Reload trước khi lưu lại.';
    el('thermal_msg').className='err small';
  }
  function loadThermal(){
    if(_thermalBusy)return;
    thermalControls(true,false);
    el('thermal_msg').textContent='Đang đọc...';
    httpGetJSON('/cal/thermal',function(r){
      if(!r || !r.ok || !Number.isFinite(r.offset_c) || r.offset_c<0 || r.offset_c>60){thermalFailure(r);return;}
      _thermalBase=r.offset_c;
      el('thermal_offset').value=r.offset_c.toFixed(1);
      el('thermal_msg').textContent='Đã đọc offset đang lưu trên Main';
      el('thermal_msg').className='small';
      thermalControls(false,true);
    },thermalFailure);
  }
  function saveThermal(){
    if(_thermalBusy || _thermalBase===null)return;
    var raw=el('thermal_offset').value.trim(), value=Number(raw);
    if(!raw || !Number.isFinite(value) || value<0 || value>60){
      el('thermal_msg').textContent='Nhập offset từ 0 đến 60°C';
      el('thermal_msg').className='err small';return;
    }
    thermalControls(true,true);
    el('thermal_msg').textContent='Đang lưu...';
    httpPostJSON('/cal/thermal',{offset_c:value,expected_offset_c:_thermalBase},function(r){
      if(!r || !r.ok || !Number.isFinite(r.offset_c)){thermalFailure(r);return;}
      _thermalBase=r.offset_c;
      el('thermal_offset').value=r.offset_c.toFixed(1);
      el('thermal_msg').textContent='Đã lưu offset trên Main';
      el('thermal_msg').className='ok small';
      thermalControls(false,true);
    },thermalFailure);
  }

  // ===== Calibration: status + pressure + flow =====
  var _statusTimer=null, _statusBusy=false;

  function refreshStatus(){
    if(_statusBusy || document.hidden)return;
    _statusBusy=true;
    httpGetJSON('/status', function(s){
      _statusBusy=false;
      el('status').textContent = JSON.stringify(s,null,2);
      if(typeof s.ZCHz !== 'undefined'){
        var hz = s.ZCHz.toFixed(1);
        el('zc_text').textContent = 'Zero-Cross: ' + hz + ' Hz';
        var isOk = (hz > 45 && hz < 55) || (hz > 80 && hz < 125);
        el('zc_dot').className = isOk ? 'dot ok' : 'dot';
        
        var mode = 'Standard';
        if(hz > 30 && hz < 65) mode = 'Ghost-Pulse (Half-wave)';
        else if(hz > 80) mode = 'Full-wave (Native)';
        else if(hz < 10) mode = 'No Signal';
        el('pump_mode_pill').textContent = mode;
        el('pump_mode_pill').className = (hz > 30) ? 'pill ok' : 'pill bad';
      }
    }, function(){ _statusBusy=false;el('status').textContent='(Disconnected)'; });
  }

  function setZero(){
    el('presmsg').textContent='Đang đo...';
    httpPostJSON('/cal/pressure',{cmd:'zero'}, function(r){
      el('presmsg').textContent=(r&&r.ok)?'✓ Đã lưu Offset':'⚠️ Error';
      el('presmsg').className=(r&&r.ok)?'ok small':'err small';
      if(r&&r.ok) loadPress();
    }, function(){ el('presmsg').textContent='⚠️ Error'; el('presmsg').className='err small'; });
  }

  function setArb(){
    var b=Number(el('pbar').value);
    if(!(b>0)){ el('presmsg').textContent='Nhập bar > 0'; el('presmsg').className='err small'; return; }
    el('presmsg').textContent='Đang đo...';
    httpPostJSON('/cal/pressure',{cmd:'p_at',bar:b}, function(r){
      el('presmsg').textContent=(r&&r.ok)?('✓ Scale @ '+b+' bar'):'⚠️ Error';
      el('presmsg').className=(r&&r.ok)?'ok small':'err small';
      if(r&&r.ok) loadPress();
    }, function(){ el('presmsg').textContent='⚠️ Error'; el('presmsg').className='err small'; });
  }

  function loadPress(){
    httpGetJSON('/cal/pressure', function(pc){
      if(!pc) return;
      el('po').value=(typeof pc.offset!=='undefined')?pc.offset:'';
      el('ps').value=(typeof pc.scale!=='undefined')?pc.scale:'';
    });
  }

  function loadFlow(){
    httpGetJSON('/cal/flow', function(j){
      if(!j) return;
      if(typeof j.q1_ml_s!=='undefined') el('f_q1').value=j.q1_ml_s;
      if(typeof j.q9_ml_s!=='undefined') el('f_q9').value=j.q9_ml_s;
      previewFlowModel();
    });
  }

  function previewFlowModel(){
    var q1=Number(el('f_q1').value),q9=Number(el('f_q9').value),box=el('flow_preview');
    if(!box)return;
    if(!(q1>0&&q9>0&&q9<q1)){box.textContent='Cần Q1 > Q9 > 0.';box.className='err small';return;}
    var q=function(p){return q1+(q9-q1)*(p-1)/8;};
    box.textContent='Q(P) = '+q1.toFixed(2)+' + ('+(q9-q1).toFixed(2)+') × (P−1)/8  ·  '+
      'Q1 '+q(1).toFixed(2)+'  ·  Q3 '+q(3).toFixed(2)+'  ·  Q6 '+q(6).toFixed(2)+'  ·  Q9 '+q(9).toFixed(2)+' ml/s';
    box.className='small';
  }

  function saveFlow(){
    var q1=Number(el('f_q1').value),q9=Number(el('f_q9').value);
    if(!(q1>=0.5&&q1<=20&&q9>=0.25&&q9<q1)){
      el('fl_msg').textContent='Cần 0.5 ≤ Q1 ≤ 20 và 0.25 ≤ Q9 < Q1';el('fl_msg').className='err';return;
    }
    var data={q1_ml_s:q1,q9_ml_s:q9};
    
    httpPostJSON('/cal/flow', data, function(r){
      var ok=r && r.ok===true;
      el('fl_msg').textContent=ok?'Đã lưu':'Error';
      el('fl_msg').className=ok?'ok':'err';
      if(ok) loadFlow();
    }, function(){ el('fl_msg').textContent='Error'; el('fl_msg').className='err'; });
  }
)JS";

#endif // WEB_JS_CALIB_H
