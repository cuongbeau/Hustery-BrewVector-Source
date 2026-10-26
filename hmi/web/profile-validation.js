// Device limits from protocol/profile_codec.h
var profileValidate=(function(){
// View adapter for protocol/profile_codec.h. Canonical JSON is retained.
const copy=x=>JSON.parse(JSON.stringify(x));
const bytes=s=>new TextEncoder().encode(s).length;
const obj=x=>x&&typeof x==='object'&&!Array.isArray(x);
const fail=s=>{throw Error(s);};
function fields(o,keys){if(!obj(o)||Object.keys(o).some(k=>!keys.includes(k)))fail('Trường JSON chưa hỗ trợ; không thể nhập mà giữ nguyên thiết lập.');}
function num(n,lo,hi){return Number.isFinite(n)&&n>=lo&&n<=hi;}
const newId=()=>crypto.randomUUID();
function validateProfile(input){
 const p=copy(input);fields(p,['id','label','type','description','temperature','favorite','selected','utility','phases']);
 p.id??=newId();p.description??='';p.temperature??=0;
 if(typeof p.id!=='string'||!p.id||bytes(p.id)>39||typeof p.label!=='string'||!p.label.trim()||bytes(p.label)>23||typeof p.description!=='string'||bytes(p.description)>191)fail('ID/tên/mô tả vượt giới hạn: 39/23/191 byte UTF-8.');
 if(!['pro','standard'].includes(p.type)||!num(p.temperature,0,160)||!Array.isArray(p.phases)||!p.phases.length||p.phases.length>8)fail('Loại profile, nhiệt độ hoặc số phase không hợp lệ.');
 for(const k of ['favorite','selected','utility'])if(p[k]!==undefined&&typeof p[k]!=='boolean')fail('Cờ profile phải là boolean.');
 for(const x of p.phases){
  fields(x,['name','phase','valve','duration','temperature','transition','pump','targets']);x.phase??='brew';x.temperature??=0;
  if(typeof x.name!=='string'||!x.name.trim()||bytes(x.name)>15||!['brew','preinfusion'].includes(x.phase)||!num(x.duration,.1,60)||!num(x.temperature,0,160))fail('Tên phase tối đa15 byte; thời gian0.1–60s; nhiệt0–160°C.');
  if(x.valve!==1&&x.valve!==true)fail('Máy hiện chỉ hỗ trợ valve=1.');
  if(obj(x.pump)){fields(x.pump,['target','pressure','flow']);if(!['pressure','flow'].includes(x.pump.target))fail('Pump target không hợp lệ.');for(const [k,max] of [['pressure',12],['flow',15]])if(x.pump[k]!==-1&&!num(x.pump[k],0,max))fail(k+' phải là -1 hoặc 0..'+max);}
  else if(!Number.isInteger(x.pump)||!num(x.pump,0,100))fail('Power phải là số nguyên0..100.');
  x.transition??={};const t=x.transition;fields(t,['type','target','duration','adaptive','fallback_seconds']);t.type??='instant';t.target??='time';t.duration??=0;t.adaptive??=false;t.fallback_seconds??=x.duration;
  if(!['instant','linear','ease-in','ease-out','ease-in-out'].includes(t.type)||!['time','volumetric','pumped'].includes(t.target)||!num(t.duration,0,t.target==='time'?60:500)||typeof t.adaptive!=='boolean'||!num(t.fallback_seconds,.1,x.duration))fail('Transition/fallback không hợp lệ.');
  x.targets??=[];if(!Array.isArray(x.targets)||x.targets.length>8)fail('Tối đa8 điều kiện.');for(const y of x.targets){fields(y,['type','operator','value']);y.operator??='gte';if(!['volumetric','pressure','flow','pumped'].includes(y.type)||!['gte','lte'].includes(y.operator)||!num(y.value,0,500))fail('Điều kiện kết thúc không hợp lệ.');}
 }
 return p;
}

return function(p){try{validateProfile(p);return "";}catch(e){return e.message;}};
})();
