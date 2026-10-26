#ifndef WEB_JS_HISTORY_H
#define WEB_JS_HISTORY_H

#include <Arduino.h>

// HMI TF control-trace browser embedded in the shared single-page web shell.
static const char WEB_JS_HISTORY[] PROGMEM = R"JS(
  var hmiHistTraces=[];
  var hmiHistCurrent=null;
  var hmiHistMode="overview";
  var hmiHistLoaded=false;
  var hmiHistTraceSupported=true,hmiHistGeneration=0,hmiHistListBusy=false;
  var hmiHistController=null;
  var hmiHistCompare=null,hmiHistCompareId=0,hmiHistHoverTime=null,hmiHistSelectedPhase=-1;
  function pauseHmiHistory(){
    ++hmiHistGeneration;
    if(hmiHistController)hmiHistController.abort();
    hmiHistController=null;
    if(hmiHistCurrent && hmiHistCurrent.loading)hmiHistCurrent=null;
    if(hmiHistCompare && hmiHistCompare.loading){hmiHistCompare=null;hmiHistCompareId=0;}
  }

  function histEsc(s){
    return String(s||"").replace(/[&<>"']/g,function(c){
      return {"&":"&amp;","<":"&lt;",">":"&gt;",'"':"&quot;","'":"&#39;"}[c];
    });
  }
  function histFmt(v,d){
    if(v===null || v===undefined || v==='')return '—';
    v=Number(v);
    return Number.isFinite(v)?v.toFixed(d===undefined?1:d):"—";
  }
  function histParseCsv(text){
    var lines=text.trim().split(/\r?\n/),profile="Unknown",head=null,rows=[],stop={},stepNames=[];
    lines.forEach(function(line){
      if(line.indexOf("# profile,")===0){ profile=line.slice(10).trim(); return; }
      if(line.indexOf('# step_name,')===0){
        var match=line.match(/^# step_name,(\d+),(.+)$/),step=match?Number(match[1]):-1;
        if(step>=0 && step<16 && match[2].trim())stepNames[step]=match[2].trim();
        return;
      }
      if(line.indexOf('# stop_')===0 || line.indexOf('# profile_exit_reason,')===0){
        var split=line.indexOf(','),key=line.slice(2,split),value=line.slice(split+1).trim();
        if(split>0 && value!=='')stop[key]=Number.isFinite(Number(value))?Number(value):value;
        return;
      }
      if(!line || line.charAt(0)==="#") return;
      if(!head){ head=line.split(","); return; }
      var cells=line.split(",");
      if(cells.length!==head.length) return;
      var row={}; head.forEach(function(key,i){ row[key]=cells[i].trim()===''?null:Number(cells[i]); });
      // Read both schema-17 names and existing traces written by older HMI.
      if(!histFinite(row.pump_flow_ml_s) && histFinite(row.flow_raw_gps))row.pump_flow_ml_s=row.flow_raw_gps;
      if(!histFinite(row.puck_flow_ml_s) && histFinite(row.flow_filtered_gps))row.puck_flow_ml_s=row.flow_filtered_gps;
      if(!histFinite(row.water_pumped_ml) && histFinite(row.water_pumped_g))row.water_pumped_ml=row.water_pumped_g;
      rows.push(row);
    });
    return {profile:profile,rows:rows,stop:stop,stepNames:stepNames,namesSource:stepNames.some(Boolean)?'trace':null};
  }
  function histFinalValues(shot){
    var rows=shot.rows||[],last=rows[rows.length-1]||{},stop=shot.stop||{};
    var valid=Number.isInteger(stop.stop_flags) && (stop.stop_flags&2)!==0 && Number.isFinite(stop.stop_scale_weight_g);
    var weight=valid?stop.stop_scale_weight_g:null;
    return {duration:Number.isFinite(stop.stop_elapsed_s)?stop.stop_elapsed_s:last.time_s,
      weight:weight,delta:valid && Number.isFinite(stop.stop_model_yield_g)?stop.stop_model_yield_g-weight:null};
  }
  function histFinite(v){return typeof v==='number' && Number.isFinite(v);}
  function histPeak(rows,key){
    var found=null;
    rows.forEach(function(r){if(histFinite(r[key]))found=found===null?r[key]:Math.max(found,r[key]);});
    return found;
  }
  function histAverage(rows,key){
    var sum=0,count=0;
    rows.forEach(function(r){if(histFinite(r[key])){sum+=r[key];count++;}});
    return count?sum/count:null;
  }
  function histModeTarget(row){
    return row.mode===1?row.target_command:row.controller_target_bar;
  }
  function histReason(stop){
    var labels={timeout:'Hết thời gian',pressure_above:'Đạt ngưỡng áp trên',pressure_below:'Đạt ngưỡng áp dưới',flow_above:'Đạt ngưỡng flow trên',flow_below:'Đạt ngưỡng flow dưới',weight:'Đạt yield',water_pumped:'Đạt lượng nước bơm',global_weight:'Đạt yield toàn shot',global_time:'Hết giờ toàn shot',manual_stop:'Dừng thủ công',profile:'Hoàn tất profile',brew_switch:'Tắt brew switch',hmi:'Dừng từ HMI',manual:'Dừng thủ công',auto_yield:'Đạt auto yield',hotwater_switch:'Hot water switch'};
    var reason=stop.profile_exit_reason && stop.profile_exit_reason!=='none'?stop.profile_exit_reason:stop.stop_reason;
    return labels[reason] || reason || 'Không ghi nhận';
  }
  function histDuration(shot){
    var value=histFinalValues(shot).duration;
    return histFinite(value)?Math.max(0,value):0;
  }
  function histPumpedAmount(rows){
    if(!rows.length)return null;
    var counter=rows.map(function(r){return r.water_pumped_ml;});
    if(counter.every(histFinite) && counter.every(function(v,i){return i===0 || v>=counter[i-1]-0.02;}))
      return Math.max(0,counter[counter.length-1]-counter[0]);
    var amount=0,used=false;
    for(var i=1;i<rows.length;i++){
      var dt=rows[i].time_s-rows[i-1].time_s,flow=rows[i-1].pump_flow_ml_s;
      if(histFinite(dt) && dt>0 && dt<1 && histFinite(flow)){amount+=Math.max(0,flow)*dt;used=true;}
    }
    return used?amount:null;
  }
  function histPhases(shot){
    if(shot._phases)return shot._phases;
    var rows=shot.rows||[],phases=[],stop=shot.stop||{};
    rows.forEach(function(row){
      if(!histFinite(row.time_s))return;
      var step=histFinite(row.step)?row.step:null,last=phases[phases.length-1];
      if(!last || last.step!==step){last={step:step,rows:[],start:row.time_s,end:row.time_s};phases.push(last);}
      last.rows.push(row);last.end=row.time_s;
    });
    var completeCounter=rows.length>0 && rows.every(function(r,i){return histFinite(r.water_pumped_ml) && (i===0 || r.water_pumped_ml>=rows[i-1].water_pumped_ml-0.02);});
    phases.forEach(function(p,i){
      var next=phases[i+1];
      if(next)p.end=next.start;
      else p.end=Math.max(p.end,histDuration(shot));
      p.duration=Math.max(0,p.end-p.start);
      p.pressure=histAverage(p.rows,'pressure_filtered_bar');
      p.mode=p.rows.find(function(r){return histFinite(r.mode);});
      p.mode=p.mode?p.mode.mode:0;
      var targets=p.rows.map(histModeTarget).filter(histFinite);
      p.target=targets.length?targets.reduce(function(a,b){return a+b;},0)/targets.length:null;
      p.flow=histAverage(p.rows,'pump_flow_ml_s');
      p.water=histPumpedAmount(p.rows);
      if(completeCounter){
        var previous=i?phases[i-1].rows[phases[i-1].rows.length-1].water_pumped_ml:0;
        p.water=Math.max(0,p.rows[p.rows.length-1].water_pumped_ml-previous);
      }
      p.weight=histPeak(p.rows,'scale_weight_g');
      p.reason=next?'Chuyển bước · không ghi lý do':histReason(stop);
    });
    shot._phases=phases;
    return phases;
  }
  function histPhaseLabel(shot,phase){
    if(phase.step===null)return 'Không rõ bước';
    var name=shot.stepNames && shot.stepNames[phase.step];
    return 'Bước '+(phase.step+1)+(name?' · '+name:'');
  }
  function histWarnings(shot){
    var rows=shot.rows||[],stop=shot.stop||{},warnings=[],final=histFinalValues(shot);
    if(!rows.length)return ['Trace không có mẫu dữ liệu.'];
    var overshoot=null,gaps=0,missingTime=0;
    rows.forEach(function(r,i){
      if(r.mode!==1 && histFinite(r.pressure_filtered_bar) && histFinite(r.controller_target_bar) && r.controller_target_bar>1)
        overshoot=Math.max(overshoot===null?0:overshoot,r.pressure_filtered_bar-r.controller_target_bar);
      if(histFinite(r.rx_gap))gaps+=Math.max(0,r.rx_gap);
      if(i && histFinite(r.time_s) && histFinite(rows[i-1].time_s) && r.time_s-rows[i-1].time_s>0.16)missingTime++;
    });
    if(overshoot!==null && overshoot>0.45)warnings.push('Áp suất vượt target tối đa '+histFmt(overshoot,1)+' bar tại một thời điểm.');
    if(gaps || missingTime)warnings.push('Trace có '+gaps+' gap báo từ Main, '+missingTime+' khoảng mẫu trên 160 ms.');
    if(final.weight===null)warnings.push('Không có cân hợp lệ lúc kết thúc; final yield chưa xác nhận.');
    if(!histFinite(stop.stop_elapsed_s))warnings.push('Trace thiếu footer kết thúc; thời gian dựa trên mẫu cuối.');
    if(histFinite(final.delta) && Math.abs(final.delta)>1.5)warnings.push('Model yield lệch cân '+histFmt(Math.abs(final.delta),1)+' g.');
    return warnings;
  }
  function histRenderList(){
    var list=el("hist_list"),count=el("hist_count");
    if(count) count.textContent=hmiHistTraces.length+" / 10 gần nhất";
    if(!list) return;
    var warning=hmiHistTraceSupported?'':'<div class="hist-empty" style="padding:10px;color:var(--accent2)">Main firmware needs an update to record detailed control traces.</div>';
    if(!hmiHistTraces.length){ list.innerHTML=warning+'<div class="hist-empty">No saved shots yet.</div>'; return; }
    list.innerHTML=warning+hmiHistTraces.map(function(t){
      var active=hmiHistCurrent&&hmiHistCurrent.id===t.id?" active":"";
      var sub=histEsc(t.profile)+' · '+(Number.isFinite(t.duration)?histFmt(t.duration)+' s · '+histFmt(t.peak)+' bar':histFmt(t.bytes/1024)+' KB');
      return '<button class="hist-shot'+active+'" onclick="histSelect('+t.id+')"><div class="hist-shot-top"><span>SHOT #'+String(t.id).padStart(6,"0")+'</span><span class="hist-tag">CONTROL TRACE</span></div><div class="hist-shot-sub">'+sub+'</div></button>';
    }).join("");
  }
  async function histReadCsv(id,generation){
    let text='',offset=0,total=Infinity;const decoder=new TextDecoder();
    while(offset<total){
      if(generation!==hmiHistGeneration)return null;
      const controller=new AbortController();
      hmiHistController=controller;
      const timer=setTimeout(()=>controller.abort(),8000);
      try{
        const r=await fetch('/traces/download?id='+id+'&offset='+offset,{cache:'no-store',signal:controller.signal});
        if(!r.ok)throw Error('Shot data unavailable');
        const chunk=new Uint8Array(await r.arrayBuffer());
        const bytes=chunk.length,header=r.headers.get('X-Trace-Total');
        const currentTotal=Number(header);
        if(header===null || !Number.isFinite(currentTotal) || currentTotal<offset || (!bytes && offset<currentTotal))throw Error('Invalid trace response');
        // Freeze size at the first read if the current trace is still growing.
        total=Math.min(total,currentTotal);
        const take=Math.min(bytes,total-offset);text+=decoder.decode(chunk.subarray(0,take),{stream:true});offset+=take;
      }finally{clearTimeout(timer);if(hmiHistController===controller)hmiHistController=null;}
      await new Promise(resolve=>setTimeout(resolve,20));
    }
    return text+decoder.decode();
  }
  function histSelect(id){
    var trace=hmiHistTraces.find(t=>t.id===id);if(!trace)return;
    if(hmiHistCurrent && hmiHistCurrent.id===id && !hmiHistCurrent.error)return;
    if(hmiHistController)hmiHistController.abort();
    const generation=++hmiHistGeneration;
    hmiHistCompare=null;hmiHistCompareId=0;hmiHistHoverTime=null;hmiHistSelectedPhase=-1;
    hmiHistCurrent={id:trace.id,profile:trace.profile,bytes:trace.bytes,loading:true};
    histRenderList();histRenderDetail();
    histReadCsv(id,generation).then(text=>{
      if(generation!==hmiHistGeneration || text===null)return;
      const data=histParseCsv(text);
      hmiHistCurrent={id:trace.id,profile:data.profile,bytes:trace.bytes,rows:data.rows,stop:data.stop,stepNames:data.stepNames,namesSource:data.namesSource,csv:text,loading:false};
      histResolveProfileNames(hmiHistCurrent);
      histRenderDetail();
    }).catch(e=>{
      if(generation!==hmiHistGeneration)return;
      hmiHistCurrent={id:trace.id,profile:trace.profile,error:e.message,loading:false};histRenderDetail();
    });
  }
  function histDownload(){
    if(!hmiHistCurrent || !hmiHistCurrent.csv)return;
    const url=URL.createObjectURL(new Blob([hmiHistCurrent.csv],{type:'text/csv'}));
    const a=document.createElement('a');a.href=url;a.download='brewvector_trace_'+String(hmiHistCurrent.id).padStart(6,'0')+'.csv';a.click();
    setTimeout(()=>URL.revokeObjectURL(url),1000);
  }
  function histProfileNameMatches(label,name){
    var current=String(label||'').trim().toLowerCase(),saved=String(name||'').trim().toLowerCase();
    return current===saved || (saved.length>=23 && current.slice(0,23)===saved.slice(0,23));
  }
  function histResolveProfileNames(shot){
    if(!shot || shot.namesSource || shot.namesPending || shot.namesTried || !shot.profile)return;
    if(typeof cacheProfiles!=='undefined'){
      var cached=cacheProfiles.filter(function(p){return histProfileNameMatches(p.label,shot.profile) && p._loaded && Array.isArray(p.phases);});
      if(cached.length===1){
        shot.stepNames=cached[0].phases.map(function(p){return typeof p.name==='string'?p.name.trim():'';});
        shot.namesSource='current';shot.namesTried=true;return;
      }
    }
    shot.namesPending=true;
    fetch('/profiles',{cache:'no-store'}).then(function(response){
      if(!response.ok)throw Error('Profile list unavailable');
      return response.json();
    }).then(function(data){
      if(hmiHistCurrent!==shot || !data.ok || !Array.isArray(data.profiles))throw Error('Profile list unavailable');
      var matches=data.profiles.filter(function(p){return histProfileNameMatches(p.label,shot.profile) && Number.isInteger(p.index);});
      if(matches.length!==1)throw Error('Profile no longer matches trace');
      return fetch('/profile/detail?index='+encodeURIComponent(matches[0].index),{cache:'no-store'});
    }).then(function(response){
      if(!response.ok)throw Error('Profile detail unavailable');
      return response.json();
    }).then(function(data){
      if(hmiHistCurrent!==shot || !data.ok || !data.profile || !histProfileNameMatches(data.profile.label,shot.profile) || !Array.isArray(data.profile.phases))throw Error('Profile detail changed');
      shot.stepNames=data.profile.phases.map(function(p){return typeof p.name==='string'?p.name.trim():'';});
      shot.namesSource='current';shot.namesPending=false;shot.namesTried=true;
      histRenderDetail();
    }).catch(function(error){
      shot.namesError=error.message||'Profile unavailable';
      shot.namesPending=false;shot.namesTried=true;
      if(hmiHistCurrent===shot)histRenderDetail();
    });
  }
  function histRetryProfileNames(){
    if(!hmiHistCurrent || hmiHistCurrent.namesPending)return;
    hmiHistCurrent.namesTried=false;hmiHistCurrent.namesError=null;
    histResolveProfileNames(hmiHistCurrent);histRenderDetail();
  }
  function histSetCompare(value){
    var id=Number(value),trace=hmiHistTraces.find(function(t){return t.id===id;});
    if(hmiHistController)hmiHistController.abort();
    var generation=++hmiHistGeneration;
    hmiHistCompareId=trace && hmiHistCurrent && id!==hmiHistCurrent.id?id:0;
    hmiHistCompare=hmiHistCompareId?{id:id,loading:true}:null;
    histRenderDetail();
    if(!hmiHistCompareId)return;
    histReadCsv(id,generation).then(function(csv){
      if(generation!==hmiHistGeneration || csv===null)return;
      var data=histParseCsv(csv);
      hmiHistCompare={id:id,profile:data.profile,rows:data.rows,stop:data.stop};
      histRenderDetail();
    }).catch(function(e){
      if(generation!==hmiHistGeneration)return;
      hmiHistCompare={id:id,error:e.message};histRenderDetail();
    });
  }
  function histSetMode(mode){hmiHistMode=mode;hmiHistHoverTime=null;histRenderDetail();}
  function histModeButton(id,label){return '<button class="hist-mode '+(hmiHistMode===id?'active':'')+'" onclick="histSetMode(\''+id+'\')">'+label+'</button>';}
  function histSeries(key,color,label,axis,dash,scale,mode){return {key:key,color:color,label:label,axis:axis||'left',dash:!!dash,scale:scale||1,mode:mode};}
  function histMetric(label,value,note){return '<div class="hist-metric"><span>'+label+'</span><b>'+value+'</b>'+(note?'<small>'+note+'</small>':'')+'</div>';}
  function histDelta(a,b,unit){return histFinite(a)&&histFinite(b)?(a-b>=0?'+':'')+histFmt(a-b,1)+' '+unit:'—';}
  function histCompareSummary(shot,other){
    var a=histFinalValues(shot),b=histFinalValues(other);
    return '<div class="hist-compare-summary"><b>Shot #'+String(other.id).padStart(6,'0')+'</b><span>Δ so với shot đang xem (cùng mốc 0 s):</span><span>Thời gian <b>'+histDelta(a.duration,b.duration,'s')+'</b></span><span>Yield <b>'+histDelta(a.weight,b.weight,'g')+'</b></span><span>Đỉnh áp <b>'+histDelta(histPeak(shot.rows,'pressure_filtered_bar'),histPeak(other.rows,'pressure_filtered_bar'),'bar')+'</b></span></div>';
  }
  function histPhaseTable(phases){
    if(!phases.length)return '<div class="hist-empty">Trace không có mốc bước.</div>';
    var shot=hmiHistCurrent;
    var source=shot.namesSource==='current'?'<div class="hist-name-source">Tên bước lấy từ profile hiện tại; profile có thể đã thay đổi sau khi pha shot này.</div>':
      !shot.namesSource?'<div class="hist-name-source">'+(shot.namesPending?'Đang tìm tên bước trong profile hiện tại…':'Trace cũ chưa lưu tên bước; chưa đọc được profile hiện tại. <button onclick="histRetryProfileNames()">Thử lại</button>')+'</div>':'';
    return source+'<div class="hist-table-scroll"><table class="hist-phase-table"><thead><tr><th>Bước</th><th>Khoảng thời gian</th><th>Thời lượng</th><th>Áp suất TB</th><th>Target TB</th><th>Flow bơm TB</th><th>Nước bơm ước tính</th><th>Kết thúc</th></tr></thead><tbody>'+phases.map(function(p,i){
      return '<tr class="'+(i===hmiHistSelectedPhase?'active':'')+'" onclick="histFocusPhase('+i+')"><td><b>'+histEsc(histPhaseLabel(shot,p))+'</b></td><td>'+histFmt(p.start,1)+'–'+histFmt(p.end,1)+' s</td><td>'+histFmt(p.duration,1)+' s</td><td>'+histFmt(p.pressure,1)+' bar</td><td>'+histFmt(p.target,1)+(p.mode===1?' ml/s':' bar')+'</td><td>'+histFmt(p.flow,1)+' ml/s</td><td>'+histFmt(p.water,1)+' ml</td><td>'+histEsc(p.reason)+'</td></tr>';
    }).join('')+'</tbody></table></div>';
  }
  function histFocusPhase(index){
    var p=histPhases(hmiHistCurrent)[index];if(!p)return;
    hmiHistSelectedPhase=index;hmiHistHoverTime=null;
    histUpdatePhaseSelection();histDrawChart();
  }
  function histUpdatePhaseSelection(){
    var box=el('hist_detail');if(!box || !box.querySelectorAll)return;
    [box.querySelectorAll('.hist-phase-strip button'),box.querySelectorAll('.hist-phase-table tbody tr')].forEach(function(items){
      items.forEach(function(item,i){if(item.classList)item.classList.toggle('active',i===hmiHistSelectedPhase);});
    });
  }
  function histRenderPhaseStrip(box,shot,phases,duration){
    var strip=box.querySelector && box.querySelector('.hist-phase-strip');if(!strip)return;
    strip.style.marginLeft='45px';strip.style.marginRight=(hmiHistMode==='overview'?45:15)+'px';
    var width=Math.max(1,strip.clientWidth);
    strip.innerHTML=phases.map(function(p,i){
      var left=100*Math.max(0,p.start)/duration,span=100*Math.max(0,p.end-p.start)/duration;
      var pixels=width*span/100,label=histPhaseLabel(shot,p);
      var content=pixels>=125?'<b>'+histEsc(label)+'</b><span>'+histFmt(p.start,1)+'–'+histFmt(p.end,1)+'s</span>':
        pixels>=25?'<b>'+(p.step===null?'?':p.step+1)+'</b>':'';
      return '<button class="'+(i===hmiHistSelectedPhase?'active':'')+'" style="left:'+left+'%;width:'+span+'%" title="'+histEsc(label+' · '+histFmt(p.start,1)+'–'+histFmt(p.end,1)+' s')+'" aria-label="'+histEsc(label)+'" onclick="histFocusPhase('+i+')">'+content+'</button>';
    }).join('');
  }
  function histRenderDetail(){
    var box=el('hist_detail');if(!box)return;
    if(!hmiHistCurrent){box.innerHTML='<div class="hist-empty">Chọn một shot để xem trace.</div>';return;}
    if(hmiHistCurrent.loading){box.innerHTML='<div class="hist-empty">Đang đọc control trace…</div>';return;}
    if(hmiHistCurrent.error){box.innerHTML='<div class="hist-empty">'+histEsc(hmiHistCurrent.error)+'</div>';return;}
    var shot=hmiHistCurrent,rows=shot.rows||[],final=histFinalValues(shot),peak=histPeak(rows,'pressure_filtered_bar'),phases=histPhases(shot);
    var water=histPeak(rows,'water_pumped_ml');if(water===null)water=histPumpedAmount(rows);
    var warnings=histWarnings(shot);
    var options='<option value="0">Không so sánh</option>'+hmiHistTraces.filter(function(t){return t.id!==shot.id;}).map(function(t){return '<option value="'+t.id+'"'+(hmiHistCompareId===t.id?' selected':'')+'>#'+String(t.id).padStart(6,'0')+' · '+histEsc(t.profile)+'</option>';}).join('');
    var compare=hmiHistCompare && !hmiHistCompare.loading && !hmiHistCompare.error?hmiHistCompare:null;
    box.innerHTML='<div class="hist-detail-head"><div class="hist-head-row"><div><div class="hist-eyebrow">SHOT ANALYZER · TF CONTROL TRACE</div><div class="hist-title">Shot #'+String(shot.id).padStart(6,'0')+' <span>· '+histEsc(shot.profile)+'</span></div><div class="hist-meta">'+rows.length+' mẫu · '+phases.length+' bước ghi nhận · '+histFmt(final.duration,1)+' s</div></div><button class="hist-download" onclick="histDownload()">Tải CSV</button></div><div class="hist-metrics">'+histMetric('FINAL YIELD',final.weight===null?'—':histFmt(final.weight,1)+' g',final.weight===null?'Cân chưa xác nhận':'Cân lúc dừng')+histMetric('THỜI GIAN',histFmt(final.duration,1)+' s','Từ footer hoặc mẫu cuối')+histMetric('ĐỈNH ÁP',histFmt(peak,1)+' bar','Áp suất đo')+histMetric('NƯỚC BƠM',water===null?'—':histFmt(water,1)+' ml','Bộ đếm ước tính')+histMetric('SAI LỆCH MODEL',final.delta===null?'—':histFmt(final.delta,1)+' g','Model − cân')+'</div></div><div class="hist-chart-panel"><div class="hist-panel-top"><div><b>Diễn biến shot</b><small>Di chuột trên đồ thị để xem mẫu · nhấn một bước để định vị</small></div><label class="hist-compare-control">So sánh với <select id="hist_compare" onchange="histSetCompare(this.value)">'+options+'</select></label></div>'+(hmiHistCompare && hmiHistCompare.loading?'<div class="hist-compare-state">Đang tải shot so sánh…</div>':hmiHistCompare && hmiHistCompare.error?'<div class="hist-compare-state">'+histEsc(hmiHistCompare.error)+'</div>':compare?histCompareSummary(shot,compare):'')+'<div class="hist-tabs">'+histModeButton('overview','Tổng quan')+histModeButton('flows','Lưu lượng')+histModeButton('control','Điều khiển')+histModeButton('model','Model')+'</div><div id="hist_legend" class="hist-legend"></div><canvas id="hist_chart" aria-label="Biểu đồ trace shot"></canvas><div id="hist_cursor" class="hist-cursor">Chạm hoặc di chuột trên đồ thị để xem giá trị theo thời gian.</div><div class="hist-phase-strip">'+phases.map(function(p,i){return '<button onclick="histFocusPhase('+i+')" style="flex:'+Math.max(.3,p.duration)+'"><b>'+(p.step===null?'?':p.step+1)+'</b><span>'+histFmt(p.start,1)+'–'+histFmt(p.end,1)+'s</span></button>';}).join('')+'</div></div><div class="hist-analysis"><div class="hist-section-head"><b>Phân tích theo bước</b><span>Giá trị tính từ mẫu trace · flow/nước bơm là ước tính</span></div>'+histPhaseTable(phases)+'</div><div class="hist-analysis"><div class="hist-section-head"><b>Ghi chú dữ liệu</b></div><div class="hist-warnings">'+(warnings.length?warnings.map(function(w){return '<div>'+histEsc(w)+'</div>';}).join(''):'<div>Không phát hiện bất thường theo các ngưỡng kiểm tra cơ bản.</div>')+'</div></div>';
    histRenderPhaseStrip(box,shot,phases,Math.max(1,histDuration(shot),compare?histDuration(compare):0));
    histDrawChart();
    var canvas=el('hist_chart');if(canvas){canvas.onmousemove=histChartMove;canvas.onmouseleave=function(){hmiHistHoverTime=null;histDrawChart();};canvas.ontouchstart=histChartMove;canvas.ontouchmove=histChartMove;canvas.ontouchend=function(){hmiHistHoverTime=null;histDrawChart();};canvas.onclick=histChartClick;}
  }
  function histChartSets(){
    if(hmiHistMode==='flows')return [histSeries('pump_flow_ml_s','#ffca28','Flow bơm'),histSeries('puck_flow_ml_s','#2dd4bf','Flow qua puck',null,true),histSeries('target_command','#ffca28','Target flow',null,true,1,1)];
    if(hmiHistMode==='control')return [histSeries('pressure_filtered_bar','#64d979','Áp suất'),histSeries('controller_target_bar','#9aa8bd','Target áp',null,true,1,0),histSeries('pump_pct','#ffca28','Công suất bơm × 0.12',null,false,.12)];
    if(hmiHistMode==='model')return [histSeries('scale_weight_g','#5aa9ff','Cân'),histSeries('model_yield_g','#ffca28','Model',null,true),histSeries('water_pumped_ml','#2dd4bf','Nước bơm ước tính')];
    return [histSeries('pressure_filtered_bar','#64d979','Áp suất'),histSeries('controller_target_bar','#9aa8bd','Target áp',null,true,1,0),histSeries('pump_flow_ml_s','#ffca28','Flow bơm'),histSeries('target_command','#ffca28','Target flow',null,true,1,1),histSeries('scale_weight_g','#5aa9ff','Cân','right')];
  }
  function histSeriesValue(row,series){
    if(series.mode===1 && row.mode!==1)return null;
    if(series.mode===0 && row.mode===1)return null;
    return row[series.key];
  }
  function histChartTime(event){
    var canvas=el('hist_chart');if(!canvas || !hmiHistCurrent)return null;
    var rect=canvas.getBoundingClientRect(),point=event.touches?event.touches[0]:event;
    if(!point)return null;
    var L=45,R=hmiHistMode==='overview'?45:15,pixel=point.clientX-rect.left;
    if(pixel<L || pixel>rect.width-R)return null;
    var duration=Math.max(1,histDuration(hmiHistCurrent),hmiHistCompare?histDuration(hmiHistCompare):0);
    return Math.max(0,Math.min(duration,(pixel-L)/Math.max(1,rect.width-L-R)*duration));
  }
  function histChartMove(event){
    hmiHistHoverTime=histChartTime(event);
    histDrawChart();
  }
  function histChartClick(event){
    var time=histChartTime(event);if(time===null || !hmiHistCurrent)return;
    var phases=histPhases(hmiHistCurrent);
    var index=phases.findIndex(function(p,i){return time>=p.start && (time<p.end || i===phases.length-1 && time<=p.end);});
    if(index<0)return;
    hmiHistSelectedPhase=index;
    histUpdatePhaseSelection();histDrawChart();
  }
  function histNearest(rows,time){
    if(!rows || !rows.length)return null;
    var lo=0,hi=rows.length-1;
    while(lo<hi){var mid=(lo+hi)>>1;if(rows[mid].time_s<time)lo=mid+1;else hi=mid;}
    return lo>0 && Math.abs(rows[lo-1].time_s-time)<Math.abs(rows[lo].time_s-time)?rows[lo-1]:rows[lo];
  }
  function histDrawChart(){
    var canvas=el('hist_chart'),legend=el('hist_legend');if(!canvas || !legend || !hmiHistCurrent)return;
    var rows=hmiHistCurrent.rows||[],other=hmiHistCompare && hmiHistCompare.rows?hmiHistCompare.rows:[],sets=histChartSets();
    legend.innerHTML=sets.map(function(s){return '<span><i class="hist-key" style="background:'+s.color+'"></i>'+s.label+'</span>';}).join('')+(other.length?'<span class="hist-compare-key">Nét đứt dài: shot so sánh</span>':'');
    var rect=canvas.getBoundingClientRect(),w=rect.width,h=rect.height;if(w<10 || h<10)return;
    var ratio=window.devicePixelRatio||1;canvas.width=Math.round(w*ratio);canvas.height=Math.round(h*ratio);
    var ctx=canvas.getContext('2d');if(!ctx)return;ctx.setTransform(ratio,0,0,ratio,0,0);
    var L=45,R=hmiHistMode==='overview'?45:15,T=20,B=29,pw=w-L-R,ph=h-T-B;
    var duration=Math.max(1,histDuration(hmiHistCurrent),other.length?histDuration(hmiHistCompare):0);
    var maxLeft=hmiHistMode==='overview'?12:1,maxRight=1;
    [rows,other].forEach(function(group){group.forEach(function(row){sets.forEach(function(s){var v=histSeriesValue(row,s);if(histFinite(v)){if(s.axis==='right')maxRight=Math.max(maxRight,v*s.scale);else maxLeft=Math.max(maxLeft,v*s.scale);}});});});
    maxLeft=Math.max(1,Math.ceil(maxLeft/5)*5);maxRight=Math.max(1,Math.ceil(maxRight/5)*5);
    function x(t){return L+pw*Math.max(0,Math.min(duration,t))/duration;}
    function y(v,axis){return T+ph*(1-Math.max(0,Math.min(axis==='right'?maxRight:maxLeft,v))/(axis==='right'?maxRight:maxLeft));}
    ctx.clearRect(0,0,w,h);
    var phases=histPhases(hmiHistCurrent);
    var selected=phases[hmiHistSelectedPhase];
    if(selected){
      ctx.fillStyle='rgba(53,201,242,.16)';
      ctx.fillRect(x(selected.start),T,Math.max(1,x(selected.end)-x(selected.start)),ph);
    }
    ctx.font='10px system-ui';ctx.strokeStyle='#303943';ctx.fillStyle='#9caaba';ctx.lineWidth=1;
    ctx.fillText(hmiHistMode==='model'?'g / ml':hmiHistMode==='flows'?'ml/s':'bar / ml/s',L,T-7);
    if(R>20)ctx.fillText('g',w-R+7,T-7);
    for(var i=0;i<=4;i++){var gy=T+ph*i/4;ctx.beginPath();ctx.moveTo(L,gy);ctx.lineTo(w-R,gy);ctx.stroke();ctx.fillText(histFmt(maxLeft*(1-i/4),0),5,gy+3);if(R>20)ctx.fillText(histFmt(maxRight*(1-i/4),0),w-R+7,gy+3);}
    for(var j=0;j<=4;j++){ctx.fillText(histFmt(duration*j/4,0)+'s',x(duration*j/4)-6,h-7);}
    phases.slice(1).forEach(function(p){ctx.strokeStyle='rgba(126,153,174,.5)';ctx.setLineDash([3,5]);ctx.beginPath();ctx.moveTo(x(p.start),T);ctx.lineTo(x(p.start),T+ph);ctx.stroke();ctx.setLineDash([]);});
    function draw(group,compare){
      sets.forEach(function(s){ctx.beginPath();ctx.strokeStyle=s.color;ctx.globalAlpha=compare?.55:1;ctx.lineWidth=compare?1.5:2.2;ctx.setLineDash(compare?[10,5]:s.dash?[3,4]:[]);var started=false;
        group.forEach(function(r){var v=histSeriesValue(r,s);if(!histFinite(v) || !histFinite(r.time_s)){started=false;return;}var px=x(r.time_s),py=y(v*s.scale,s.axis);if(started)ctx.lineTo(px,py);else ctx.moveTo(px,py);started=true;});
        ctx.stroke();ctx.setLineDash([]);ctx.globalAlpha=1;
      });
    }
    if(other.length)draw(other,true);draw(rows,false);
    var cursor=el('hist_cursor');
    if(histFinite(hmiHistHoverTime)){
      ctx.strokeStyle='rgba(220,239,248,.8)';ctx.lineWidth=1;ctx.setLineDash([4,4]);
      ctx.beginPath();ctx.moveTo(x(hmiHistHoverTime),T);ctx.lineTo(x(hmiHistHoverTime),T+ph);ctx.stroke();ctx.setLineDash([]);
      var row=histNearest(rows,hmiHistHoverTime),compareRow=histNearest(other,hmiHistHoverTime);
      if(cursor && row)cursor.innerHTML='<b>'+histFmt(row.time_s,1)+' s</b> · Áp '+histFmt(row.pressure_filtered_bar,1)+' bar · Target '+histFmt(histModeTarget(row),1)+(row.mode===1?' ml/s':' bar')+' · Flow '+histFmt(row.pump_flow_ml_s,1)+' ml/s · Cân '+histFmt(row.scale_weight_g,1)+' g'+(compareRow?' <span>│ So sánh: áp '+histFmt(compareRow.pressure_filtered_bar,1)+' bar · cân '+histFmt(compareRow.scale_weight_g,1)+' g</span>':'');
    }else if(cursor)cursor.textContent=selected?histPhaseLabel(hmiHistCurrent,selected)+' · '+histFmt(selected.start,1)+'–'+histFmt(selected.end,1)+' s':'Chạm hoặc di chuột trên đồ thị để xem giá trị theo thời gian.';
  }
  window.addEventListener('resize',function(){if(el('hist_chart') && hmiHistCurrent && !hmiHistCurrent.loading)histDrawChart();});
  function loadHmiHistory(force){
    if(hmiHistLoaded && !force){histRenderList();histRenderDetail();return;}
    if(hmiHistListBusy)return;hmiHistListBusy=true;
    var list=el("hist_list"); if(list) list.innerHTML='<div class="hist-empty">Loading saved traces…</div>';
    httpGetJSON("/traces/list",function(data){
      hmiHistListBusy=false;
      hmiHistTraceSupported=data.supported!==false;
      hmiHistLoaded=true; hmiHistTraces=(data.traces||[]).sort(function(a,b){return b.id-a.id;}).slice(0,10);
      if(hmiHistCurrent && !hmiHistTraces.some(t=>t.id===hmiHistCurrent.id)){pauseHmiHistory();hmiHistCurrent=null;}
      if(hmiHistCompareId && !hmiHistTraces.some(t=>t.id===hmiHistCompareId)){hmiHistCompareId=0;hmiHistCompare=null;}
      histRenderList();histRenderDetail();
    },function(){hmiHistListBusy=false;if(list)list.innerHTML='<div class="hist-empty">TF card or trace archive unavailable.</div>';});
  }

)JS";

#endif
