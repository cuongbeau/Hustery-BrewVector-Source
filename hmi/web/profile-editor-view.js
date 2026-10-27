// Block editor for the canonical device profile. All writes go through saveProfiles().
const profilePencil='<svg viewBox="0 0 24 24" aria-hidden="true" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="m15 4 5 5M4 20l4-1L20 7a2 2 0 0 0-4-4L4 15zM13 20h8"/></svg>';
function profileHelp(text){return `<button type="button" class="profile-help" title="${profileEsc(text)}" aria-label="${profileEsc(text)}" onclick="profileHint(this)">i</button>`;}
function profileHint(button){let hint=document.getElementById('profile_hint');if(hint&&hint.dataset.owner===button.title){hint.remove();return;}if(hint)hint.remove();hint=document.createElement('div');hint.id='profile_hint';hint.className='profile-hint-popup';hint.role='status';hint.textContent=button.title;hint.dataset.owner=button.title;document.getElementById('profiles_workspace').append(hint);const r=button.getBoundingClientRect();hint.style.left=Math.max(10,Math.min(r.left,window.innerWidth-hint.offsetWidth-10))+'px';hint.style.top=Math.max(10,Math.min(r.bottom+8,window.innerHeight-hint.offsetHeight-10))+'px';button.onblur=()=>hint.remove();}
function profilePhaseMenu(){return `<details class="profile-menu"><summary aria-label="Thao tác bước">⋯</summary><div class="profile-menu-items">${[['copy','Nhân bản',profileDraft.phases.length>=8],['up','Đưa lên trước',profilePhase===0],['down','Đưa ra sau',profilePhase===profileDraft.phases.length-1],['delete','Xóa bước',profileDraft.phases.length===1]].map(([a,label,disabled])=>`<button onclick="profilePhaseAction('${a}')" ${disabled?'disabled':''}>${label}</button>`).join('')}</div></details>`;}
function profilePhaseList(){const list=document.getElementById('profile_phase_list');if(!list||!profileDraft)return;list.innerHTML=profileDraft.phases.map((x,i)=>{const mode=profileModeOf(x),m=profileModes[mode],v=profileValue(x),lim=typeof x.pump==='object'?x.pump[mode==='pressure'?'flow':'pressure']:0;
 return `<article class="profile-phase-card ${i===profilePhase?'profile-active':''}" style="--phase-color:${m.color}" role="button" tabindex="0" aria-pressed="${i===profilePhase}" aria-label="Chỉnh bước ${i+1}: ${profileEsc(x.name)}" onclick="profilePick(${i})" onkeydown="if(event.target===this&&(event.key==='Enter'||event.key===' ')){event.preventDefault();profilePick(${i})}"><div class="profile-card-title"><span class="profile-number">${i+1}</span><b>${profileEsc(x.name)}</b><span class="profile-grip" draggable="true" data-phase-drag="${i}" title="Kéo để đổi thứ tự; menu ⋯ có thao tác tương đương">⠿</span></div><div class="profile-card-value">${mode==='off'?'Bơm tắt':profileRampArrow(x,i)+(v===-1?'Mốc đầu bước':profileNum(v)+' '+m.unit)}</div><div class="profile-card-exit">${profileEsc(profileExitShort(x))}</div>${lim?`<span class="profile-card-limit">Giới hạn ${lim===-1?'mốc đầu bước':profileNum(lim)+' '+profileModes[mode==='pressure'?'flow':'pressure'].unit}</span>`:''}</article>`;}).join('')+`<button class="profile-add-step" onclick="profilePhaseAction('add')" ${profileDraft.phases.length>=8?'disabled':''} aria-label="Thêm bước">＋</button>`;
 const active=list.querySelector('.profile-active');if(active){const left=active.offsetLeft-list.firstElementChild.offsetLeft;if(left<list.scrollLeft||left+active.clientWidth>list.scrollLeft+list.clientWidth)list.scrollLeft=Math.max(0,left-(list.clientWidth-active.clientWidth)/2);}
 let moving=null;list.ondragstart=e=>{const h=e.target.closest('[data-phase-drag]');if(profileBusy||!h||!profileFinishInline())return;moving=Number(h.dataset.phaseDrag);e.dataTransfer.setData('text/plain',String(moving));e.dataTransfer.effectAllowed='move';};list.ondragover=e=>{if(moving!==null)e.preventDefault();};list.ondrop=e=>{e.preventDefault();const card=e.target.closest('.profile-phase-card');if(profileBusy||moving===null||!card)return;const to=[...list.querySelectorAll('.profile-phase-card')].indexOf(card);if(to!==moving){const x=profileDraft.phases.splice(moving,1)[0];profileDraft.phases.splice(to,0,x);profilePhase=to;profileDirty=true;profileEditor();}moving=null;};list.ondragend=()=>moving=null;list.onscroll=profileConnector;profileConnector();
}
function profileConnector(){const box=document.getElementById('profile_inspector'),list=document.getElementById('profile_phase_list'),card=list?.querySelector('.profile-active');if(!box||!card)return;const r=card.getBoundingClientRect(),b=box.getBoundingClientRect(),lr=list.getBoundingClientRect(),x=r.left+r.width/2;box.classList.toggle('profile-no-connector',x<lr.left||x>lr.right);box.style.setProperty('--connector',Math.max(15,Math.min(b.width-15,x-b.left))+'px');}
function profileSummaryRows(){return profileDraft.phases.map((x,i)=>`<button class="profile-summary-row" onclick="profilePick(${i})"><b>${i+1}. ${profileEsc(x.name)}</b><span>${profileEsc(profileDescribe(x,i))}</span></button>`).join('');}
function profileEditor(){
 if(!profileDraft||!profileFinishInline())return;
 profileDerivedPending=false;
 const p=profileDraft,x=p.phases[profilePhase],m=profileModes[profileModeOf(x)];x.transition??={};x.targets??=[];
 const message=(document.getElementById('profile_editor_status')||document.getElementById('profile_status'))?.textContent||'';
 const opened=[...document.querySelectorAll('#profile_edit details.profile-fold[open]')].map(e=>e.id);
 document.getElementById('profile_library').hidden=true;
 document.getElementById('profile_edit').innerHTML=`
 <button class="profile-link" onclick="closeEditor()">‹ Danh sách profile</button>
 <header class="profile-heading">
  <div class="profile-heading-fields"><input id="profile_name" aria-label="Tên profile" value="${profileEsc(p.label)}"></div>
  <button onclick="newProfile()">＋ Tạo mới</button><button id="profile_save" onclick="saveProfiles()">Lưu vào Main</button>
  <details class="profile-menu"><summary aria-label="Thao tác profile">⋯</summary><div class="profile-menu-items">
   <button onclick="profileToggleNotes()">Ghi chú</button><button onclick="exportProfile()">Xuất JSON</button>
   <button onclick="document.getElementById('profile_json').hidden=false;document.getElementById('profile_json').open=true">JSON / thiết lập được giữ</button>
   <button onclick="profileDelete()" ${selectedIdx>=cacheProfiles.length?'disabled':''}>Xóa bài trên Main</button>
  </div></details>
 </header>
 <div id="profile_notes" hidden><label>Ghi chú<textarea id="profile_desc" rows="2" onchange="profileSaveNote(this.value)">${profileEsc(p.description||'')}</textarea></label></div>
 <div class="profile-stopbar"><span>■ Dừng toàn shot ${profileHelp('Giới hạn runtime cố định 60 giây. Hoàn tất bước cuối cũng kết thúc shot. Điều kiện gram thuộc từng bước.')}</span><span>Giới hạn 60 s</span></div>
 <div id="profile_phase_list" class="profile-phases" aria-label="Các bước pha"></div>
 <section id="profile_inspector" data-mode="${profileModeOf(x)}" class="profile-inspector" style="--phase-color:${m.color}">
  <header class="profile-inspector-head">${profilePencil}<span>Đang chỉnh bước ${profilePhase+1} ·</span><input id="profile_phase_name" aria-label="Tên bước" value="${profileEsc(x.name)}" style="width:${Math.max(8,x.name.length+2)}ch">${profilePhaseMenu()}</header>
  ${profileInlineRows(x)}
 </section>
 <section class="profile-chart"><div class="profile-chart-head">
  <h3>Xem toàn bài ${profileHelp('Đường mục tiêu, không phải cảm biến thực. Các màu dùng đơn vị riêng; trục đứng theo bước đang chọn. Dấu ? / nét đứt chỉ mốc đo chưa biết hoặc ramp theo gram/ml.')}</h3>
  <div class="profile-chart-legend"><span class="profile-legend-pressure">Áp suất (bar)</span><span class="profile-legend-flow">Lưu lượng (ml/s)</span><span class="profile-legend-power">Công suất (%)</span></div>
  <span id="profile_axis_caption" class="profile-muted"></span>
 </div><div class="profile-chart-scroll"><div id="profile_preview"></div></div><p id="profile_chart_note" class="profile-muted profile-chart-note"></p></section>
 <div class="profile-summary-bar"><details id="profile_summary" class="profile-fold"><summary>Tóm tắt bài pha</summary><div id="profile_summary_rows">${profileSummaryRows()}</div></details><button onclick="document.getElementById('profile_json').hidden=false;document.getElementById('profile_json').open=true">{ } JSON</button></div>
 <details id="profile_json" class="profile-fold" hidden><summary>JSON đầy đủ</summary><pre id="profile_raw">${profileEsc(JSON.stringify(p,null,2))}</pre></details>
 <footer class="profile-footer"><p id="profile_editor_status" role="status">${profileEsc(message)}</p><span id="profile_dirty">${profileDirty?'Có thay đổi chưa lưu':'Đã tải từ Main'}</span><button onclick="profileResetDraft()">Hủy thay đổi</button></footer>`;
 opened.forEach(id=>{const e=document.getElementById(id);if(e){e.open=true;e.hidden=false;}});
 profilePhaseList();profilePlot();
 for(const [id,object,key] of [['profile_name',p,'label'],['profile_phase_name',x,'name']])document.getElementById(id).onchange=e=>{if(profileBusy)return;object[key]=e.target.value;profileDirty=true;profileEditor();};
}
function profileToggleNotes(){if(profileBusy||!profileFinishInline())return;const node=document.getElementById('profile_notes');node.hidden=!node.hidden;node.closest('#profile_edit').querySelector('.profile-heading .profile-menu').open=false;if(!node.hidden)document.getElementById('profile_desc').focus();}
function profileSaveNote(value){if(profileBusy)return;profileDraft.description=value;profileDirty=true;profileScheduleDerived();}
