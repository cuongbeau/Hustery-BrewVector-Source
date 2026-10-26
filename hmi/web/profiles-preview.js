// Profile target preview. Pressure and flow share time; measured values are never inferred.
function profileEase(t,type){t=Math.max(0,Math.min(1,t));return type==='ease-in'?t*t:type==='ease-out'?1-(1-t)*(1-t):type==='ease-in-out'?(t<.5?2*t*t:1-2*(1-t)*(1-t)):t;}
function profileTargetPoints(start,duration,from,to,transition){
 const tr=transition||{},ramp=tr.type&&tr.type!=='instant'&&Number(tr.duration)>0;
 if(!ramp){if(from===to)return [[start,to],[start+duration,to]];const step=Math.min(.06,duration/20);return [[start,from],[start+step,to],[start+duration,to]];}
 const rampTime=Math.min(duration,Number(tr.duration)),points=[],count=24;
 for(let i=0;i<=count;i++){const elapsed=rampTime*i/count;points.push([start+elapsed,from+(to-from)*profileEase(elapsed/rampTime,tr.type)]);}
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
 const model=profilePreviewCurves(profileDraft.phases),segments=model.segments,total=model.total,mobile=window.matchMedia('(max-width:760px)').matches;
 const W=Math.max(320,node.clientWidth),H=mobile?285:260,L=mobile?43:50,R=mobile?39:48,T=34,B=34,plotW=W-L-R,plotH=H-T-B;
 const xp=t=>L+t/total*plotW,yp=(metric,v)=>T+plotH-Math.max(0,Math.min(metric==='pressure'?12:model.flowMax,v))/(metric==='pressure'?12:model.flowMax)*plotH;
 let svg=`<svg viewBox="0 0 ${W} ${H}" role="img" aria-label="Mục tiêu áp suất và flow theo thời gian tối đa"><rect x="${L}" y="${T}" width="${plotW}" height="${plotH}" rx="3" fill="#0e1820"/>`;
 const selected=segments[profilePhase];if(selected)svg+=`<rect x="${xp(selected.start)}" y="${T}" width="${selected.duration/total*plotW}" height="${plotH}" fill="#24c9ed" fill-opacity=".07"/>`;
 for(let i=0;i<=4;i++){
  const pressure=12*i/4,flow=model.flowMax*i/4,y=yp('pressure',pressure);
  svg+=`<line x1="${L}" y1="${y}" x2="${W-R}" y2="${y}" stroke="#34434d" stroke-dasharray="3 6"/><text x="${L-8}" y="${y+4}" fill="#a2afb7" font-size="10" text-anchor="end">${pressure}</text><text x="${W-R+8}" y="${y+4}" fill="#a2afb7" font-size="10">${Number(flow.toFixed(1))}</text>`;
 }
 svg+=`<text x="${mobile?4:8}" y="${T+plotH/2}" fill="#24c9ed" font-size="10" font-weight="700">bar</text><text x="${W-4}" y="${T+plotH/2}" fill="#efb769" font-size="10" font-weight="700" text-anchor="end">ml/s</text>`;
 segments.forEach((segment,i)=>{
  const x=xp(segment.start),width=segment.duration/total*plotW,label=width>82?segment.label:String(i+1);
  svg+=`<line x1="${x}" y1="${T-12}" x2="${x}" y2="${T+plotH}" stroke="#52616b" stroke-dasharray="4 4"/><text x="${x+width/2}" y="18" fill="${profileColors[i]}" font-size="10" text-anchor="middle">${profileEsc(label.length>Math.floor(width/7)?label.slice(0,Math.max(1,Math.floor(width/7)-1))+'…':label)}</text>`;
  ['pressure','flow'].forEach(metric=>{
   const data=segment[metric];if(!data?.role||!data.points.length)return;const color=metric==='pressure'?'#24c9ed':'#efb769';
   svg+=`<polyline points="${data.points.map(([t,v])=>xp(t)+','+yp(metric,v)).join(' ')}" fill="none" stroke="${color}" stroke-width="${data.role==='target'?2.8:2.2}" ${data.role==='limit'?'stroke-dasharray="7 5" opacity=".9"':''} stroke-linejoin="round"/>`;
   if(data.adaptive){const cy=yp(metric,data.entry);svg+=`<circle cx="${x}" cy="${cy}" r="5" fill="#131c24" stroke="${color}" stroke-width="2"/>`;if(i===profilePhase&&width>85)svg+=`<text x="${x+10}" y="${Math.max(T+12,cy-10)}" fill="${color}" font-size="9">Adaptive entry</text>`;}
  });
  svg+=`<rect data-pick="${i}" role="button" tabindex="0" aria-label="Phase ${i+1}: ${profileEsc(segment.label)}" x="${x}" y="${T-14}" width="${width}" height="${plotH+14}" fill="transparent"><title>${profileEsc(segment.label+' · '+profilePhaseSummary(profileDraft.phases[i]))}</title></rect>`;
 });
 svg+=`<line x1="${L}" y1="${T+plotH}" x2="${W-R}" y2="${T+plotH}" stroke="#52616b"/>`;
 const boundaries=[0,...segments.map(s=>s.start+s.duration)],shown=[];
 boundaries.forEach((time,i)=>{const x=xp(time);if(i!==0&&i!==boundaries.length-1&&shown.length&&x-shown[shown.length-1]<48)return;shown.push(x);svg+=`<text x="${x}" y="${H-9}" fill="#a2afb7" font-size="10" text-anchor="${i===0?'start':i===boundaries.length-1?'end':'middle'}">${Number(time.toFixed(1))} s</text>`;});
 if(total>60)svg+=`<line x1="${xp(60)}" y1="${T}" x2="${xp(60)}" y2="${T+plotH}" stroke="#ef6d6d" stroke-dasharray="5 3"/>`;
 node.innerHTML=svg+'</svg>';
 document.getElementById('profile_chart_note').textContent='Xem trước mục tiêu · Thời gian phase là tối đa · Nét đứt là giới hạn.';
 node.querySelectorAll('[data-pick]').forEach(e=>{e.onclick=()=>profilePick(Number(e.dataset.pick));e.onkeydown=k=>{if(k.key==='Enter'||k.key===' '){k.preventDefault();e.onclick();}};});
}
window.addEventListener('resize',()=>{if(profileDraft){profilePhaseList();profilePlot();}});
