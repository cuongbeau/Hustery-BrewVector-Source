// Profile target preview. Pressure and flow share time; measured values are never inferred.
function profileEase(t,type){t=Math.max(0,Math.min(1,t));return type==='ease-in'?t*t:type==='ease-out'?1-(1-t)*(1-t):type==='ease-in-out'?(t<.5?2*t*t:1-2*(1-t)*(1-t)):t;}
function profileTargetPoints(start,duration,from,to,transition){
 const tr=transition||{},ramp=tr.type&&tr.type!=='instant';
 if(!ramp){if(from===to)return [[start,to],[start+duration,to]];const step=Math.min(.06,duration/20);return [[start,from],[start+step,to],[start+duration,to]];}
 const fullRamp=Number(tr.duration)>0?Number(tr.duration):duration,rampTime=Math.min(duration,fullRamp),points=[],count=24;
 for(let i=0;i<=count;i++){const elapsed=rampTime*i/count;points.push([start+elapsed,from+(to-from)*profileEase(elapsed/fullRamp,tr.type)]);}
 if(rampTime<duration)points.push([start+duration,to]);return points;
}
function profilePreviewCurves(phases){
 let time=0;const current={pressure:0,flow:0};
 const segments=phases.map((phase,index)=>{
  const duration=Math.max(.1,Number(phase.duration)||.1),segment={index,start:time,duration,label:phase.name,pressure:null,flow:null,power:null};
  if(typeof phase.pump==='number'){
   segment.power=Number(phase.pump);current.pressure=0;current.flow=0;
  }else{
   ['pressure','flow'].forEach(metric=>{
    const raw=Number(phase.pump?.[metric]??0),entry=current[metric],value=raw===-1?entry:raw,active=phase.pump?.target===metric;
    const role=active?'target':value>0?'limit':null;
    segment[metric]={role,value,entry,adaptive:raw===-1,points:role==='target'?profileTargetPoints(time,duration,entry,value,phase.transition):role==='limit'?[[time,value],[time+duration,value]]:[]};
    current[metric]=value;
   });
  }
  time+=duration;return segment;
 });
 const flowValues=segments.flatMap(s=>s.flow?.points?.map(p=>p[1])||[]);
 return {segments,total:Math.max(1,time),flowMax:Math.max(10,Math.min(15,Math.ceil(Math.max(0,...flowValues)/5)*5))};
}
function profilePlot(){
 if(!profileDraft)return;const node=document.getElementById('profile_preview');if(!node)return;
 const model=profilePreviewCurves(profileDraft.phases),total=model.total,mode=profileModeOf(profileDraft.phases[profilePhase]),axis=mode==='off'?'pressure':mode,m=profileModes[axis];
 const W=Math.max(690,node.clientWidth),H=238,L=45,R=22,T=42,B=34,pw=W-L-R,ph=H-T-B;
 const xp=t=>L+t/total*pw,yp=(k,v)=>T+ph-Math.max(0,Math.min(profileModes[k].max,v))/profileModes[k].max*ph;
 const caption=document.getElementById('profile_axis_caption');if(caption)caption.textContent='Thang đứng: '+m.label+' ('+m.unit+')';
 let svg=`<svg viewBox="0 0 ${W} ${H}" aria-label="Đường mục tiêu; các màu dùng đơn vị riêng" xmlns="http://www.w3.org/2000/svg"><defs><pattern id="profile_pause_hatch" width="8" height="8" patternUnits="userSpaceOnUse"><path d="M-2 2L2-2M0 8L8 0M6 10L10 6" stroke="#8298a5" stroke-opacity=".12"/></pattern></defs>`;
 const grid=[];for(let i=0;i<=4;i++){const v=m.max*i/4,y=yp(axis,v);grid.push(`<line x1="${L}" y1="${y}" x2="${W-R}" y2="${y}" stroke="#34434d" stroke-dasharray="2 5"/><text x="${L-9}" y="${y+4}" fill="${m.color}" font-size="10" text-anchor="end">${profileNum(v)}</text>`);}
 model.segments.forEach(seg=>{const x=xp(seg.start),w=seg.duration/total*pw;svg+=`<rect x="${x}" y="${T}" width="${w}" height="${ph}" fill="${seg.index===profilePhase?profileModes[profileModeOf(profileDraft.phases[seg.index])].color:'#0e1820'}" fill-opacity="${seg.index===profilePhase?'.05':'.6'}"/>`;});svg+=grid.join('');
 let uncertain=false;
 model.segments.forEach((seg,i)=>{const x=profileDraft.phases[i],k=profileModeOf(x),meta=profileModes[k],left=xp(seg.start),right=xp(seg.start+seg.duration),width=right-left,tr=x.transition||{},selected=i===profilePhase;
  const name=width>110?x.name:x.name.slice(0,Math.max(1,Math.floor(width/7)-3));
  svg+=`<line x1="${left}" y1="${T-20}" x2="${left}" y2="${T+ph}" stroke="#4a5d69" stroke-dasharray="3 5"/><text x="${left+width/2}" y="22" text-anchor="middle" font-size="10" fill="#b6ccd9">${i+1}. ${profileEsc(name)} (${profileNum(x.duration)} s)</text>`;
  if(k==='off')svg+=`<rect x="${left}" y="${T}" width="${width}" height="${ph}" fill="url(#profile_pause_hatch)"/><text x="${left+width/2}" y="${T+ph/2}" text-anchor="middle" fill="#a8bfce" font-size="10">Bơm tắt</text>`;
  else{
   const target=k==='power'?seg.power:seg[k].value,entry=k==='power'?target:seg[k].entry,ramp=k!=='power'&&profileRamp(x),measured=k!=='power'&&(seg[k].adaptive||(ramp&&tr.adaptive)),nonTime=ramp&&tr.target&&tr.target!=='time';
   const rt=ramp?(nonTime?seg.duration*.55:(tr.duration>0?tr.duration:seg.duration)):0;
   const points=ramp?profileTargetPoints(seg.start,seg.duration,entry,target,{...tr,target:'time',duration:rt}):[[seg.start,target],[seg.start+seg.duration,target]];
   const unknownEnd=k!=='power'&&seg[k].adaptive;
   svg+=`<polyline points="${points.map(([t,v])=>xp(t)+','+yp(k,v)).join(' ')}" fill="none" stroke="${meta.color}" stroke-width="${selected?3:2.4}" ${measured||nonTime?'stroke-dasharray="5 5"':''} stroke-linejoin="round"/>`;
   const hx=xp(ramp?seg.start+Math.min(rt,seg.duration):seg.start+seg.duration/2),hy=yp(k,points[points.length-1][1]);
   svg+=`<circle cx="${hx}" cy="${hy}" r="${selected?5:4}" fill="${unknownEnd?'#131c24':meta.color}" stroke="${meta.color}" stroke-width="1.5"/><text x="${Math.min(right-4,hx+10)}" y="${Math.max(T+14,hy-10)}" text-anchor="${hx+65>right?'end':'start'}" font-size="10" fill="${meta.color}">${unknownEnd?'Mốc đầu bước':(rt>seg.duration?'Đích ':'')+profileNum(target)+' '+meta.unit}</text>`;
   if(ramp&&(measured||nonTime))svg+=`<circle cx="${left+2}" cy="${yp(k,entry)}" r="4" fill="#131c24" stroke="${meta.color}" stroke-width="1.5"/><text x="${left+8}" y="${yp(k,entry)-8}" fill="${meta.color}" font-size="11">?</text>`;
   if(nonTime)svg+=`<text x="${left+width/2}" y="${T+ph-10}" text-anchor="middle" fill="#b7c9d6" font-size="9">Ramp theo ${profileAxisUnit(x)}</text>`;
   if(k!=='power'){const lim=x.pump[k==='pressure'?'flow':'pressure'];if(lim)svg+=`<text x="${left+width/2}" y="${T+17}" text-anchor="middle" fill="#b3c6d2" font-size="10">Giới hạn ${lim===-1?'mốc đầu bước':profileNum(lim)+' '+profileModes[k==='pressure'?'flow':'pressure'].unit}</text>`;}
  }
  svg+=`<rect data-pick="${i}" role="button" tabindex="0" aria-label="Chọn bước ${i+1}: ${profileEsc(x.name)}" x="${left}" y="${T-22}" width="${width}" height="${ph+22}" fill="transparent"><title>${profileEsc(profileDescribe(x,i))}</title></rect>`;
  if(x.targets?.length)uncertain=true;
  svg+=`<text x="${right}" y="${H-10}" text-anchor="${i===model.segments.length-1?'end':'middle'}" fill="#a8bfcd" font-size="10">${uncertain?'≤ ':''}${profileNum(seg.start+seg.duration)} s</text>`;
 });
 svg+=`<text x="${L}" y="${H-10}" fill="#a8bfcd" font-size="10">0 s</text>`;
 if(total>60)svg+=`<rect x="${xp(60)}" y="${T}" width="${xp(total)-xp(60)}" height="${ph}" fill="#ed8586" fill-opacity=".06" pointer-events="none"/><line x1="${xp(60)}" y1="${T}" x2="${xp(60)}" y2="${T+ph}" stroke="#ed8586" stroke-dasharray="5 4" pointer-events="none"/><text x="${xp(60)-5}" y="${T+15}" text-anchor="end" fill="#ed8586" font-size="10">Dừng 60 s</text>`;
 node.innerHTML=svg+'</svg>';
 document.getElementById('profile_chart_note').textContent='Mỗi màu dùng đơn vị riêng · ? / nét đứt: mốc đo chưa biết hoặc ramp theo gram/ml · Thời gian có thể rút ngắn theo điều kiện.';
 node.querySelectorAll('[data-pick]').forEach(e=>{e.onclick=()=>profilePick(Number(e.dataset.pick));e.onkeydown=k=>{if(k.key==='Enter'||k.key===' '){k.preventDefault();e.onclick();}};});
}
window.addEventListener('resize',()=>{if(profileDraft){profilePhaseList();profilePlot();}});
