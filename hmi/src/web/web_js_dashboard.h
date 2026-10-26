// File: web_js_dashboard.h
// Logic dashboard: vẽ chart, /dash/live, split IDLE and BREW modes

#ifndef WEB_JS_DASHBOARD_H
#define WEB_JS_DASHBOARD_H

#include <Arduino.h>

static const char WEB_JS_DASH[] PROGMEM = R"JS(
  var brewStreamBoot=null, brewStreamSession=null, brewStreamCursor=0;
  var brewFrame=null, brewFrameAt=0, brewPlayhead=null, brewAnimating=false;
  var brewSummaryPending=false;
  function mergeBrewStream(j){
    if(!Array.isArray(j.samples))return false;
    if(j.streamBoot!==brewStreamBoot || j.streamSession!==brewStreamSession){
      brewStreamBoot=j.streamBoot;brewStreamSession=j.streamSession;brewStreamCursor=0;
      shotSamples=[];brewPlayhead=null;brewSummaryPending=false;
    }
    // A gap means the finite HMI buffer expired. Do not draw across missing data.
    if(j.streamGap){shotSamples=[];brewPlayhead=null;}
    j.samples.forEach(function(r){
      if(!Array.isArray(r)||r.length<8||!Number.isFinite(r[0])||r[0]<=brewStreamCursor)return;
      if(!r.every(Number.isFinite)){brewStreamCursor=r[0];shotSamples=[];brewPlayhead=null;return;}
      var last=shotSamples[shotSamples.length-1];
      if(last && r[1]<last.t)return;
      shotSamples.push({t:r[1],p:r[2],pt:r[3],flow:r[4],wt:r[5],temp:r[6],m:r[7]});
      brewStreamCursor=r[0];
    });
    if(shotSamples.length>2400)shotSamples.splice(0,shotSamples.length-2400);
    return true;
  }
  function brewVisibleSamples(time){
    var visible=[];
    for(var i=0;i<shotSamples.length;i++){
      var s=shotSamples[i];
      if(s.t<=time){visible.push(s);continue;}
      var prev=visible[visible.length-1];
      if(prev && s.t>prev.t){
        var f=(time-prev.t)/(s.t-prev.t),edge={t:time,m:prev.m};
        ['p','flow','wt','temp'].forEach(function(k){edge[k]=prev[k]+(s[k]-prev[k])*f;});
        edge.pt=prev.pt;visible.push(edge);
      }
      break;
    }
    return visible;
  }
  function animateBrew(at){
    brewFrame=null;
    if(!brewAnimating || document.hidden)return;
    if(!brewFrameAt || at-brewFrameAt>=33){
      var dt=brewFrameAt?Math.min(0.1,(at-brewFrameAt)/1000):0;
      brewFrameAt=at;
      if(shotSamples.length){
        var latest=shotSamples[shotSamples.length-1].t;
        if(brewPlayhead===null)brewPlayhead=Math.max(shotSamples[0].t,latest-0.15);
        // Catch up smoothly, but never extrapolate beyond a received measurement.
        brewPlayhead=Math.min(latest,brewPlayhead+dt*Math.min(4,Math.max(1,(latest-brewPlayhead)/0.15)));
        drawChart(brewPlayhead,brewVisibleSamples(brewPlayhead));
      }
    }
    startBrewAnimation();
  }
  function startBrewAnimation(){
    if(brewAnimating && !document.hidden && brewFrame===null && typeof requestAnimationFrame==='function')
      brewFrame=requestAnimationFrame(animateBrew);
  }
  function initChart(){
    if(chartCanvas) return;
    chartCanvas = el("dash_chart");
    chartCtx = chartCanvas.getContext("2d");
  }

  function drawChart(currentTime,displaySamples){
    var chartSamples=displaySamples||shotSamples;
    initChart();
    var ctx = chartCtx;
    var rawW = chartCanvas.clientWidth || chartCanvas.width;
    var rawH = chartCanvas.clientHeight || chartCanvas.height;
    
    // Keep CSS pixels as the drawing coordinate system while sharpening Retina screens.
    var dpr = Math.min(2, window.devicePixelRatio || 1);
    var pixelW=Math.max(1,Math.round(rawW*dpr)),pixelH=Math.max(1,Math.round(rawH*dpr));
    if(chartCanvas.width!==pixelW)chartCanvas.width=pixelW;
    if(chartCanvas.height!==pixelH)chartCanvas.height=pixelH;
    ctx.setTransform(dpr,0,0,dpr,0,0);
    var w = rawW, h = rawH;
    ctx.clearRect(0,0,w,h);

    var targetPts = profilePointsByName[currentProfileName] || [];

    var tMax=0;
    chartSamples.forEach(function(s){
      if(s.t>tMax) tMax=s.t;
    });
    targetPts.forEach(function(p){
      if(p.t>tMax) tMax=p.t;
    });
    if(tMax<1) tMax=1;
    if(tMax<45) tMax=45;

    var pMax = 12;
    var flowMax = 12;  // Flow shares the 0-12 left axis with pressure.
    var rightMax = 8;  // 0–8: Flow ml/s & Weight/10 (0–80g)
    var tempMin = 0, tempMax = 120;

    var padL = (w < 520) ? 34 : 50;
    var padR = (w < 520) ? 18 : 50;
    var padT = 20;
    var padB = 30;
    var plotW = Math.max(40, w - padL - padR);
    var plotH = Math.max(60, h - padT - padB);

    var px = function(t){return padL+(t/tMax)*plotW;};

    function pyLeftUnit(u){
      if(u<0) u=0;
      if(u>pMax) u=pMax;
      return h-padB-(u/pMax)*plotH;
    }
    var pyPress = function(p){ return pyLeftUnit(p); };
    var pyTemp  = function(tp){
      if(tp<tempMin)tp=tempMin;
      if(tp>tempMax)tp=tempMax;
      return pyLeftUnit(tp/10);
    };

    function pyRightUnit(v){
      if(v<0) v=0;
      if(v>rightMax) v=rightMax;
      return h-padB-(v/rightMax)*plotH;
    }
    var pyFlow   = function(f){ if(f<0)f=0; if(f>flowMax)f=flowMax; return pyLeftUnit(f); };
    var pyWeight = function(wt){ if(wt<0)wt=0; if(wt>80)wt=80; return pyRightUnit(wt/10); };

    // Grid
    ctx.strokeStyle = "rgba(42, 45, 53, 0.5)";
    ctx.lineWidth = 1;
    var xStep = w < 520 ? 10 : 5;
    for(var s=0; s<=tMax; s+=xStep){
      var x=px(s);
      ctx.beginPath();ctx.moveTo(x,padT);ctx.lineTo(x,h-padB);ctx.stroke();
    }
    for(var b=0; b<=12; b+=3){
      var y=pyPress(b);
      ctx.beginPath();ctx.moveTo(padL,y);ctx.lineTo(w-padR,y);ctx.stroke();
    }

    // Y labels (Trái)
    ctx.fillStyle="#9ea3af";
    ctx.font="11px system-ui";
    ctx.textAlign="right";
    for(var b=0; b<=12; b+=3){
      var y = pyPress(b)+4;
      ctx.fillText(b,padL-5,y);
    }

    // Y labels (Phải)
    // X labels
    ctx.textAlign="center";
    for(var ss=0; ss<=tMax; ss+=xStep){
      var xx=px(ss);
      ctx.fillText(ss+"s",xx,h-10);
    }

    function drawLine(series,color,dash){
      if(series.length<2) return;
      ctx.save();
      ctx.strokeStyle=color;
      ctx.lineWidth=3;
      if(dash) ctx.setLineDash(dash);
      
      ctx.beginPath();
      ctx.moveTo(series[0].x, series[0].y);
      if(series.length < 3) {
        series.forEach(function(p){ ctx.lineTo(p.x, p.y); });
      } else {
        // Quadratic curve interpolation (smooth preview)
        var i;
        for (i = 1; i < series.length - 2; i++) {
          var xc = (series[i].x + series[i + 1].x) / 2;
          var yc = (series[i].y + series[i + 1].y) / 2;
          ctx.quadraticCurveTo(series[i].x, series[i].y, xc, yc);
        }
        ctx.quadraticCurveTo(series[i].x, series[i].y, series[i+1].x, series[i+1].y);
      }
      ctx.stroke();
      ctx.restore();
    }

    function drawArea(series,color){
      if(series.length<2) return;
      ctx.save();
      ctx.fillStyle = color;
      ctx.globalAlpha = 0.15;
      ctx.beginPath();
      var baseY = pyRightUnit(0);
      ctx.moveTo(series[0].x, baseY);
      
      if(series.length < 3) {
        series.forEach(function(p){ ctx.lineTo(p.x, p.y); });
      } else {
        ctx.lineTo(series[0].x, series[0].y);
        var i;
        for (i = 1; i < series.length - 2; i++) {
          var xc = (series[i].x + series[i + 1].x) / 2;
          var yc = (series[i].y + series[i + 1].y) / 2;
          ctx.quadraticCurveTo(series[i].x, series[i].y, xc, yc);
        }
        ctx.quadraticCurveTo(series[i].x, series[i].y, series[i+1].x, series[i+1].y);
      }
      
      ctx.lineTo(series[series.length-1].x, baseY);
      ctx.closePath();
      ctx.fill();
      ctx.restore();
    }

    var pressSeries=[],flowSeries=[],weightSeries=[],tempSeries=[],targetSeries=[];
    var lp=0, lf=0, lw=0, lt=0;
    var aP=0.6, aF=0.4, aW=0.7, aT=0.5; // High alpha = less lag, matches TFT better
    chartSamples.forEach(function(s, i){
      var x=px(s.t);
      if(i==0){ lp=s.p; lf=s.flow; lw=s.wt/10; lt=s.temp/10; }
      else {
        lp = lp + aP*(s.p - lp);
        lf = lf + aF*(s.flow - lf);
        lw = lw + aW*(s.wt/10 - lw);
        lt = lt + aT*(s.temp/10 - lt);
      }
      pressSeries.push({x:x,y:pyPress(lp)});
      flowSeries.push({x:x,y:pyFlow(lf)});
      weightSeries.push({x:x,y:pyWeight(lw*10)});
      tempSeries.push({x:x,y:pyTemp(lt*10)});

      var ty;
      if(s.m === 1)      ty = pyFlow(s.pt);
      else if(s.m === 2) ty = pyPress(s.pt / 10); // 100% -> 10 bar position on Left Axis
      else               ty = pyPress(s.pt);
      targetSeries.push({x:x, y:ty, m:s.m});
    });
    
    var lastWeightVal = chartSamples.length ? chartSamples[chartSamples.length-1].wt : null;
    var lastTempVal = chartSamples.length ? chartSamples[chartSamples.length-1].temp : null;

    var rootStyle=getComputedStyle(document.documentElement);
    var colPress   = rootStyle.getPropertyValue("--pressure")||"#00e676";
    var colTemp    = rootStyle.getPropertyValue("--temp")||"#ef5350";
    var colFlow    = rootStyle.getPropertyValue("--flow")||"#ffca28";
    var colWeight  = rootStyle.getPropertyValue("--weight")||"#42a5f5";

    // Target Line Segmented
    if(seriesVisible.target && targetSeries.length > 1) {
      ctx.save();
      ctx.lineWidth = 1.5;
      ctx.setLineDash([5, 4]);
      var lastM = -1;
      for(var i=0; i<targetSeries.length; i++){
        var p = targetSeries[i];
        if(p.m !== lastM) {
          if(i > 0) ctx.stroke();
          ctx.beginPath();
          ctx.moveTo(p.x, p.y);
          ctx.strokeStyle = (p.m===1) ? "#ffca28" : (p.m===2 ? "#ffffff" : "#00e5ff");
          lastM = p.m;
        } else {
          ctx.lineTo(p.x, p.y);
        }
      }
      ctx.stroke();
      ctx.restore();
    }

    if(seriesVisible.pressure && pressSeries.length) drawLine(pressSeries,colPress);
    if(seriesVisible.temp && tempSeries.length)      drawLine(tempSeries, colTemp);
    if(seriesVisible.flow && flowSeries.length)      drawLine(flowSeries,colFlow);

    if(seriesVisible.weight && weightSeries.length){
      drawArea(weightSeries, colWeight);
      drawLine(weightSeries,colWeight);
    }

    ctx.save();
    ctx.font = "12px system-ui";
    ctx.textAlign = "left";
    ctx.shadowBlur = 4;
    ctx.shadowColor = "rgba(0,0,0,0.8)";

    if(seriesVisible.temp && tempSeries.length && lastTempVal!=null){
      var tp = tempSeries[tempSeries.length-1];
      ctx.fillStyle = colTemp;
      ctx.fillText(lastTempVal.toFixed(0) + "°C", tp.x+6, tp.y-6);
    }
    if(seriesVisible.weight && weightSeries.length && lastWeightVal!=null){
      var wp = weightSeries[weightSeries.length-1];
      ctx.fillStyle = colWeight;
      ctx.fillText(lastWeightVal.toFixed(0) + "g", wp.x+6, wp.y-6);
    }

    // Active Cursor Line
    if(currentTime !== undefined && currentTime > 0) {
      var x = px(currentTime);
      ctx.save();
      ctx.strokeStyle = "rgba(255,255,255,0.4)";
      ctx.setLineDash([2, 4]);
      ctx.lineWidth = 1;
      ctx.beginPath();ctx.moveTo(x, padT);ctx.lineTo(x, h-padB);ctx.stroke();
      
      // Time label on cursor
      ctx.fillStyle = "rgba(255,255,255,0.8)";
      ctx.font = "10px monospace";
      ctx.textAlign = "center";
      ctx.fillText(currentTime.toFixed(1) + "s", x, 15);
      ctx.restore();
    }
    ctx.restore();
  }

  var dashCommandPending=false;
  var heaterStartBlocked=true, heaterSleeping=false;
  var brewStopLocked=false;
  function setDashCommandPending(pending){
    dashCommandPending=pending;
    ['btn_dash_start','btn_dash_flush'].forEach(function(id){var node=el(id);if(node)node.disabled=pending||(id==='btn_dash_start'&&heaterStartBlocked);});
    ['btn_brew_stop_desktop','btn_brew_stop_mobile'].forEach(function(id){var node=el(id);if(node)node.disabled=pending||brewStopLocked;});
  }
  function sendDashCommand(url,pendingLabel){
    if(dashCommandPending)return;
    setDashCommandPending(true);
    // Release the background read before sending a command. Never retry commands.
    pauseDashLive();
    notifyUi(pendingLabel||'Đang gửi lệnh…');
    fetch(url,{cache:'no-store'}).then(function(r){return r.text().then(function(txt){
      if(!r.ok)throw new Error(txt||('HTTP '+r.status));
      return txt;
    });}).then(function(){
      notifyUi('Main đã nhận yêu cầu',true);
    }).catch(function(err){
      notifyUi(err.message==='REJECTED'?'Main từ chối yêu cầu':'Không gửi được lệnh: '+err.message,false);
    }).finally(function(){setTimeout(function(){
      setDashCommandPending(false);
      scheduleDashLive(0);
    },450);});
  }

  function dashStart() {
    var btn = el("btn_dash_start");
    if(!btn || btn.disabled) return;
    var isRunning = btn.getAttribute("data-running") === "true" || (lastLiveState === "PROFILE_RUN" || lastLiveState === "HOTWATER_RUN");
    sendDashCommand(isRunning ? "/dash/stop" : "/dash/start",isRunning?'Đang yêu cầu dừng shot…':heaterSleeping?'Đang yêu cầu Preheat…':'Đang yêu cầu bắt đầu shot…');
  }

  function dashStop() {
    sendDashCommand('/dash/stop','Đang yêu cầu dừng shot…');
  }

  function dashFlush() {
    var isFlush = (lastLiveState === "FLUSH_RUN");
    sendDashCommand(isFlush ? "/dash/stop" : "/dash/flush",isFlush?'Đang yêu cầu dừng flush…':'Đang yêu cầu flush…');
  }

  function goHome() {
    summaryLatched=false;
    var vIdle = el("dash_view_idle");
    var vBrew = el("dash_view_brew");
    if(vIdle && vBrew) {
      vIdle.classList.add("active");
      vBrew.classList.remove("active");
    }
  }

  var lastLiveState = "IDLE";
  var summaryLatched = false;
  var shotWasActive = false;
  var lastShotSnapshot = null;

  function targetYieldFor(j){
    var target=(typeof j.autoYield==='number')?j.autoYield:0;
    if(target>0)return target;
    for(var i=0;i<dashProfiles.length;i++){
      if(dashProfiles[i].idx===j.profileIdx && dashProfiles[i].tW>0)return dashProfiles[i].tW;
    }
    return 0;
  }

  function captureShotSnapshot(j){
    if(!shotSamples.length)return lastShotSnapshot;
    var last=shotSamples[shotSamples.length-1],peak=0,pressureTarget=0;
    shotSamples.forEach(function(s){peak=Math.max(peak,s.p||0);if(s.m===0)pressureTarget=Math.max(pressureTarget,s.pt||0);});
    lastShotSnapshot={
      time:last.t||0,yield:last.wt||0,profile:j.profileName||currentProfileName||'--',
      targetYield:targetYieldFor(j),peak:peak,pressureTarget:pressureTarget,
      finalPressure:last.p||0,avgFlow:last.t>0?(last.wt||0)/last.t:0
    };
    return lastShotSnapshot;
  }

  function renderShotSummary(s){
    if(!s)return;
    if(el('sum_time'))el('sum_time').textContent=s.time.toFixed(1)+'s';
    if(el('sum_yield'))el('sum_yield').textContent=s.yield.toFixed(1)+'g';
    if(el('sum_profile'))el('sum_profile').textContent=s.profile;
    if(el('sum_peak'))el('sum_peak').textContent=s.peak.toFixed(1)+' bar';
    if(el('sum_avg_flow'))el('sum_avg_flow').textContent=s.avgFlow.toFixed(2)+' g/s';
    var deltaText='--',deltaColor='var(--muted)',lines=[];
    if(s.targetYield>0){
      var dy=s.yield-s.targetYield;
      deltaText=s.targetYield.toFixed(1)+'g / '+(dy>=0?'+':'')+dy.toFixed(1)+'g';
      deltaColor=Math.abs(dy)<=2?'var(--ok)':(dy>0?'#ff8c2a':'var(--danger)');
      lines.push(Math.abs(dy)<=2?'Yield sát mục tiêu ('+(dy>=0?'+':'')+dy.toFixed(1)+'g).':(dy>0?'Ra dư '+dy.toFixed(1)+'g, nên dừng sớm hơn.':'Ra thiếu '+Math.abs(dy).toFixed(1)+'g, có thể cần kéo shot dài hơn.'));
    }else lines.push('Chưa có target yield để so sánh độ lệch.');
    if(el('sum_delta')){el('sum_delta').textContent=deltaText;el('sum_delta').style.color=deltaColor;}
    lines.push(s.avgFlow>=1.8?'Nhịp chiết nhanh, flow trung bình '+s.avgFlow.toFixed(2)+' g/s.':(s.avgFlow<=0.9?'Nhịp chiết chậm, flow trung bình '+s.avgFlow.toFixed(2)+' g/s.':'Nhịp chiết cân bằng, flow trung bình '+s.avgFlow.toFixed(2)+' g/s.'));
    if(s.pressureTarget>0){
      if(s.peak<s.pressureTarget-1.2)lines.push('Áp chưa chạm target: peak '+s.peak.toFixed(1)+'/'+s.pressureTarget.toFixed(1)+' bar.');
      else if(s.peak>s.pressureTarget+1.2)lines.push('Áp vượt target: peak '+s.peak.toFixed(1)+'/'+s.pressureTarget.toFixed(1)+' bar.');
      else lines.push('Áp đạt vùng target, peak '+s.peak.toFixed(1)+' bar.');
      if(s.peak>3 && s.finalPressure<s.peak-2)lines.push('Cuối shot áp tụt rõ, puck có thể đang thoáng dần.');
    }
    if(s.time<20)lines.push('Thời gian shot ngắn, nếu vị chua/mỏng thì xay mịn hơn.');
    else if(s.time>40)lines.push('Thời gian shot dài, nếu đắng/chát thì xay thô hơn.');
    if(el('sum_note'))el('sum_note').innerHTML=lines.map(function(txt){return '<div>'+txt+'</div>';}).join('');
  }

  function showBrewSurface(summary){
    var viewIdle=el('dash_view_idle'),viewBrew=el('dash_view_brew');
    if(viewIdle)viewIdle.classList.remove('active');
    if(viewBrew)viewBrew.classList.add('active');
    if(el('brew_sidebar_live'))el('brew_sidebar_live').style.display=summary?'none':'flex';
    if(el('brew_sidebar_summary'))el('brew_sidebar_summary').style.display=summary?'flex':'none';
    if(el('btn_brew_stop_mobile'))el('btn_brew_stop_mobile').style.display=summary?'none':'';
  }
  function updateConnectionState(id,dotId,textId,online,label){
    var wrap=el(id),dot=el(dotId),txt=el(textId);
    if(wrap){wrap.classList.toggle('ok',!!online);wrap.classList.toggle('bad',!online);}
    if(dot)dot.classList.toggle('ok',!!online);
    if(txt)txt.textContent=label;
  }
  function updateStateFromLive(j){
    var now=Date.now();
    lastLiveTs=now;
    lastLiveState = j.state;

    var isBrewing = !!j.isBrewing;
    var isFlush = (j.state === "FLUSH_RUN");
    var isShotActive = (j.state === "PROFILE_RUN" || j.state === "HOTWATER_RUN");
    var isEnding = !!j.isEnding || j.machineState === "ENDING";
    var isDone = !!j.isDone || j.machineState === "DONE";
    var streamed=mergeBrewStream(j);
    if(!isShotActive && (isDone || (!isEnding && shotWasActive)))brewSummaryPending=true;
    brewAnimating=isShotActive && streamed;
    startBrewAnimation();

    if(isShotActive){
      summaryLatched=false;
      brewSummaryPending=false;
    }else if(brewSummaryPending && !j.streamMore && shotSamples.length){
      brewSummaryPending=false;
      summaryLatched=true;
      var captured=captureShotSnapshot(j);
      if(captured){
        j.lastT=captured.time;
        j.lastW=captured.yield;
        j.lastP=captured.profile;
        renderShotSummary(captured);
      }
    }else if(summaryLatched && lastShotSnapshot){
      j.lastT=lastShotSnapshot.time;
      j.lastW=lastShotSnapshot.yield;
      j.lastP=lastShotSnapshot.profile;
    }

    var isIdleMode = summaryLatched;
    var shouldManageBrew = isShotActive || isEnding || summaryLatched;
    var viewIdle = shouldManageBrew ? el("dash_view_idle") : null;
    var viewBrew = shouldManageBrew ? el("dash_view_brew") : null;
    if(viewIdle && viewBrew) {
      if(isIdleMode) {
        if(!viewIdle.classList.contains("active")) {
          // Bệ hạ muốn: "khi dừng shot thì chuyển sang màn hình tổng kết, đồ thị vẫn giữ nguyên bên phải, nhưng bên trái thì có những tổng kết quan trọng"
          // => Không active viewIdle, giữ nguyên viewBrew nhưng đổi nội dung sidebar trái!
          viewBrew.classList.add("active");
          viewIdle.classList.remove("active");
          
          if(el("brew_sidebar_live")) el("brew_sidebar_live").style.display = "none";
          if(el("brew_sidebar_summary")) el("brew_sidebar_summary").style.display = "flex";
          if(el("btn_brew_stop_mobile")) el("btn_brew_stop_mobile").style.display = "none";
          
          // Điền dữ liệu summary
          if(j.lastT > 0 && typeof j.lastW === "number") {
             var shotTime = j.lastT;
             var shotYield = j.lastW;
             var profileLabel = j.lastP || j.profileName || currentProfileName || "--";
             var targetYield = (typeof j.autoYield === "number") ? j.autoYield : 0;
             if((!targetYield || targetYield <= 0) && typeof j.profileIdx === "number") {
               for(var pfIdx=0; pfIdx<dashProfiles.length; pfIdx++) {
                 if(dashProfiles[pfIdx].idx === j.profileIdx && dashProfiles[pfIdx].tW > 0) {
                   targetYield = dashProfiles[pfIdx].tW;
                   break;
                 }
               }
             }
             if(el("sum_time")) el("sum_time").textContent = j.lastT.toFixed(1) + "s";
             if(el("sum_yield")) el("sum_yield").textContent = j.lastW.toFixed(1) + "g";
             if(el("sum_profile")) el("sum_profile").textContent = profileLabel;
             // Tính Peak Pressure từ mảng data
             var peak = 0;
             var pressureTarget = 0;
             var finalPressure = 0;
             for(var i=0; i<shotSamples.length; i++) {
                 if(shotSamples[i].p > peak) peak = shotSamples[i].p;
                 if(shotSamples[i].m === 0 && shotSamples[i].pt > pressureTarget) pressureTarget = shotSamples[i].pt;
                 finalPressure = shotSamples[i].p;
             }
             if(el("sum_peak")) el("sum_peak").textContent = peak.toFixed(1) + " bar";
             var avgFlow = shotTime > 0 ? shotYield / shotTime : 0;
             if(el("sum_avg_flow")) el("sum_avg_flow").textContent = avgFlow.toFixed(2) + " g/s";

             var deltaText = "--";
             var deltaColor = "var(--muted)";
             if(targetYield > 0) {
               var delta = shotYield - targetYield;
               deltaText = targetYield.toFixed(1) + "g / " + (delta >= 0 ? "+" : "") + delta.toFixed(1) + "g";
               deltaColor = Math.abs(delta) <= 2.0 ? "var(--ok)" : (delta > 0 ? "#ff8c2a" : "var(--danger)");
             }
             if(el("sum_delta")) {
               el("sum_delta").textContent = deltaText;
               el("sum_delta").style.color = deltaColor;
             }

             var lines = [];
             if(targetYield > 0) {
               var dy = shotYield - targetYield;
               if(Math.abs(dy) <= 2.0) lines.push("Yield sát mục tiêu (" + (dy >= 0 ? "+" : "") + dy.toFixed(1) + "g).");
               else if(dy > 2.0) lines.push("Ra dư " + dy.toFixed(1) + "g, nên dừng sớm hơn hoặc giảm yield mục tiêu.");
               else lines.push("Ra thiếu " + Math.abs(dy).toFixed(1) + "g, có thể cần kéo shot dài hơn.");
             } else {
               lines.push("Chưa có target yield để so sánh độ lệch.");
             }

             if(avgFlow >= 1.8) lines.push("Nhịp chiết nhanh, flow trung bình " + avgFlow.toFixed(2) + " g/s.");
             else if(avgFlow <= 0.9) lines.push("Nhịp chiết chậm, flow trung bình " + avgFlow.toFixed(2) + " g/s.");
             else lines.push("Nhịp chiết cân bằng, flow trung bình " + avgFlow.toFixed(2) + " g/s.");

             if(pressureTarget > 0) {
               if(peak < pressureTarget - 1.2) lines.push("Áp chưa chạm target: peak " + peak.toFixed(1) + "/" + pressureTarget.toFixed(1) + " bar.");
               else if(peak > pressureTarget + 1.2) lines.push("Áp vượt target: peak " + peak.toFixed(1) + "/" + pressureTarget.toFixed(1) + " bar.");
               else lines.push("Áp đạt vùng target, peak " + peak.toFixed(1) + " bar.");
               if(peak > 3.0 && finalPressure < peak - 2.0) lines.push("Cuối shot áp tụt rõ, puck có thể đang thoáng dần.");
             } else if(peak > 0) {
               lines.push("Peak pressure " + peak.toFixed(1) + " bar.");
             }

             if(shotTime < 20) lines.push("Thời gian shot ngắn, nếu vị chua/mỏng thì xay mịn hơn.");
             else if(shotTime > 40) lines.push("Thời gian shot dài, nếu đắng/chát thì xay thô hơn.");

             if(el("sum_note")) el("sum_note").innerHTML = lines.map(function(txt){ return "<div>" + txt + "</div>"; }).join("");
          }
        }
      } else {
        if(!viewBrew.classList.contains("active") || (el("brew_sidebar_live") && el("brew_sidebar_live").style.display === "none")) {
          viewIdle.classList.remove("active");
          viewBrew.classList.add("active");
          if(el("brew_sidebar_live")) el("brew_sidebar_live").style.display = "flex";
          if(el("brew_sidebar_summary")) el("brew_sidebar_summary").style.display = "none";
          if(el("btn_brew_stop_mobile")) el("btn_brew_stop_mobile").style.display = "";
          setTimeout(function() { if(!brewAnimating)drawChart(j.t); }, 100);
        }
      }
    }

    var stateStrip=el('brew_state_strip');
    var telemetryFresh=!!j.ok && (typeof j.frameAgeMs!=='number' || j.frameAgeMs<1500);
    if(stateStrip){
      stateStrip.classList.toggle('ending',isEnding);
      stateStrip.classList.toggle('offline',!telemetryFresh);
    }
    if(el('brew_state_label'))el('brew_state_label').textContent=!telemetryFresh?'MAIN OFFLINE':(isEnding?'STOPPING':'LIVE BREW');
    if(el('brew_link_age'))el('brew_link_age').textContent=typeof j.frameAgeMs==='number'?(j.frameAgeMs<1000?'Telemetry now':(j.frameAgeMs/1000).toFixed(1)+'s old'):'Main telemetry';
    brewStopLocked=isEnding||!telemetryFresh;
    ['btn_brew_stop_desktop','btn_brew_stop_mobile'].forEach(function(id){var node=el(id);if(node)node.disabled=dashCommandPending||brewStopLocked;});

    var startBtn = el("btn_dash_start");
    if(startBtn) {
      startBtn.setAttribute("data-running", isBrewing);
      heaterSleeping=j.heaterFresh && j.heaterMode===0;
      heaterStartBlocked=!isBrewing && (!j.heaterFresh || (!heaterSleeping && !j.heaterReady));
      startBtn.disabled=dashCommandPending || isFlush || heaterStartBlocked;
      startBtn.style.opacity = (isFlush) ? "0.6" : "1.0";
    }

    var flushBtn = el("btn_dash_flush");
    if(flushBtn) {
      flushBtn.style.background = isFlush ? "var(--danger)" : "var(--bg-card)";
      flushBtn.style.color = isFlush ? "#fff" : "var(--text)";
      flushBtn.style.borderColor = isFlush ? "transparent" : "var(--border)";
      var flushTxt = el("btn_dash_flush_text");
      if(flushTxt) flushTxt.textContent = isFlush ? "STOP FLUSH" : "FLUSH";
      flushBtn.style.animation = isFlush ? "pulse 1s infinite" : "none";
    }

    var blockTemp = (typeof j.blockTempC==="number") ? j.blockTempC : ((typeof j.boilerTemp==="number") ? j.boilerTemp : NaN);
    var brewTemp = (typeof j.brewTempEstC==="number") ? j.brewTempEstC : blockTemp;
    var temp = brewTemp;
    var tempEl = el("boiler_temp");
    var stp = temp >= 92 && temp <= 96.5 ? "Ready" : (temp > 96.5 ? "Hot" : "Heating");
    var tempColor = "#ff7a2f";
    var gaugeNumberColor = "#ff6d3a";
    var arcColor = "url(#temp_grad)";
    var tempHint = "Đang chờ dữ liệu";
    if(isFinite(temp)){
      var overheat = (isFinite(blockTemp) && blockTemp > 125) || temp > 102;
      tempColor = overheat ? "#ff5252" : (temp >= 92 && temp <= 96.5 ? "#66bb6a" : (temp > 96.5 ? "#ffb74d" : "#ff7a2f"));
      arcColor = overheat ? "#ff5252" : (temp >= 92 && temp <= 96.5 ? "#66bb6a" : "url(#temp_grad)");
      tempHint = overheat ? "Cảnh báo quá nhiệt" : (temp >= 92 && temp <= 96.5 ? "Brew estimate sẵn sàng" : (temp > 96.5 ? "Brew estimate đang nóng" : "Brew estimate đang làm nóng"));
      if(isFinite(blockTemp)) tempHint += " • Block " + blockTemp.toFixed(0) + "°C";
      if(tempEl) tempEl.innerHTML = temp.toFixed(1) + '<span class="big-temp-unit">°C</span>';
      if(tempEl) tempEl.style.color = gaugeNumberColor;
      if(el("idle_block_temp")) el("idle_block_temp").textContent = isFinite(blockTemp) ? blockTemp.toFixed(0) + "°C" : "--°C";
      if(el("temp_progress_current")) el("temp_progress_current").textContent = temp.toFixed(1) + "°C";
      if(el("temp_progress_fill")) {
        el("temp_progress_fill").style.width = Math.min(100,Math.max(0,temp / 93 * 100)).toFixed(1) + "%";
        el("temp_progress_fill").style.background = tempColor;
      }
      var ready = temp >= 92 && temp <= 96.5 && !overheat;
      if(el("boiler_label")) el("boiler_label").textContent = ready ? "Brew estimate sẵn sàng" : tempHint;
      
      var pct = Math.min(1, Math.max(0, (temp - 80) / 25));
      var fill = el("temp_arc_fill");
      if(fill) {
        fill.style.strokeDashoffset = (100 - pct * 100).toString();
        fill.style.stroke = arcColor;
      }
      
      var wrap = el("idle_main_display_wrapper");
      var overAlert = el("overheat_alert");
      if (overheat) {
         if(wrap) wrap.classList.add("overheat-warn");
         if(overAlert) overAlert.style.display = "block";
      } else {
         if(wrap) wrap.classList.remove("overheat-warn");
         if(overAlert) overAlert.style.display = "none";
      }
    } else {
      if(tempEl) tempEl.innerHTML = '--<span class="big-temp-unit">°C</span>';
      if(tempEl) tempEl.style.color = gaugeNumberColor;
      if(el("idle_block_temp")) el("idle_block_temp").textContent = "--°C";
      if(el("temp_progress_current")) el("temp_progress_current").textContent = "--°C";
      if(el("temp_progress_fill")) el("temp_progress_fill").style.width = "0";
      if(el("boiler_label")) el("boiler_label").textContent = "Đang chờ dữ liệu nhiệt độ...";
      stp = "No sensor";
    }
    if(startBtn) {
      var startTxt = el("btn_dash_start_text");
      var heating = !isBrewing && j.heaterFresh && !heaterSleeping && !j.heaterReady;
      var startLabel = isBrewing ? "STOP SHOT" : !j.heaterFresh ? "HEATER OFFLINE" : heaterSleeping ? "PREHEAT" : j.heaterReady ? "START BREW" : "HEATING";
      if(heating && isFinite(temp)) startLabel = "HEATING · " + temp.toFixed(1) + " / 93°C";
      if(startTxt) startTxt.textContent = startLabel;
      startBtn.style.background = isBrewing ? "var(--danger)" : j.heaterReady ? "var(--ok)" : (!j.heaterFresh ? "#3b4149" : "#ffbd24");
      startBtn.style.color = (!isBrewing && j.heaterFresh && !j.heaterReady) ? "#101216" : "#fff";
      startBtn.classList.toggle("btn-pulse",!isBrewing && !!j.heaterReady);
    }
    var bsp = el("boiler_state_pill");
    if(bsp) {
      bsp.textContent = stp;
      bsp.className = "pill " + (stp==="Ready"?"ok":(stp==="No sensor"?"bad":""));
      bsp.style.color = tempColor;
      bsp.style.borderColor = tempColor;
    }
    if(el("idle_boiler_state")) {
      el("idle_boiler_state").textContent = stp;
      el("idle_boiler_state").style.color = tempColor;
    }
    if(el("idle_boiler_hint")) el("idle_boiler_hint").textContent = tempHint;
    
    // Update Uptime
    var upEl = el("dash_uptime");
    if(upEl && typeof j.upS === "number") {
      var h = Math.floor(j.upS / 3600);
      var m = Math.floor((j.upS % 3600) / 60);
      var s = j.upS % 60;
      upEl.textContent = "UP: " + [h,m,s].map(v => v < 10 ? "0"+v : v).join(":");
    }

    var p = (typeof j.pressure==="number") ? j.pressure : 0;
    if(el("brew_pressure")) el("brew_pressure").textContent = p.toFixed(1);
    if(el("idle_pressure")) el("idle_pressure").textContent = p.toFixed(1);

    var hasScale = !!j.hasScale;
    updateConnectionState('main_connection','main_dot','main_text',!!j.ok,j.ok?'Connected':'Offline');
    updateConnectionState('scale_connection','scale_dot','scale_text',hasScale,hasScale?'Connected':'Not connected');
    if (window.sysHasScale !== hasScale) {
        window.sysHasScale = hasScale;
        if (typeof renderSteps === 'function') renderSteps();
    }
    
    var wt = (typeof j.weight==="number") ? j.weight : NaN;
    if(el("brew_weight")) el("brew_weight").textContent = isFinite(wt)?wt.toFixed(1):"0.0";
    if(el("idle_yield_w")) el("idle_yield_w").textContent = isFinite(wt)?wt.toFixed(1):"--";
    if(el("idle_scale_weight")) el("idle_scale_weight").textContent = hasScale && isFinite(wt)?wt.toFixed(1):"--";

    var chkYield = el("chk_auto_yield");
    var inYield = el("in_yield_target");

    if(hasScale){
      if(el("scale_status_text")) el("scale_status_text").textContent = "Scale: Connected";
      if(el("scale_status")) el("scale_status").className = "scale-connected";
      if(el("idle_scale_state")) { el("idle_scale_state").textContent = "Connected"; el("idle_scale_state").style.color = "var(--ok)"; }
      if(el("idle_scale_hint")) el("idle_scale_hint").textContent = "Connected";
      if(el("auto_yield_hint")) el("auto_yield_hint").textContent = "Bơm tự dừng trước mục tiêu -2g";
      if(el("auto_yield_side_hint")) el("auto_yield_side_hint").innerHTML = "Bật cân để dừng<br>theo target";
      if(el("auto_yield_lock")) { el("auto_yield_lock").textContent = "✓"; el("auto_yield_lock").style.color = "var(--ok)"; }
      if(chkYield) chkYield.disabled = false;
      if(inYield && !chkYield.checked) inYield.disabled = false;
    }else{
      if(el("scale_status_text")) el("scale_status_text").textContent = "Scale: Not Connected";
      if(el("scale_status")) el("scale_status").className = "scale-simulated";
      if(el("idle_scale_state")) { el("idle_scale_state").textContent = "Not connected"; el("idle_scale_state").style.color = "#9ca3af"; }
      if(el("idle_scale_hint")) el("idle_scale_hint").textContent = "Not connected";
      if(el("auto_yield_hint")) el("auto_yield_hint").textContent = "Cần kết nối cân để bật Auto Yield";
      if(el("auto_yield_side_hint")) el("auto_yield_side_hint").innerHTML = "Cần kết nối cân<br>để bật Auto Yield";
      if(el("auto_yield_lock")) { el("auto_yield_lock").textContent = "▣"; el("auto_yield_lock").style.color = "#69707d"; }
      if(chkYield) { chkYield.checked = false; chkYield.disabled = true; }
      if(inYield) inYield.disabled = true;
    }

    if(chkYield && !chkYield.matches(":active") && inYield && !inYield.matches(":focus")) {
      var ay = (typeof j.autoYield === "number") ? j.autoYield : 0;
      if(ay > 0 && String(ay) !== String(inYield.value)) {
        inYield.value = ay;
      }
      chkYield.checked = (ay > 0);
      inYield.disabled = (!chkYield.checked || !hasScale);
    }

    var readonlyYield = (typeof j.autoYield === "number") ? j.autoYield : 0;
    if(el("auto_yield_value")) {
      el("auto_yield_value").innerHTML = readonlyYield > 0 ? readonlyYield.toFixed(0) + "<small>g</small>" : "--<small>g</small>";
    }
    if(hasScale && readonlyYield > 0) {
      if(el("auto_yield_hint")) el("auto_yield_hint").textContent = "Dừng shot theo target yield hiện tại";
      if(el("auto_yield_side_hint")) el("auto_yield_side_hint").innerHTML = "Readonly trên Dashboard<br>chỉnh trong profile";
      if(el("auto_yield_lock")) { el("auto_yield_lock").textContent = "✓"; el("auto_yield_lock").style.color = "var(--ok)"; }
    } else if(hasScale) {
      if(el("auto_yield_hint")) el("auto_yield_hint").textContent = "Profile hiện tại không đặt target yield";
      if(el("auto_yield_side_hint")) el("auto_yield_side_hint").innerHTML = "Máy sẽ chạy theo<br>logic profile";
      if(el("auto_yield_lock")) { el("auto_yield_lock").textContent = "○"; el("auto_yield_lock").style.color = "#69707d"; }
    } else {
      if(el("auto_yield_hint")) el("auto_yield_hint").textContent = "Cần kết nối cân để dừng theo yield";
      if(el("auto_yield_side_hint")) el("auto_yield_side_hint").innerHTML = "Readonly trên Dashboard<br>chỉnh trong profile";
      if(el("auto_yield_lock")) { el("auto_yield_lock").textContent = "▣"; el("auto_yield_lock").style.color = "#69707d"; }
    }

    // Update Last Shot Result
    var lastShotBox = el("last_shot_box");
    var lastResult=(typeof j.lastW === 'number' && j.lastT>0)?{yield:j.lastW,time:j.lastT,profile:j.lastP||j.profileName}:lastShotSnapshot;
    if(lastShotBox && lastResult && lastResult.time > 0) {
      lastShotBox.style.display = "grid";
      if(el("last_shot_yield_val")) el("last_shot_yield_val").textContent = lastResult.yield.toFixed(1) + " g";
      if(el("last_shot_time_val")) el("last_shot_time_val").textContent = lastResult.time.toFixed(1) + " s";
      if(el("last_shot_profile_val")) el("last_shot_profile_val").textContent = lastResult.profile || "N/A";
      var note = "Sweet spot";
      if(lastResult.time < 20) note = "Shot hơi nhanh - nên xay mịn hơn";
      else if(lastResult.time > 30) note = "Shot hơi chậm";
      if(el("last_shot_note")) el("last_shot_note").textContent = note;
    } else if(lastShotBox) {
      lastShotBox.style.display = "none";
    }

    var pname = j.profileName || "—";
    currentProfileName = pname;
    var chartNameEl = el("chart_profile_name");
    if(chartNameEl) chartNameEl.textContent = pname;
    if(el("btn_dash_profile_sub")) el("btn_dash_profile_sub").textContent = "Selected: " + pname;
    if(el("dash_selected_profile_badge")) el("dash_selected_profile_badge").textContent = pname;

    var t = (typeof j.t==="number") ? j.t : 0;
    var pt = (typeof j.target==="number") ? j.target : 0;
    var flow = (typeof j.flow==="number") ? j.flow : 0;
    var tempShot = isFinite(brewTemp) ? brewTemp : (typeof j.temp==="number" ? j.temp : 0);
    var weightShot = isFinite(wt) ? wt : 0;
    var liveTargetYield = (typeof j.autoYield === "number") ? j.autoYield : 0;
    if((!liveTargetYield || liveTargetYield <= 0) && typeof j.profileIdx === "number") {
      for(var gy=0; gy<dashProfiles.length; gy++) {
        if(dashProfiles[gy].idx === j.profileIdx && dashProfiles[gy].tW > 0) {
          liveTargetYield = dashProfiles[gy].tW;
          break;
        }
      }
    }

    if(el("brew_profile_name")) el("brew_profile_name").textContent = pname;

    var modeStrLive = j.sMode == 1 ? "Flow" : (j.sMode == 2 ? "Duty" : "Pressure");
    var unitStrLive = j.sMode == 1 ? "ml/s" : (j.sMode == 2 ? "%" : "bar");
    var targetColorLive = j.sMode == 1 ? "var(--flow)" : (j.sMode == 2 ? "var(--text)" : "var(--pressure)");
    if(el("brew_target_value")) {
      el("brew_target_value").textContent = pt.toFixed(j.sMode==2 ? 0 : 1) + " " + unitStrLive;
      el("brew_target_value").style.color = targetColorLive;
    }
    if(el("brew_target_mode")) el("brew_target_mode").textContent = "Mode: " + modeStrLive;
    if(el("brew_pump_power")) {
      var pumpPct = (typeof j.pumpPower === "number") ? Math.max(0, Math.min(100, j.pumpPower)) : 0;
      el("brew_pump_power").textContent = pumpPct.toFixed(0) + "%";
    }
    if(isShotActive && !streamed){
      if(!lastWasBrewing) shotSamples=[];
      shotSamples.push({t:t,p:p,pt:pt,flow:flow,wt:weightShot,temp:tempShot,m:j.sMode});
      if(shotSamples.length>600) shotSamples.shift();
    } else if(isFlush && !lastWasBrewing) {
        shotSamples = [];
    }
    
    if(!brewAnimating && (!isIdleMode || shotSamples.length)) {
      drawChart(isIdleMode?shotSamples[shotSamples.length-1].t:j.t);
    }
    lastWasBrewing = isBrewing;
    shotWasActive = isShotActive || isEnding;

    // Timer MM:SS format
    if(el("brew_time")) {
      var sec = Math.floor(t % 60);
      var min = Math.floor(t / 60);
      el("brew_time").textContent = (min < 10 ? "0"+min : min) + ":" + (sec < 10 ? "0"+sec : sec);
    }

    if(el("brew_flow")) el("brew_flow").textContent = flow.toFixed(1);
    if(el("brew_weight")) {
      el("brew_weight").textContent = liveTargetYield > 0
        ? (weightShot.toFixed(1) + " / " + liveTargetYield.toFixed(0))
        : weightShot.toFixed(1);
    }

    // Update Step Monitor
    if(j.curStep > 0) {
      if(el("step_name_display")) el("step_name_display").textContent = "Step " + j.curStep + " / " + j.totStep + " · " + (j.sName || "Running");
      if(el("step_time_progress")) {
        var sNow = (j.sTime/1000).toFixed(1);
        var sMax = (j.sMax/1000).toFixed(1);
        el("step_time_progress").textContent = j.sMax > 0 ? (sNow + "s / " + sMax + "s") : (sNow + "s elapsed");
      }
      if(el("step_progress_bar")) {
        var pct = (j.sMax > 0) ? Math.min(100, (j.sTime / j.sMax) * 100) : 0;
        el("step_progress_bar").style.width = pct + "%";
        el("step_progress_bar").style.opacity = j.sMax > 0 ? "1" : ".35";
        if(el("step_percent")) el("step_percent").textContent = j.sMax > 0 ? (Math.floor(pct) + "%") : "LIVE";
      }
    }

    var st = (j.state || "").toUpperCase();

    // Update Status Banner
    var banner = el("dash_status_banner");
    if(banner) {
      if(summaryLatched) {
        banner.innerHTML = "SHOT COMPLETE <span style='color:#555;'>|</span> Review the extraction summary";
      } else if(isEnding) {
        banner.innerHTML = "STOPPING… <span style='color:#555;'>|</span> Waiting for pressure release";
      } else if(isBrewing && j.curStep > 0) {
        var modeStr = j.sMode == 1 ? "Flow" : (j.sMode == 2 ? "Duty" : "Pressure");
        var unitStr = j.sMode == 1 ? "ml/s" : (j.sMode == 2 ? "%" : "bar");
        var color = j.sMode == 1 ? "#ffca28" : (j.sMode == 2 ? "#ffffff" : "#00e5ff");
        var stepTxt = "Step " + j.curStep + "/" + j.totStep;
        if(j.sName) stepTxt += ": " + j.sName;
        banner.innerHTML = stepTxt + " <span style='color:#555;'>|</span> " + modeStr + " Target: <span style='color:"+color+"'>" + pt.toFixed(j.sMode==2?0:1) + " " + unitStr + "</span>";
      } else if (isFlush) {
        banner.innerHTML = "FLUSHING WATER... <span style='color:#555;'>|</span> <span style='color:var(--ok);'>Warming up</span>";
      } else {
        banner.innerHTML = "PREPARING...";
      }
    }

    if(typeof j.profileIdx === "number" && j.profileIdx !== dashSelectedIdx) {
      dashSelectedIdx = j.profileIdx;
      renderDashProfiles();
    }
    if(typeof j.manualDuty === "number") {
      dashManualDuty = j.manualDuty;
      var sl = el("manual_slider");
      if(sl && !sl.matches(":active")) {
        sl.value = j.manualDuty;
        if(el("manual_val_display")) el("manual_val_display").textContent = j.manualDuty + "%";
      }
    }

    var over = el("manual_overlay");
    if(over) over.style.display = "none"; // Manual Web API is unavailable on HMI.
  }

  var dashProfiles = [];
  var dashSelectedIdx = -1; // -1 means none selected yet
  var dashManualDuty = 50;

  var dashProfilesBusy=false,dashProfilesRetry=null;
  function fetchDashProfiles() {
    if(dashProfilesBusy)return;clearTimeout(dashProfilesRetry);dashProfilesBusy=true;
    httpGetJSON('/dash/profiles',function(j){
      dashProfilesBusy=false;
      if(j && j.ok){dashProfiles=j.profiles||[];renderDashProfiles();}
      if(!j || !j.ok || !dashProfiles.length)dashProfilesRetry=setTimeout(fetchDashProfiles,1500);
    },function(){dashProfilesBusy=false;dashProfilesRetry=setTimeout(fetchDashProfiles,3000);});
  }

  function renderDashProfiles() {
    var pList = el("dash_profile_list");
    if(!pList) return;
    pList.innerHTML = "";
    dashProfiles.forEach(function(pf){
      var div = document.createElement("div");
      var isSel = (pf.idx === dashSelectedIdx);
      var isManual = (pf.name === "MANUAL");
      var isOff = (pf.idx < 0 || pf.name === "OFF");
      div.className = "dash-profile-card" + ((isOff || isManual) ? " utility" : "") + (isSel ? " selected" : "");

      var safeName=escHtml(pf.name||'Unnamed');
      var inner = "<div class='profile-row'>";
      inner += "<div class='profile-name'>" + safeName + "</div>";
      if(isOff) inner += "<div class='special-icon'>⏻</div>";
      else if(isManual) inner += "<div class='manual-icon'>☝</div>";
      else if(pf.steps>0) inner += "<div class='profile-chip'>"+pf.steps+" steps</div>";
      inner += "</div>";

      if(isOff) inner += "<div class='profile-sub'>Machine off</div>";
      if(isManual) {
        inner += "<div class='profile-sub'>Manual control</div>";
        inner += "<div style='display:flex; justify-content:flex-end; margin-top:-12px;'><div class='manual-bars'>";
        for(var mb=0; mb<6; mb++) inner += "<span></span>";
        inner += "</div></div>";
      }
      
      if(pf.idx >= 0 && !isManual){
        inner += "<div class='profile-meta'>";
        if(pf.tW > 0) inner += "<span>Yield: <b>" + pf.tW + "g</b></span>";
        if(pf.tT > 0) inner += "<span>Time: <b>" + pf.tT + "s</b></span>";
        inner += "</div>";
      }

      if(!isManual && !isOff && pf.steps > 0) {
        inner += "<div class='profile-preview'>";
        var bars = Math.max(3, Math.min(5, pf.steps));
        for(var b=0; b<bars; b++) inner += "<span></span>";
        inner += "</div>";
        var phase = (pf.steps >= 5) ? "Pre → Rise → Extract → Finish → Stop" :
                    (pf.steps === 4 ? "Pre → Bloom → Rise → Extract" : "Pre → Bloom → Extract");
        inner += "<div class='profile-phases'>" + phase + "</div>";
      }

      if(isSel) {
         if(pf.steps > 0 || pf.name === "FULL OUT") {
           var tp = pf.type === 1 ? "BASIC" : "ADV";
           inner += "<div class='profile-mode'>Mode: " + tp + " / AUTO</div>";
         }
      }

      div.innerHTML = inner;
      div.setAttribute('role','button');
      div.setAttribute('tabindex','0');
      div.setAttribute('aria-pressed',isSel?'true':'false');
      div.onclick = function(e){
        if(e.target.tagName==='INPUT') return;
        if(!isSel) {
          notifyUi('Đang chọn '+(pf.name||'profile')+'…');
          fetch("/dash/set_profile?idx="+encodeURIComponent(pf.idx),{cache:'no-store'}).then(function(r){
            return r.text().then(function(txt){if(!r.ok)throw new Error(txt||('HTTP '+r.status));});
          }).then(function(){
            dashSelectedIdx=pf.idx;renderDashProfiles();notifyUi('Đã gửi yêu cầu chọn profile',true);
          }).catch(function(err){notifyUi(err.message==='REJECTED'?'Main từ chối đổi profile':'Không đổi được profile',false);});
        }
      };
      div.onkeydown=function(e){if(e.key==='Enter'||e.key===' '){e.preventDefault();div.click();}};

      pList.appendChild(div);
      

      if (isSel) {
         setTimeout(function(){ 
           if(pList.scrollTop > div.offsetTop || (pList.scrollTop + pList.clientHeight) < (div.offsetTop+div.clientHeight)) {
               div.scrollIntoView({behavior: "smooth", block:"nearest"});
           }
         }, 100);
      }
    });
  }

  var dashLiveStarted=false, dashLiveRequest=null;
  function scheduleDashLive(delay){
    if(!dashLiveStarted)return;
    if(liveTimer!==null)clearTimeout(liveTimer);
    liveTimer=null;
    if(dashCommandPending || dashLiveRequest)return;
    liveTimer=setTimeout(pollLive,document.hidden?5000:delay);
  }
  function pauseDashLive(){
    if(!dashLiveStarted)return;
    if(liveTimer!==null)clearTimeout(liveTimer);
    liveTimer=null;
    if(dashLiveRequest){
      dashLiveRequest.cancelled=true;
      dashLiveRequest.controller.abort();
    }
  }
  function pollLive(){
    if(liveTimer!==null)clearTimeout(liveTimer);
    liveTimer=null;
    if(dashCommandPending || dashLiveRequest)return;
    var request={controller:new AbortController(),cancelled:false,started:Date.now(),fast:false};
    dashLiveRequest=request;
    // Bound the entire read, including the response body, without overlapping reads.
    var deadline=setTimeout(function(){request.controller.abort();},8000);
    var url='/dash/live';
    if(brewStreamBoot!==null)url+='?boot='+brewStreamBoot+'&session='+brewStreamSession+'&after='+brewStreamCursor;
    return fetch(url,{cache:"no-store",signal:request.controller.signal})
      .then(function(r){if(!r.ok)throw new Error('HTTP '+r.status);return r.json();})
      .then(function(j){
        if(request.cancelled)return;
        liveFailed=0;
        updateWifiStatus(true);
        if(!j) return;
        request.fast=!!j.streamMore || j.state==='PROFILE_RUN' || j.state==='HOTWATER_RUN';
        updateStateFromLive(j);
      })
      .catch(function(_){
        if(request.cancelled)return;
        liveFailed++;
        if(liveFailed>=3){
          updateWifiStatus(false);
          updateConnectionState('main_connection','main_dot','main_text',false,'Unavailable');
          updateConnectionState('scale_connection','scale_dot','scale_text',false,'Unknown');
        }
      }).finally(function(){
        clearTimeout(deadline);
        dashLiveRequest=null;
        scheduleDashLive(request.fast && !liveFailed?Math.max(20,100-(Date.now()-request.started)):250);
      });
  }

  function startLive(){
    // Both the system and profile modules call this during page startup.
    if(dashLiveStarted)return;
    dashLiveStarted=true;
    window.scrollTo(0,0);
    fetchDashProfiles();
    if(document.hidden)scheduleDashLive(5000);
    else pollLive();
  }

  document.addEventListener('visibilitychange',function(){brewFrameAt=0;startBrewAnimation();scheduleDashLive(0);});

  document.addEventListener("click", function(e){
    var li = e.target.closest(".legend-item[data-series]");
    if(!li) return;
    var key = li.getAttribute("data-series");
    if(!seriesVisible.hasOwnProperty(key)) return;
    seriesVisible[key] = !seriesVisible[key];
    li.classList.toggle("off", !seriesVisible[key]);
    drawChart();
  });

  // Manual Web control is intentionally not wired: the HMI endpoint returns
  // 501 until the Main-side command contract is migrated. Keeping the old
  // document-level touch drag handlers here would block native phone scroll.

)JS";

#endif // WEB_JS_DASHBOARD_H
