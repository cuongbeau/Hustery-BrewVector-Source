// File: web_html.h
// Split from Web_dashboard.h (HTML + CSS + shell + m/ng <script>)

#ifndef WEB_HTML_H
#define WEB_HTML_H

#include <Arduino.h>

// Phn u: <!doctype html> ... n ngay tr degc code JS (m <script>)
static const char WEB_HTML_TOP[] PROGMEM = R"HTML(
<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>HUSTERY BREWVECTOR</title>
  <style>
    :root{
      --bg:#090b0e;
      --bg-card:#12151a;
      --bg-card-soft:#0f1216;
      --border:#2a3038;
      --border-strong:#3a424d;
      --accent:#35c9f2;
      --accent-soft:#25aecd;
      --accent2:#ffca28;
      --danger:#ff5252;
      --ok:#55cf59;
      --text:#f2f5f7;
      --muted:#959da9;
      --grid:#252b32;
      --pressure:#55cf59;
      --pressure-target:#9e9e9e;
      --flow:#ffca28;
      --weight:#42a5f5;
      --temp:#ef5350;
      --radius:12px;
      --touch:44px;
      --space-1:4px;
      --space-2:8px;
      --space-3:12px;
      --space-4:16px;
      --space-5:20px;
    }
    *{box-sizing:border-box;margin:0;padding:0}
    html{
      width:100%;
      height:100%;
      overflow:hidden;
      overscroll-behavior:none;
    }
    body{
      font-family:-apple-system,BlinkMacSystemFont,"SF Pro Text",system-ui,Segoe UI,Roboto,Helvetica,Arial,sans-serif;
      background:var(--bg);
      color:var(--text);
      padding:clamp(6px, 1vh, 10px);
      padding-top:max(clamp(6px, 1vh, 10px), env(safe-area-inset-top));
      overscroll-behavior: none;
      overflow:hidden;
      height:100svh;
      min-height:100svh;
      width:100vw;
      position:fixed;
      inset:0;
      display:flex;
    }
    a{color:var(--accent-soft);text-decoration:none}
    a:hover{text-decoration:underline}

    .shell{
      max-width:1460px;
      width:100%;
      margin:0 auto;
      display:flex;
      flex-direction:column;
      gap:10px;
      flex:1;
      height:auto;
      min-height:0;
    }

    .topbar{
      display:flex;
      align-items:center;
      justify-content:space-between;
      padding:clamp(8px, 1.2vh, 10px) 14px;
      border-radius:12px;
      background:linear-gradient(135deg,#181b22,#111214);
      border:1px solid var(--border);
      box-shadow:none;
    }
    .topbar-left{
      display:flex;
      align-items:center;
      gap:10px;
    }
    .logo{
      width:28px;
      height:28px;
      border-radius:50%;
      background:conic-gradient(from 210deg,#00e5ff,#42a5f5,#7e57c2,#00e5ff);
      display:flex;
      align-items:center;
      justify-content:center;
      color:#111;
      font-weight:700;
      font-size:16px;
      box-shadow:0 0 12px rgba(0,229,255,0.6);
    }
    .top-title{font-weight:600;font-size:16px;}
    .top-sub{font-size:12px;color:var(--muted);}
    .topbar-right{
      display:flex;
      align-items:center;
      gap:8px;
      font-size:12px;
    }
    .connection-pill{
      display:flex;
      align-items:center;
      gap:7px;
      min-height:34px;
      padding:5px 10px;
      border:1px solid var(--border);
      border-radius:8px;
      background:#0d1014;
      color:var(--muted);
      white-space:nowrap;
    }
    .connection-pill strong{display:block;color:var(--text);font-size:10px;line-height:1.1;font-weight:700;}
    .connection-pill span{display:block;font-size:9px;line-height:1.15;margin-top:2px;}
    .connection-pill.ok span{color:var(--ok);}
    .connection-pill.bad span{color:var(--danger);}
    .wifi-pill{
      display:flex;
      align-items:center;
      gap:6px;
      padding:4px 10px;
      border-radius:999px;
      font-size:11px;
      border:1px solid var(--border);
      background:rgba(0,0,0,0.3);
    }
    .dot{
      width:8px;height:8px;border-radius:50%;
      background:var(--danger);
      box-shadow:0 0 8px rgba(255,82,82,0.6);
    }
    .dot.ok{
      background:var(--ok);
      box-shadow:0 0 8px rgba(102,187,106,0.6);
    }

    .tabs{
      display:flex;
      gap:8px;
      margin-top:clamp(2px, 0.7vh, 6px);
      margin-bottom:clamp(2px, 0.7vh, 6px);
    }
    .tab{
      padding:6px 12px;
      border-radius:999px;
      border:1px solid var(--border);
      font-size:12px;
      cursor:pointer;
      user-select:none;
      background:#16181e;
      color:var(--muted);
    }
    .tab.active{
      background:var(--accent);
      border-color:var(--accent);
      color:#111;
    }

    .page{display:none;}
    .page.active{display:block;}
    #page-dash{flex:1;min-height:0;}
    #page-hist{flex:1;min-height:0;overflow:hidden;}
    #page-hist.active{display:flex;}

    .hist-page{width:100%;min-height:0;display:grid;grid-template-columns:270px minmax(0,1fr);gap:12px;}
    .hist-side,.hist-detail{min-height:0;background:var(--bg-card);border:1px solid var(--border);border-radius:12px;overflow:hidden;}
    .hist-side{display:flex;flex-direction:column;}
    .hist-side-head{padding:12px 14px;border-bottom:1px solid var(--border);display:flex;align-items:center;justify-content:space-between;}
    .hist-count{font-size:10px;color:var(--accent);border:1px solid rgba(0,229,255,.35);padding:3px 7px;border-radius:999px;}
    .hist-list{padding:6px;overflow:auto;min-height:0;}
    .hist-shot{width:100%;text-align:left;color:var(--text);background:transparent;border:1px solid transparent;border-radius:8px;padding:10px;cursor:pointer;margin-bottom:3px;}
    .hist-shot:hover{background:#202329}.hist-shot.active{background:rgba(0,229,255,.08);border-color:rgba(0,229,255,.45);}
    .hist-shot-top{display:flex;justify-content:space-between;font-weight:700;font-size:12px;}
    .hist-shot-sub{font-size:10px;color:var(--muted);margin-top:5px;}
    .hist-tag{font-size:9px;color:var(--accent2);border:1px solid rgba(255,202,40,.35);padding:2px 5px;border-radius:4px;}
    .hist-detail{display:flex;flex-direction:column;overflow:auto;}
    .hist-detail{gap:12px;padding:14px;background:#0e1218;}
    .hist-detail-head,.hist-chart-panel,.hist-analysis{background:#151b22;border:1px solid #303b46;border-radius:12px;}
    .hist-detail-head{padding:18px;}.hist-head-row{display:flex;justify-content:space-between;gap:16px;align-items:flex-start;}
    .hist-eyebrow{font-size:10px;letter-spacing:.13em;font-weight:800;color:var(--accent);margin-bottom:7px;}
    .hist-title{font-size:21px;font-weight:800;line-height:1.25;}.hist-title span{color:#b5c4d2;font-weight:500;}.hist-meta{font-size:11px;color:var(--muted);margin-top:6px;}
    .hist-download{white-space:nowrap;padding:8px 12px;border:1px solid rgba(53,201,242,.5);border-radius:7px;font-size:11px;font-weight:700;color:var(--accent);background:#101c25;cursor:pointer;}
    .hist-metrics{display:grid;grid-template-columns:repeat(5,minmax(0,1fr));gap:9px;margin-top:18px;}
    .hist-metric{background:#0f151c;border:1px solid #283540;border-radius:9px;padding:12px;min-width:0;}.hist-metric span{display:block;font-size:10px;font-weight:800;letter-spacing:.05em;color:#8fa2b4;}.hist-metric b{display:block;font-size:20px;margin-top:7px;white-space:nowrap;}.hist-metric small{display:block;font-size:10px;color:#7f94a6;margin-top:5px;}
    .hist-chart-panel{padding:16px;}.hist-panel-top{display:flex;align-items:flex-start;justify-content:space-between;gap:15px;}.hist-panel-top b,.hist-section-head b{font-size:14px;}.hist-panel-top small{display:block;color:var(--muted);font-size:10px;margin-top:5px;}
    .hist-compare-control{font-size:10px;color:#9fb2c2;white-space:nowrap;}.hist-compare-control select{display:block;max-width:225px;margin-top:5px;background:#101820;border:1px solid #395060;color:var(--text);padding:7px 9px;border-radius:7px;font-size:11px;}
    .hist-compare-summary{display:flex;gap:12px;flex-wrap:wrap;align-items:center;background:#13222a;border:1px solid #31515c;border-radius:7px;padding:8px 10px;margin-top:10px;font-size:11px;}.hist-compare-summary>span{color:#a9b8c4;}.hist-compare-summary>span b{color:var(--text);}.hist-compare-state{font-size:11px;color:var(--accent2);margin-top:9px;}
    .hist-tabs{display:flex;gap:4px;margin-top:12px;border-bottom:1px solid #2c3943;overflow:auto;}.hist-mode{background:transparent;border:0;border-bottom:2px solid transparent;color:var(--muted);font-size:11px;font-weight:700;padding:9px 11px;cursor:pointer;white-space:nowrap}.hist-mode.active{color:var(--accent);border-color:var(--accent);}
    .hist-legend{display:flex;gap:13px;flex-wrap:wrap;color:#b0c0ce;font-size:10px;margin:13px 0 8px;}.hist-key{display:inline-block;width:17px;height:3px;margin:0 5px 3px 0;border-radius:2px;}.hist-compare-key{color:var(--accent2);}
    #hist_chart{width:100%;height:300px;display:block;background:#0c131a;border:1px solid #2c3943;border-radius:7px;touch-action:pan-y;}
    .hist-cursor{min-height:20px;padding:8px 2px 2px;font-size:11px;color:#a8bac8;}.hist-cursor b{color:#fff;}.hist-cursor span{color:var(--accent2);}
    .hist-phase-strip{position:relative;height:38px;margin-top:8px;border-bottom:1px solid #29414d;}.hist-phase-strip button{position:absolute;top:0;height:34px;box-sizing:border-box;min-width:0;padding:4px 2px;border-radius:4px;border:1px solid #34505d;background:#142630;color:#b9d7e3;cursor:pointer;display:flex;align-items:center;justify-content:center;gap:5px;font-size:10px;overflow:hidden;}.hist-phase-strip button:hover,.hist-phase-strip button:focus-visible{border-color:var(--accent);background:#1d3e4d;z-index:2;}.hist-phase-strip button.active{border-color:var(--accent);background:#176079;color:#fff;box-shadow:inset 0 0 0 1px rgba(53,201,242,.5);z-index:1;}.hist-phase-strip b,.hist-phase-strip span{overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
    .hist-analysis{padding:16px;}.hist-section-head{display:flex;align-items:baseline;gap:12px;margin-bottom:12px;}.hist-section-head span{font-size:10px;color:var(--muted);}
    .hist-name-source{font-size:10px;color:#91a5b6;margin:-3px 0 9px;}.hist-name-source button{border:0;background:none;color:var(--accent);font-size:10px;text-decoration:underline;cursor:pointer;padding:0 2px;}.hist-table-scroll{overflow:auto;}.hist-phase-table{width:100%;border-collapse:collapse;min-width:780px;text-align:left;font-size:11px;}.hist-phase-table th{font-size:9px;color:#91a5b6;letter-spacing:.03em;text-transform:uppercase;white-space:nowrap;padding:9px;border-bottom:1px solid #35434e;}.hist-phase-table td{padding:11px 9px;border-bottom:1px solid #28343e;white-space:nowrap;}.hist-phase-table tr:last-child td{border:0;}.hist-phase-table tbody tr{cursor:pointer;}.hist-phase-table tbody tr:hover,.hist-phase-table tbody tr.active{background:#19343f;}.hist-phase-table td:first-child b{color:var(--accent);}
    .hist-warnings{display:grid;gap:7px;}.hist-warnings div{padding:9px 11px;background:#121b23;border-left:3px solid var(--accent2);font-size:11px;color:#c5d0da;line-height:1.45;}
    .hist-empty{padding:36px 18px;text-align:center;color:var(--muted);font-size:12px;}
    @media(max-width:1100px){.hist-metrics{grid-template-columns:repeat(3,minmax(0,1fr));}}
    @media(max-width:760px){#page-hist{overflow:auto}.hist-page{grid-template-columns:1fr}.hist-side{max-height:210px}.hist-metrics{grid-template-columns:repeat(2,minmax(0,1fr))}#hist_chart{height:235px}.hist-panel-top{display:block}.hist-compare-control{display:block;margin-top:12px}.hist-section-head{display:block}.hist-section-head span{display:block;margin-top:5px}}

    /* ===== CONNECTIONS / HEATER / SYSTEM ===== */
    .settings-page{max-width:1120px;margin:0 auto;padding:4px 0 30px}.settings-title{font-size:18px;margin:5px 0 14px}.settings-grid{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:14px}.settings-card{background:var(--bg-card);border:1px solid var(--border);border-radius:10px;padding:16px}.settings-card h3{font-size:14px;margin:0 0 12px;padding-bottom:8px;border-bottom:1px solid var(--border)}.connection-summary{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:9px;margin-bottom:14px}.connection-tile{background:var(--bg-card-soft);border:1px solid var(--border);border-radius:8px;padding:11px}.connection-tile span{display:block;color:var(--muted);font-size:10px;text-transform:uppercase;letter-spacing:.06em}.connection-tile b{display:block;margin-top:5px;font-size:14px}.settings-form{display:grid;gap:10px}.settings-form label{font-size:11px;color:var(--muted)}.settings-form input,.settings-form select{width:100%;box-sizing:border-box;background:#111820;color:var(--text);border:1px solid var(--border-strong);border-radius:6px;padding:9px}.settings-form .checkbox-row{display:flex;align-items:center;gap:8px}.settings-form .checkbox-row input{width:auto;margin:0;padding:0}.settings-actions{display:flex;gap:8px;flex-wrap:wrap;margin-top:12px}.settings-actions button{padding:9px 13px}.device-list{display:grid;gap:7px;margin-top:10px}.device-row{display:grid;grid-template-columns:minmax(0,1fr) auto auto;gap:9px;align-items:center;background:var(--bg-card-soft);border:1px solid var(--border);border-radius:7px;padding:9px}.device-row.selected{border-color:var(--accent)}.device-row small{display:block;color:var(--muted);margin-top:3px}.status-ok{color:var(--ok)}.status-warn{color:var(--accent2)}.status-bad{color:var(--danger)}
    .heater-shell{max-width:1180px;margin:0 auto;padding-bottom:30px}.heater-head{display:flex;justify-content:space-between;gap:12px;align-items:flex-start;margin-bottom:12px}.heater-alarm{padding:9px 13px;border:1px solid var(--border);border-radius:999px;background:var(--bg-card-soft);font-size:11px;font-weight:800;white-space:nowrap}.heater-alarm.warning{background:#493813;border-color:#987528;color:#ffe09a}.heater-alarm.danger{background:#50171b;border-color:#9d3038;color:#fff}.heater-live{display:grid;grid-template-columns:1.3fr repeat(3,minmax(90px,1fr));gap:9px;margin-bottom:12px}.heater-live>div{background:#111820;border:1px solid var(--border);border-radius:8px;padding:11px}.heater-live span{display:block;color:var(--muted);font-size:10px;margin-bottom:4px}.heater-live strong{display:block;font-size:18px}.heater-live .heater-temperature strong{font-size:35px;color:#ff7547;line-height:1}.heater-live .heater-temperature small{display:block;margin-top:8px;color:var(--muted)}.heater-live .heater-temperature small b{color:var(--text);font-size:15px}.heater-comp-live{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));gap:7px;margin:10px 0}.heater-comp-live div{background:#111820;border:1px solid var(--border);border-radius:7px;padding:8px}.heater-comp-live span{display:block;color:var(--muted);font-size:9px}.heater-comp-live b{display:block;margin-top:3px;font-size:14px}.heater-chart-head{display:flex;align-items:center;justify-content:space-between;gap:10px;margin-bottom:8px}.heater-chart-head h3{margin:0!important;padding:0!important;border:0!important}.heater-legend{display:flex;gap:12px;flex-wrap:wrap;color:var(--muted);font-size:10px}.heater-legend i{display:inline-block;width:15px;height:2px;margin:0 5px 3px 0;background:currentColor}.heater-legend b{color:var(--text)}.heater-plot{width:100%;height:260px;background:#111820;border:1px solid var(--border);border-radius:8px;display:block}.heater-fields{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}.heater-field-group{background:#111820;border:1px solid var(--border);border-radius:8px;padding:12px}.heater-field-group h4{font-size:12px;margin:0 0 10px;padding-bottom:7px;border-bottom:1px solid var(--border)}.heater-field-group-grid{display:grid;grid-template-columns:repeat(2,minmax(110px,1fr));gap:10px}.heater-field-group label{font-size:10px;color:var(--muted)}.heater-field-group input{display:block;width:100%;box-sizing:border-box;margin-top:4px;padding:8px;background:#0d141b;color:var(--text);border:1px solid var(--border-strong);border-radius:5px;font-weight:800}.heater-field-help{display:block;margin-top:4px;color:#84909f;font-size:9px;line-height:1.35}.heater-config-note{margin:10px 0 0;color:var(--muted);font-size:10px;line-height:1.45}.heater-settings-switches{display:flex;align-items:center;gap:16px;flex-wrap:wrap;margin-top:12px}.heater-settings-switches label{font-size:11px}.heater-fold{margin-top:12px;border:1px solid var(--border);border-radius:8px;background:var(--bg-card)}.heater-fold summary{cursor:pointer;padding:12px 14px;font-weight:700}.heater-diagnostic-badge{display:none;margin-left:8px;padding:2px 7px;border:1px solid #987528;border-radius:999px;color:#ffe09a;background:#493813;font-size:9px}.heater-diagnostic-badge.visible{display:inline-block}.heater-fold-body{padding:0 14px 14px}.heater-pre{white-space:pre-wrap;color:var(--muted);font:11px/1.5 ui-monospace,monospace}.danger-button{background:#a7232d!important;color:#fff!important}.heat-mut:disabled{opacity:.45;cursor:not-allowed}
    .system-health{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:9px}.system-health div{background:var(--bg-card-soft);border:1px solid var(--border);border-radius:7px;padding:10px}.system-health span{display:block;color:var(--muted);font-size:10px}.system-health b{display:block;margin-top:4px;font-size:13px}
    @media(max-width:760px){.settings-grid{grid-template-columns:1fr}.connection-summary,.system-health{grid-template-columns:1fr}.heater-head{display:block}.heater-alarm{display:inline-block;margin:8px 0}.heater-live{grid-template-columns:repeat(2,minmax(0,1fr))}.heater-temperature{grid-column:1/-1}.heater-comp-live{grid-template-columns:repeat(2,minmax(0,1fr))}.heater-fields{grid-template-columns:1fr}.heater-field-group-grid{grid-template-columns:1fr}.heater-plot{height:225px}}

    .dash-view {
      display: none;
      animation: fadeIn 0.3s ease-out;
    }
    .dash-view.active {
      display: block;
      height:100%;
    }
    @keyframes fadeIn { from { opacity: 0; transform: translateY(5px); } to { opacity: 1; transform: translateY(0); } }
    @keyframes pulse {
      0% { transform: scale(1); box-shadow: 0 0 0 0 rgba(102,187,106, 0.7); }
      70% { transform: scale(1.02); box-shadow: 0 0 0 10px rgba(102,187,106, 0); }
      100% { transform: scale(1); box-shadow: 0 0 0 0 rgba(102,187,106, 0); }
    }
    .btn-pulse { animation: pulse 2s infinite; }

    .help-dot{
      display:inline-flex;
      align-items:center;
      justify-content:center;
      width:15px;
      height:15px;
      border-radius:50%;
      border:1px solid rgba(0,229,255,0.35);
      color:var(--accent);
      font-size:10px;
      font-weight:800;
      cursor:help;
      position:relative;
      flex-shrink:0;
      user-select:none;
    }
    .help-dot::after{
      content:attr(data-tip);
      position:absolute;
      left:0;
      top:calc(100% + 8px);
      transform:none;
      min-width:220px;
      max-width:320px;
      white-space:normal;
      line-height:1.45;
      text-transform:none;
      letter-spacing:0;
      font-weight:500;
      font-size:11px;
      color:#d8f8ff;
      background:#10151a;
      border:1px solid rgba(0,229,255,0.35);
      border-radius:8px;
      padding:8px 10px;
      box-shadow:0 10px 28px rgba(0,0,0,0.55);
      opacity:0;
      pointer-events:none;
      z-index:20000;
      transition:opacity 0.15s ease;
    }
    .help-dot:hover::after,
    .help-dot:focus::after,
    .help-dot:active::after{
      opacity:1;
    }

    @keyframes glow {
      0% { box-shadow: 0 0 5px rgba(102,187,106, 0.2); }
      50% { box-shadow: 0 0 20px rgba(102,187,106, 0.4); }
      100% { box-shadow: 0 0 5px rgba(102,187,106, 0.2); }
    }

    .layout-idle {
      display: grid;
      grid-template-columns: minmax(0,1.34fr) minmax(360px,0.76fr);
      gap: 18px;
      align-items: stretch;
      height:100%;
      min-height:0;
    }
    .dash-left-col {
      display:flex;
      flex-direction:column;
      gap:10px;
      min-height:0;
    }
    .idle-center-card {
      display: flex;
      flex-direction: column;
      padding: 0;
      overflow: hidden;
      background: linear-gradient(145deg, #1a1c20 0%, #14161a 100%);
      flex:1;
      min-height:0;
    }
    .idle-header-bar {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 12px 18px;
      background: rgba(255,255,255,0.02);
      border-bottom: 1px solid rgba(255,255,255,0.05);
      font-size: 11px;
      color: var(--muted);
      letter-spacing: 0.05em;
    }
    .header-item { display: flex; align-items: center; gap: 6px; }
    .header-icon { width: 16px; height: 16px; fill: currentColor; }

    /* Scale status */
    .scale-connected { color: var(--ok); }
    .scale-simulated  { color: #9ca3af; } /* Gray when not connected */

    /* ===== IDLE MAIN DISPLAY ===== */
    .idle-main-display {
      flex: 1;
      display: flex;
      flex-direction: column;
      align-items: stretch;
      justify-content: space-between;
      padding: 14px 22px 16px;
      text-align: center;
      gap: 12px;
    }
    .idle-cockpit {
      display: grid;
      grid-template-columns: minmax(300px, 1fr) 238px;
      gap: 34px;
      align-items: center;
      width: 100%;
    }
    .gauge-stack {
      display: flex;
      flex-direction: column;
      align-items: center;
      min-width: 0;
    }

    /* ===== ARC GAUGE (half-circle) ===== */
    /* Wrapper ch chim na trn ca SVG  clip phn d degi  degng knh */
    .temp-gauge-wrapper {
      position: relative;
      width: 292px;
      height: 218px;
      margin: 0 auto;
      overflow: visible;        /* cho gauge-label th ra */
    }
    .arc-svg {
      position: absolute;
      top: 0; left: 0;
      width: 270px;
      height: 270px;
      transform: scale(1.08);
      transform-origin: center top;
      overflow: visible;
      z-index: 0;
    }
    .arc-bg {
      fill: none;
      stroke: rgba(255,255,255,0.09);
      stroke-width: 17;
      stroke-linecap: round;
    }
    .arc-fill {
      fill: none;
      stroke: var(--accent-soft);
      stroke-width: 17;
      stroke-linecap: round;
      stroke-dasharray: 100;
      stroke-dashoffset: 100;
      transition: stroke-dashoffset 0.6s ease-out, stroke 0.3s ease;
      filter: drop-shadow(0 0 7px rgba(0,229,255,0.38));
    }
    /* S nhit  t GIA vng cung */
    .big-temp {
      position: absolute;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -35%);
      z-index: 2;
      font-size: 62px;
      font-weight: 800;
      line-height: 1;
      color: #ff6d3a;          /*  cam */
      text-shadow: 0 0 26px rgba(255,109,58,0.42);
      font-variant-numeric: tabular-nums;
      letter-spacing: -0.03em;
      white-space: nowrap;
    }
    .big-temp-unit {
      font-size: 20px;
      color: var(--muted);
      vertical-align: super;
      margin-left: 2px;
      font-weight: 300;
    }
    /* Nhn 0 degC / 130 degC */
    .gauge-labels {
      position: absolute;
      bottom: -6px;
      left: 6px; right: 6px;
      display: flex;
      justify-content: space-between;
      z-index: 1;
    }
    .gauge-label-text {
      font-size: 11px;
      color: rgba(255,255,255,0.4);
      font-weight: 600;
    }

    /* Overheat */
    @keyframes flashRed {
      0%, 100% { color: var(--danger); text-shadow: 0 0 28px rgba(255,82,82,0.9); }
      50%       { color: #ff6d3a;      text-shadow: 0 0 8px  rgba(255,109,58,0.5); }
    }
    .overheat-warn .big-temp { animation: flashRed 1s infinite; }
    .overheat-warn .arc-fill  { stroke: var(--danger) !important; }
    #overheat_alert {
      color: var(--danger);
      font-size: 13px;
      font-weight: 900;
      letter-spacing: 0.12em;
      margin-top: 2px;
      animation: flashRed 1s infinite;
    }

    /* Status pill */
    .idle-sub-status {
      font-size: 12px;
      color: var(--muted);
      margin-top: 8px;
      padding: 5px 18px;
      border-radius: 999px;
      background: rgba(255,255,255,0.04);
      border: 1px solid rgba(255,255,255,0.06);
    }
    .idle-telemetry {
      width: 100%;
      display: flex;
      flex-direction: column;
      gap: 8px;
    }
    .tele-card {
      text-align: left;
      padding: 10px 12px;
      border-radius: 10px;
      background: linear-gradient(135deg, rgba(20,30,40,0.92), rgba(14,18,24,0.96));
      border: 1px solid rgba(255,255,255,0.12);
      border-left: 4px solid var(--accent);
      box-shadow: inset 0 1px 0 rgba(255,255,255,0.05), 0 12px 28px rgba(0,0,0,0.28);
    }
    .tele-card.pressure { border-left-color: var(--ok); }
    .tele-card.scale { border-left-color: #a66cff; }
    .tele-main { display:flex; align-items:center; gap:10px; }
    .tele-icon {
      width:38px; height:38px; border-radius:50%;
      display:flex; align-items:center; justify-content:center;
      border:1px solid currentColor;
      color:var(--accent);
      font-size:0;
      font-weight:900;
      flex-shrink:0;
      background:rgba(0,229,255,0.045);
    }
    .pressure .tele-icon { color:var(--ok); background:rgba(102,187,106,0.045); }
    .scale .tele-icon { color:#a66cff; background:rgba(166,108,255,0.055); }
    .tele-icon svg { width:22px; height:22px; stroke:currentColor; fill:none; stroke-width:2; stroke-linecap:round; stroke-linejoin:round; }
    .tele-copy { min-width:0; }
    .tele-label {
      font-size: 9px;
      color: var(--muted);
      text-transform: uppercase;
      letter-spacing: 0.08em;
    }
    .tele-value {
      margin-top: 4px;
      font-size: 16px;
      font-weight: 800;
      color: var(--text);
      font-variant-numeric: tabular-nums;
    }
    .tele-hint {
      margin-top: 2px;
      font-size: 10px;
      color: #6f7785;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }

    /* ===== LAST SHOT BOX ===== */
    .last-shot-box {
      margin: auto auto 0;
      padding: 15px 10px 12px;
      border-radius: 12px;
      background: linear-gradient(135deg, rgba(22,28,36,0.78), rgba(18,20,24,0.9));
      border: 1px solid rgba(255,255,255,0.11);
      display: grid;
      grid-template-columns: 1fr 1px 1fr 1px 1.25fr 1px 1.2fr;
      align-items: center;
      width: 100%;
      position: relative;
    }
    .last-shot-title {
      position: absolute;
      top: -10px; left: 50%;
      transform: translateX(-50%);
      background: #1d222b;
      padding: 0 10px;
      font-size: 10px; font-weight: 800; color: var(--muted);
      letter-spacing: 0.1em; border-radius: 4px;
    }
    .last-shot-col {
      display: flex; flex-direction: column; gap: 3px;
      align-items: center; justify-content: center;
    }
    .last-shot-divider {
      width: 1px; height: 34px;
      background: rgba(255,255,255,0.12);
      align-self: center;
    }
    .last-shot-label { font-size: 9px; text-transform: uppercase; color: var(--muted); letter-spacing: 0.1em; }
    .last-shot-val   { font-size: 18px; font-weight: 800; color: var(--text); font-variant-numeric: tabular-nums; }
    .last-shot-note {
      color: #ff8c2a;
      font-size: 13px;
      font-weight: 700;
      text-align: center;
    }
    .analysis-icon { color:#ff8c2a; font-size:18px; line-height:1; }

    .action-row { display:flex; gap:12px; min-height:clamp(72px, 10.8vh, 90px); }
    .dash-action-btn {
      border-radius:12px;
      text-transform:uppercase;
      display:flex;
      flex-direction:column;
      align-items:center;
      justify-content:center;
      gap:8px;
      transition:0.2s;
      min-height:clamp(72px, 10.8vh, 90px);
    }
    .dash-flush-btn {
      flex:1;
      background:rgba(255,255,255,0.03);
      color:var(--text);
      border:1px solid var(--border);
      font-weight:bold;
      font-size:14px;
    }
    .dash-start-btn {
      flex:2;
      background:var(--ok);
      color:#111;
      border:none;
      font-weight:900;
      font-size:20px;
      letter-spacing:0.05em;
    }
    .action-sub {
      font-size: 10px;
      font-weight: 700;
      letter-spacing: 0;
      text-transform: none;
      opacity: 0.75;
    }
    .profile-list-head {
      display: flex;
      justify-content: space-between;
      align-items: flex-end;
      gap: 12px;
    }
    .profile-list-hint {
      font-size: 10px;
      color: var(--muted);
    }
    .dash-profile-card {
      padding: 16px 18px;
      border-radius: 12px;
      cursor: pointer;
      transition: all 0.22s ease;
      background: linear-gradient(135deg, rgba(20,25,31,0.9), rgba(12,14,18,0.96));
      border: 1px solid rgba(255,255,255,0.07);
      border-left: 5px solid transparent;
      opacity: 0.72;
    }
    .dash-profile-card.selected {
      background: linear-gradient(135deg, #1e2128 0%, #14161a 100%);
      border-color: rgba(102,187,106,0.42);
      border-left-color: var(--ok);
      box-shadow: 0 12px 32px rgba(0,0,0,0.38), 0 0 18px rgba(102,187,106,0.1);
      transform: translateX(5px);
      opacity: 1;
    }
    .profile-row { display:flex; justify-content:space-between; align-items:center; gap:10px; }
    .profile-name { font-size:17px; font-weight:900; letter-spacing:0.01em; color:#aaa; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
    .selected .profile-name { color:#fff; }
    .profile-chip { font-size:10px; color:var(--muted); border:1px solid rgba(255,255,255,0.1); border-radius:4px; padding:2px 6px; white-space:nowrap; }
    .profile-meta { display:flex; gap:10px; margin-top:8px; font-size:11px; color:var(--muted); font-weight:500; flex-wrap:wrap; }
    .profile-preview { display:flex; gap:4px; margin-top:12px; height:6px; }
    .profile-preview span { flex:1; border-radius:999px; background:rgba(255,255,255,0.08); }
    .profile-preview span:nth-child(1) { background:#58bfff; }
    .profile-preview span:nth-child(2) { background:#66d17a; }
    .profile-preview span:nth-child(3) { background:#ffca28; }
    .profile-preview span:nth-child(4) { background:#ff8c2a; }
    .profile-preview span:nth-child(5) { background:#ff5252; }
    .profile-phases { margin-top:8px; font-size:10px; color:#777f8b; white-space:nowrap; overflow:hidden; text-overflow:ellipsis; }
    .profile-mode { margin-top:8px; font-size:10px; color:var(--ok); font-weight:800; text-transform:uppercase; letter-spacing:0.05em; }
    .manual-bars { display:flex; gap:5px; width:180px; max-width:42%; }
    .manual-bars span { height:4px; flex:1; border-radius:999px; background:rgba(255,255,255,0.32); }
    .special-icon { color:var(--ok); font-size:24px; font-weight:800; }
    .manual-icon { color:#42a5f5; font-size:22px; }
    .profile-sub { margin-top:5px; color:#7b8390; font-size:11px; }
    .profile-col { display:flex; flex-direction:column; min-height:0; }
    .profile-card-shell { flex:1; min-height:0; display:flex; flex-direction:column; background:rgba(0,0,0,0.1); }
    
    .switch-wrap {
      display: flex; align-items: center; gap: 8px;
    }
    .switch {
      position: relative; display: inline-block; width: 34px; height: 20px;
    }
    .switch input { opacity: 0; width: 0; height: 0; }
    .slider {
      position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0;
      background-color: var(--border); transition: .3s; border-radius: 34px;
    }
    .slider:before {
      position: absolute; content: ""; height: 14px; width: 14px; left: 3px; bottom: 3px;
      background-color: white; transition: .3s; border-radius: 50%;
    }
    input:checked + .slider { background-color: var(--accent); }
    input:checked + .slider:before { transform: translateX(14px); }
    input:disabled + .slider { opacity: 0.5; cursor: not-allowed; }

    .layout-brew {
      display: grid;
      grid-template-columns: 300px 1fr;
      gap: 15px;
      align-items: stretch;
      height:100%;
      min-height:0;
    }
    .brew-sidebar {
      display: flex;
      flex-direction: column;
      gap: 10px;
      min-height:0;
    }
    
    /* Step Monitor Card */
    .step-monitor-card {
      background: linear-gradient(135deg, rgba(30,33,40,0.8), rgba(20,22,26,0.8));
      backdrop-filter: blur(10px);
      border: 1px solid rgba(255,255,255,0.08);
      border-radius: 12px;
      padding: 12px 15px;
      margin-bottom: 5px;
    }
    .step-label { font-size: 10px; color: var(--muted); text-transform: uppercase; letter-spacing: 0.1em; }
    .step-name { font-size: 14px; font-weight: 700; color: var(--accent); margin: 4px 0; }
    .step-progress-bg { height: 6px; background: rgba(255,255,255,0.1); border-radius: 3px; overflow: hidden; margin-top: 8px; }
    .step-progress-fill { height: 100%; background: var(--accent); width: 0%; transition: width 0.3s ease; }
    .brew-info-grid { display:grid; grid-template-columns:1fr 1fr; gap:8px; }
    .brew-info-cell {
      background:rgba(0,0,0,0.28);
      border:1px solid rgba(255,255,255,0.06);
      border-radius:8px;
      padding:9px 10px;
      min-width:0;
    }
    .brew-info-cell.wide { grid-column:1 / -1; }
    .brew-info-label { font-size:9px; color:var(--muted); text-transform:uppercase; letter-spacing:0.08em; font-weight:700; }
    .brew-info-value { margin-top:4px; font-size:15px; font-weight:900; color:var(--text); font-variant-numeric:tabular-nums; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
    .brew-info-sub { margin-top:2px; font-size:10px; color:#7b8390; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }

    .big-metric-card {
      background: rgba(255,255,255,0.03);
      backdrop-filter: blur(5px);
      border: 1px solid rgba(255,255,255,0.05);
      border-radius: 12px;
      padding: 15px;
      display: flex;
      flex-direction: column;
      position: relative;
      overflow: hidden;
    }
    .big-metric-header { display: flex; justify-content: space-between; align-items: flex-start; }
    .big-metric-label { font-size: 11px; color: var(--muted); text-transform: uppercase; letter-spacing: 0.05em; }
    .big-metric-main { display: flex; align-items: baseline; margin-top: 5px; z-index: 1; }
    .big-metric-val { font-size: 36px; font-weight: 800; color: var(--text); font-variant-numeric: tabular-nums; }
    .big-metric-unit { font-size: 14px; color: var(--muted); margin-left: 4px; }
    
    .sparkline-icon {
      position: absolute; right: 10px; bottom: 5px;
      width: 100px; height: 40px; opacity: 0.3;
      pointer-events: none;
    }

    .btn-stop-huge {
      background: rgba(239,83,80,0.1);
      color: var(--danger);
      border: 1px solid rgba(239,83,80,0.3);
      border-radius: 12px;
      padding: 15px;
      font-weight: 900;
      font-size: 18px;
      text-transform: uppercase;
      letter-spacing: 0.1em;
      cursor: pointer;
      transition: all 0.2s;
      margin-top: auto;
      height: 60px;
      width: 100%;
    }
    .btn-stop-huge:hover { background: var(--danger); color: #fff; }

    .chart-card {
      background: linear-gradient(180deg, #16181d 0%, #0d0f12 100%);
      padding: 20px;
    }
    .chart-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
    .chart-live-label { display: flex; align-items: center; gap: 8px; font-weight: 700; color: #fff; font-size: 15px; }
    .chart-live-dot { width: 8px; height: 8px; background: var(--ok); border-radius: 50%; animation: blink 1.5s infinite; }
    .brew-state-strip{display:flex;align-items:center;gap:8px;padding:9px 11px;border:1px solid rgba(102,187,106,.24);border-radius:10px;background:rgba(102,187,106,.07);font-size:10px;letter-spacing:.08em;text-transform:uppercase;}
    .brew-state-strip.ending{border-color:rgba(255,183,77,.35);background:rgba(255,183,77,.08);}
    .brew-state-strip.offline{border-color:rgba(239,83,80,.35);background:rgba(239,83,80,.08);}
    .brew-state-dot{width:8px;height:8px;border-radius:50%;background:var(--ok);box-shadow:0 0 10px rgba(102,187,106,.55);flex:none;}
    .brew-state-strip.ending .brew-state-dot{background:#ffb74d;box-shadow:0 0 10px rgba(255,183,77,.55);}
    .brew-state-strip.offline .brew-state-dot{background:var(--danger);box-shadow:none;}
    #brew_state_label{font-weight:900;color:var(--text);}
    #brew_link_age{margin-left:auto;color:var(--muted);letter-spacing:0;text-transform:none;}
    .brew-primary-metrics{background:rgba(0,0,0,.4);border-radius:12px;padding:5px 14px;border:1px solid rgba(255,255,255,.05);backdrop-filter:blur(10px);}
    .brew-metric{display:flex;justify-content:space-between;align-items:center;padding:10px 0;border-bottom:1px solid rgba(255,255,255,.06);}
    .brew-metric:last-child{border-bottom:0;}
    .brew-metric-label{color:var(--muted);font-size:10px;text-transform:uppercase;font-weight:700;letter-spacing:.07em;}
    .brew-metric-value{display:flex;align-items:baseline;gap:4px;min-width:0;}
    .brew-metric-number{font-size:24px;font-weight:900;font-variant-numeric:tabular-nums;white-space:nowrap;}
    .brew-metric-unit{font-size:11px;color:var(--muted);}
    .brew-stop-mobile{display:none;}
    #dash_status_banner{display:block!important;margin:-7px 0 10px;padding:8px 10px;border:1px solid rgba(255,255,255,.06);border-radius:8px;background:rgba(0,0,0,.22);color:var(--muted);font-size:10px;line-height:1.35;}
    @keyframes blink { 0% { opacity: 1; } 50% { opacity: 0.3; } 100% { opacity: 1; } }
    @media (max-height:720px) and (min-width:901px){
      html{
        overflow:hidden;
        width:100%;
        height:100%;
        background:#111214;
      }
      body{
        overflow:hidden;
        position:fixed;
        inset:0;
        width:100vw;
        height:100svh;
        min-height:100svh;
        background:radial-gradient(circle at top left,#202532 0,#111214 55%);
      }
      .shell{height:100%;min-height:0;gap:6px;}
      .topbar{padding:6px 12px;border-radius:10px;}
      .logo{width:24px;height:24px;font-size:14px;}
      .top-title{font-size:14px;}
      .top-sub{font-size:10px;}
      .tabs{margin-top:2px;margin-bottom:2px;gap:6px;}
      .tab{padding:5px 11px;}
      .dash-view.active{height:100%;}
      .layout-idle,.layout-brew{height:100%;min-height:0;}
      .action-row{min-height:58px;}
      .dash-action-btn{min-height:58px;gap:4px;}
      .dash-flush-btn{font-size:12px;}
      .dash-start-btn{font-size:17px;}
      .dash-action-btn svg{width:26px!important;height:26px!important;}
      .idle-header-bar{padding:7px 14px;}
      .idle-main-display{padding:10px 22px 10px;gap:8px;}
      .idle-cockpit{grid-template-columns:minmax(320px,1fr) 282px;gap:30px;}
      .temp-gauge-wrapper{transform:scale(1.04);margin:-2px 0 -4px;}
      .big-temp{font-size:66px;}
      .idle-sub-status{margin-top:0;padding:4px 15px;}
      .idle-telemetry{gap:8px;}
      .tele-card{padding:11px 13px;}
      .tele-icon{width:40px;height:40px;}
      .tele-icon svg{width:23px;height:23px;}
      .tele-value{font-size:17px;}
      .profile-card-shell{padding:8px;}
      .profile-list-head{padding-bottom:7px!important;margin-bottom:7px!important;}
      .dash-profile-card{padding:11px 15px;}
      .profile-preview{margin-top:8px;}
      .profile-phases,.profile-mode{margin-top:6px;}
      #brew_sidebar_live{gap:7px!important;}
      .brew-sidebar .step-monitor-card{padding:9px 11px;margin-bottom:0;}
      .brew-info-grid{gap:6px;}
      .brew-info-cell{padding:7px 8px;}
      .brew-info-value{font-size:13px;margin-top:3px;}
      .brew-info-sub{font-size:9px;}
      .brew-sidebar .big-metric-card{padding:11px!important;}
      .brew-sidebar .big-metric-val{font-size:30px;}
      .brew-sidebar .card{padding:12px!important;gap:9px!important;margin-bottom:5px!important;}
      .brew-sidebar .btn-stop-huge{min-height:68px!important;font-size:18px!important;}
      #dash_chart{height:clamp(300px, 58vh, 430px)!important; min-height:0!important;}
    }
    @media (max-width:900px){
      .layout-idle { grid-template-columns: 1fr; overflow:auto; }
      .layout-brew { grid-template-columns: 1fr; height:auto; min-height:0; }
      .brew-sidebar { order:1; }
      .chart-card { order:2; }
      .chart-header { flex-wrap:wrap; align-items:flex-start!important; gap:8px; }
      .legend { justify-content:flex-start!important; }
      #chart_container{min-height:0;}
      #dash_chart{height:clamp(320px, 52svh, 430px)!important; min-height:0!important;}
      html,body{overflow:auto;position:static;height:auto;min-height:100svh;}
    }
    @media (max-width:600px){
      body{padding:6px;}
      .shell{gap:8px;}
      .topbar{padding:8px 10px;}
      .top-title{font-size:13px;}
      .top-sub{display:none;}
      .tabs{overflow-x:auto; padding-bottom:2px;}
      .tab{white-space:nowrap;}
      .chart-card{padding:8px!important;}
      .chart-live-label{font-size:13px; flex-wrap:wrap;}
      #chart_profile_name{display:block; margin-left:0!important; width:100%; font-size:11px!important;}
      .legend{font-size:9px; gap:4px;}
      .legend-item{padding:2px 5px;}
      #dash_chart{height:clamp(290px, 46svh, 360px)!important; min-height:0!important;}
      .btn-stop-huge{height:54px; font-size:15px; padding:10px;}
      .brew-info-grid{grid-template-columns:1fr;}
      .brew-info-cell.wide{grid-column:auto;}
    }
    @media (max-height:820px) and (min-width:901px){
      .shell { gap:8px; }
      .layout-idle { gap:14px; grid-template-columns:minmax(0,1.36fr) minmax(340px,0.74fr); }
      .dash-left-col { gap:8px; }
      .idle-header-bar { padding:9px 16px; }
      .idle-main-display { padding:10px 20px 12px; }
      .idle-cockpit { grid-template-columns:minmax(280px,1fr) 228px; gap:28px; }
      .tele-card { padding:9px 11px; }
      .temp-gauge-wrapper { transform:scale(0.94); margin:-10px 0; }
      .last-shot-box { padding:16px 12px 12px; }
      .profile-card-shell { padding:9px; }
      .dash-profile-card { padding:13px 16px; }
    }

    .card{
      background:var(--bg-card);
      border-radius:12px;
      border:1px solid var(--border);
      padding:10px;
      box-shadow:0 10px 24px rgba(0,0,0,0.4);
    }
    .card-header{
      display:flex;
      align-items:center;
      justify-content:space-between;
      margin-bottom:4px;
    }
    .card-title{
      font-size:13px;
      font-weight:600;
      letter-spacing:.02em;
      text-transform:uppercase;
      color:var(--muted);
    }
    .pill{
      font-size:11px;
      padding:2px 8px;
      border-radius:999px;
      border:1px solid var(--border);
      color:var(--muted);
    }
    .pill.ok{border-color:rgba(102,187,106,.5);color:var(--ok);}
    .pill.bad{border-color:rgba(255,82,82,.6);color:var(--danger);}

    .temp-main{
      display:flex;
      align-items:flex-end;
      gap:6px;
      margin-top:4px;
    }
    .temp-value{
      font-size:32px;
      font-weight:600;
      letter-spacing:0.04em;
    }
    .metric-temp{
      color:var(--temp);
    }
    .temp-unit{
      font-size:14px;
      color:var(--muted);
      margin-bottom:4px;
    }
    .temp-label{
      font-size:11px;
      color:var(--muted);
      margin-top:2px;
    }

    .kv{
      display:flex;
      justify-content:space-between;
      font-size:11px;
      margin-top:4px;
      color:var(--muted);
    }
    .kv span:last-child{color:var(--text);font-weight:500;}

    /* 3 metrics ngang: Temp / Pressure / Scale */
    .metrics-row{
      display:flex;
      gap:8px;
      margin-top:4px;
      flex-wrap:nowrap;
    }
    .metric-card{
      flex:1 1 0;
      min-width:0;
      padding:6px 8px;
      border-radius:10px;
      background:var(--bg-card-soft);
      border:1px solid var(--border);
      display:flex;
      flex-direction:column;
      justify-content:flex-start;
      gap:2px;
    }
    .metric-label{
      font-size:10px;
      text-transform:uppercase;
      letter-spacing:.08em;
      color:var(--muted);
    }
    .metric-main{
      display:flex;
      align-items:flex-end;
      gap:4px;
    }
    .metric-value{
      font-size:20px;
      font-weight:600;
    }
    .metric-unit{
      font-size:11px;
      color:var(--muted);
      margin-bottom:2px;
    }
    .metric-sub{
      font-size:10px;
      color:var(--muted);
    }
    .warn-icon{
      font-size:12px;
      color:var(--danger);
      margin-left:4px;
      display:none;
    }

    .scale-value{font-size:20px;font-weight:600;}
    .scale-unit{font-size:11px;color:var(--muted);margin-bottom:3px;}
    .scale-status{font-size:10px;color:var(--muted);margin-top:2px;}

    .chart-card{padding:10px;}
    .chart-header{
      display:flex;
      align-items:center;
      justify-content:space-between;
      margin-bottom:4px;
    }
    .chart-title{
      font-size:13px;
      font-weight:600;
      letter-spacing:.02em;
      text-transform:uppercase;
      color:var(--muted);
      display:flex;
      align-items:center;
      gap:6px;
      flex-wrap:wrap;
    }
    .chart-profile-name{
      font-weight:700;
      color:var(--text);
    }

    .legend{
      display:flex;
      flex-wrap:wrap;
      gap:6px;
      justify-content:flex-end;
      font-size:10px;
    }
    .legend-item{
      display:flex;
      align-items:center;
      gap:4px;
      padding:2px 6px;
      border-radius:999px;
      background:#15171d;
      border:1px solid var(--border);
      cursor:pointer;
    }
    .legend-item.off{
      opacity:0.4;
    }
    .legend-swatch{
      width:10px;height:2px;border-radius:999px;background:var(--muted);
    }
    .legend-swatch.pressure{background:var(--pressure);}
    .legend-swatch.target{
      background:var(--pressure-target);
      border-bottom:1px dashed var(--pressure-target);
      height:0;
    }
    .legend-swatch.flow{background:var(--flow);}
    .legend-swatch.weight{background:var(--weight);}
    .legend-swatch.temp{background:var(--temp);}
    #chart_container{
      border-radius:10px;
      background:#12141a;
      border:1px solid var(--border);
      padding:6px;
      min-height:0;
      flex:1;
    }
    #dash_chart{
      width:100%;
      height:100%;
      min-height:400px;
      background:#0d0f13;
      border-radius:6px;
      display:block;
    }

    .chart-footer{
      display:flex;
      justify-content:space-between;
      align-items:center;
      margin-top:4px;
      font-size:11px;
      color:var(--muted);
    }
    .shot-state{
      text-transform:uppercase;
      letter-spacing:.08em;
      font-weight:600;
    }
    .shot-state.idle{color:var(--muted);}
    .shot-state.run{color:var(--accent2);}
    .shot-state.done{color:var(--ok);}
    .shot-state.hot{color:var(--accent-soft);}
    .shot-timing{font-variant-numeric:tabular-nums;}
    .shot-meta{font-variant-numeric:tabular-nums;}

    .offline-banner{
      position:fixed;
      left:50%;
      bottom:8px;
      transform:translateX(-50%);
      padding:4px 10px;
      border-radius:999px;
      background:#b71c1c;
      color:#fff;
      font-size:11px;
      box-shadow:0 10px 25px rgba(0,0,0,0.7);
      display:none;
    }
    .offline-banner.show{display:block;}

    /* ====== Profiles + Calibration styling (dark) ====== */
    .page-inner{
      display:flex;
      flex-direction:column;
      gap:10px;
    }
    fieldset{
      border:1px solid var(--border);
      border-radius:10px;
      padding:8px 10px 8px;
      background:var(--bg-card);
    }
    legend{
      padding:0 4px;
      font-size:12px;
      font-weight:600;
      color:var(--muted);
      text-transform:uppercase;
      letter-spacing:.03em;
    }
    label{display:inline-block;min-width:140px;font-size:12px;}
    input,button,select{
      margin:4px;
      font-size:12px;
      border-radius:6px;
      border:1px solid var(--border);
      background:#141820;
      color:var(--text);
      padding:4px 6px;
      outline: none;
    }
    select option {
      background: #1a1c22;
      color: #fff;
    }
    input[type=number]{width:110px;}
    button{
      padding:6px 10px;
      border-radius:999px;
      background:var(--accent-soft);
      border:none;
      color:#111;
      cursor:pointer;
    }
    button.red{background:#c62828;color:#fff;}
    button.active-red{background:var(--danger)!important;color:#fff;box-shadow:0 0 20px rgba(255,82,82,0.5);}
    button:disabled{opacity:.4;cursor:default;}
    .small{font-size:11px;color:var(--muted);}
    pre{
      background:#050607;
      color:#e0e0e0;
      padding:6px;
      border-radius:6px;
      font-size:11px;
      white-space:pre-wrap;
      border:1px solid var(--border);
    }
    .row{
      display:flex;
      gap:12px;
      flex-wrap:wrap;
      align-items:center;
    }
    .row>div{min-width:180px;}
    table{
      border-collapse:collapse;
      width:auto;
      font-size:12px;
      margin-top:4px;
    }
    th,td{
      border:1px solid var(--border);
      padding:4px 6px;
      text-align:left;
    }
    th{background:#181b22;color:var(--muted);}
    .ok{color:var(--ok);}
    .err{color:var(--danger);}

    #pf_canvas{
      background:#0d0f13;
      border:1px solid var(--border);
      border-radius:8px;
      display:block;
      width:100%;
      height:400px;
    }
    #dash_profile_list::-webkit-scrollbar { display: none; }
    #dash_profile_list { scrollbar-width: none; touch-action: pan-y; overflow-x: hidden; }
    
    input[type=range].thick-sl {
      -webkit-appearance:none; height:12px; border-radius:6px; background:#2a2d35; border:none; margin:10px 0;
    }
    input[type=range].thick-sl::-webkit-slider-thumb {
      -webkit-appearance:none; width:28px; height:28px; border-radius:50%; background:var(--accent); cursor:pointer; border:2px solid #111; box-shadow:0 0 5px rgba(0,0,0,0.5);
    }

    /* Manual Overlay Styles */
    .manual-overlay {
      position: absolute;
      top: 70px; right: 20px;
      z-index: 100;
      background: rgba(15, 18, 25, 0.85);
      border: 1px solid rgba(255, 255, 255, 0.1);
      border-radius: 16px;
      padding: 20px 15px;
      display: flex;
      flex-direction: column;
      align-items: center;
      gap: 15px;
      backdrop-filter: blur(15px);
      box-shadow: 0 15px 35px rgba(0,0,0,0.6), inset 0 1px 0 rgba(255,255,255,0.05);
      cursor: grab;
      user-select: none;
    }
    .manual-overlay:active { cursor: grabbing; }
    .drag-handle {
      color: #555;
      font-size: 20px;
      line-height: 0.5;
      padding-bottom: 5px;
      border-bottom: 1px solid rgba(255,255,255,0.05);
      width: 100%;
      text-align: center;
    }
    .manual-val {
      font-size: 22px;
      font-weight: 900;
      color: var(--accent);
      font-variant-numeric: tabular-nums;
    }
    .vertical-slider {
      -webkit-appearance: slider-vertical;
      appearance: slider-vertical;
      width: 25px;
      height: 220px;
      background: rgba(0,0,0,0.5);
      border-radius: 12px;
      outline: none;
      writing-mode: vertical-lr;
      direction: rtl;
      cursor: pointer;
    }
    .vertical-slider::-webkit-slider-thumb {
      -webkit-appearance: none; appearance: none;
      width: 40px; height: 40px;
      border-radius: 50%;
      background: var(--accent);
      cursor: pointer;
      border: 4px solid #111;
      box-shadow: 0 0 15px rgba(0,229,255,0.6);
    }

    /* ===== RESPONSIVE PRODUCT SHELL ===== */
    button,input,select,textarea{font:inherit;}
    button,.tab,.dash-profile-card{touch-action:manipulation;}
    button:focus-visible,.tab:focus-visible,input:focus-visible,select:focus-visible,.dash-profile-card:focus-visible{
      outline:2px solid var(--accent);outline-offset:2px;
    }
    .card,.idle-center-card,.step-monitor-card,.chart-card,.dash-profile-card{
      box-shadow:none;
    }
    .card{background:var(--bg-card);border-color:var(--border);}
    .tabs{flex:none;border-bottom:1px solid var(--border);gap:4px;margin:0;padding:0 2px;}
    .tab{
      min-height:var(--touch);display:flex;align-items:center;justify-content:center;
      border:0;border-bottom:2px solid transparent;border-radius:0;background:transparent;
      padding:8px 14px;font-weight:700;letter-spacing:.03em;
    }
    .tab.active{background:rgba(53,201,242,.08);border-color:var(--accent);color:var(--accent);}
    .tab:hover{background:rgba(255,255,255,.035);text-decoration:none;}
    .idle-center-card{background:var(--bg-card);}
    .idle-main-display{padding:14px 18px 16px;}
    .tele-card{background:var(--bg-card-soft);border-color:var(--border);border-left-width:2px;box-shadow:none;}
    .tele-card.scale{border-left-color:var(--accent);}
    .scale .tele-icon{color:var(--accent);background:rgba(53,201,242,.05);}
    .last-shot-box{background:var(--bg-card-soft);border-color:var(--border);}
    .dash-profile-card{background:var(--bg-card-soft);border-color:var(--border);border-left-width:3px;opacity:.84;transform:none;}
    .dash-profile-card:hover{border-color:var(--border-strong);opacity:1;}
    .dash-profile-card.selected{background:#111820;border-color:rgba(53,201,242,.58);border-left-color:var(--accent);box-shadow:none;transform:none;}
    .selected .profile-name,.dash-profile-card.selected .profile-mode{color:var(--accent);}
    .dash-start-btn{background:var(--ok);}
    .dash-flush-btn{border-color:var(--accent);color:var(--accent);}
    .dash-action-btn:disabled,.btn-stop-huge:disabled{opacity:.48;cursor:wait;}
    .command-feedback{font-size:10px;color:var(--muted);min-height:14px;text-align:center;}

    @media (min-width:1101px){
      .shell{max-width:1440px;}
      .layout-idle{grid-template-columns:minmax(0,7fr) minmax(350px,5fr);gap:14px;}
    }

    @media (max-width:1100px){
      body{padding:8px;}
      .shell{gap:8px;}
      .topbar{padding:8px 10px;}
      .layout-idle{grid-template-columns:minmax(0,1.25fr) minmax(300px,.75fr);gap:10px;}
      .idle-cockpit{grid-template-columns:minmax(260px,1fr) 210px;gap:18px;}
      .temp-gauge-wrapper{transform:scale(.9);margin:-12px auto;}
      .profile-name{font-size:15px;}
      .dash-profile-card{padding:12px 14px;}
      .layout-brew{grid-template-columns:270px minmax(0,1fr);gap:10px;}
      .chart-card{padding:12px;}
      .hist-page{grid-template-columns:240px minmax(0,1fr);gap:8px;}
    }

    @media (min-width:761px) and (max-width:900px){
      html{overflow-x:hidden;}
      body{width:100%;overflow-x:hidden;}
      .layout-idle{grid-template-columns:1fr;height:auto;}
      .profile-col{width:100%;min-height:420px;}
      .profile-card-shell{width:100%;}
      #dash_profile_list{max-height:520px;}
      .idle-cockpit{grid-template-columns:minmax(330px,1fr) 260px;}
      .layout-brew{grid-template-columns:1fr;height:auto;}
      .brew-sidebar{display:grid;grid-template-columns:1fr 1fr;align-items:stretch;}
      #brew_sidebar_live,#brew_sidebar_summary{grid-column:1 / -1;}
      .chart-card{min-height:480px;}
      .hist-page{grid-template-columns:220px minmax(0,1fr);}
    }

    @media (max-width:760px){
      html{height:auto;min-height:100%;overflow:auto;}
      body{
        position:static;inset:auto;width:100%;height:auto;min-height:100dvh;
        overflow-x:hidden;overflow-y:auto;padding:0 0 max(12px,env(safe-area-inset-bottom));
      }
      .shell{width:100%;height:auto;min-height:100dvh;gap:0;}
      .topbar{
        position:sticky;top:0;z-index:100;border:0;border-bottom:1px solid var(--border);
        border-radius:0;background:rgba(9,11,14,.96);backdrop-filter:blur(14px);
        padding:max(10px,env(safe-area-inset-top)) 12px 10px;
      }
      .logo{width:32px;height:32px;font-size:15px;box-shadow:none;}
      .top-title{font-size:14px;letter-spacing:.06em;}
      .top-sub{display:none;}
      .topbar-right{gap:5px;}
      .connection-pill{display:none;}
      .connection-pill:first-child{display:flex;max-width:145px;min-height:32px;padding:4px 8px;}
      .connection-pill:first-child strong{display:none;}
      #main_connection,#scale_connection{display:flex;width:30px;min-width:30px;padding:0;justify-content:center;border-radius:8px;}
      #main_connection>div:last-child,#scale_connection>div:last-child{display:none;}
      .tabs{
        position:sticky;top:53px;z-index:90;overflow-x:auto;overscroll-behavior-x:contain;
        scrollbar-width:none;background:rgba(9,11,14,.97);padding:0 6px;
      }
      .tabs::-webkit-scrollbar{display:none;}
      .tab{flex:0 0 auto;min-width:92px;min-height:48px;padding:8px 12px;font-size:11px;}
      .page{padding:10px 10px 0;}
      #page-dash{min-height:auto;}
      .dash-view.active{height:auto;}
      .layout-idle,.layout-brew{display:flex;flex-direction:column;height:auto;overflow:visible;gap:10px;}
      .dash-left-col{display:contents;}
      .idle-center-card{order:1;overflow:visible;}
      .action-row{order:2;min-height:68px;position:relative;z-index:2;gap:8px;}
      .profile-col{order:3;min-height:0;}
      .profile-card-shell{padding:10px;}
      .idle-header-bar{padding:9px 11px;}
      .idle-header-bar>div:first-child{gap:8px!important;}
      .idle-main-display{padding:10px;gap:10px;}
      .idle-cockpit{grid-template-columns:1fr;gap:6px;}
      .temp-gauge-wrapper{width:270px;height:194px;transform:scale(.82);margin:-18px auto -22px;}
      .big-temp{font-size:58px;}
      .idle-telemetry{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:7px;}
      .tele-card.boiler{display:none;}
      .tele-card{padding:11px 10px;min-width:0;}
      .tele-main{gap:8px;}
      .tele-icon{width:34px;height:34px;}
      .tele-icon svg{width:19px;height:19px;}
      .tele-value{font-size:15px;}
      .tele-hint{white-space:normal;line-height:1.25;}
      .last-shot-box{grid-template-columns:1fr 1px 1fr 1px 1fr;margin-top:8px;padding-top:16px;}
      .last-shot-box .last-shot-col:nth-last-child(1),.last-shot-box .last-shot-divider:nth-last-child(2){display:none;}
      .last-shot-val{font-size:15px;}
      .dash-action-btn{min-height:68px;gap:4px;}
      .dash-flush-btn{font-size:12px;}
      .dash-start-btn{font-size:16px;}
      .dash-action-btn svg{width:25px!important;height:25px!important;}
      .action-sub{max-width:170px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;}
      .profile-list-head{align-items:center;}
      #dash_profile_list{overflow:visible!important;max-height:none!important;gap:7px!important;}
      .dash-profile-card{padding:13px 12px;min-height:64px;}
      .profile-preview,.profile-phases{display:none;}
      .profile-meta,.profile-mode{margin-top:5px;}
      .layout-brew .brew-sidebar{order:1;}
      .layout-brew .chart-card{order:2;min-height:390px;}
      #brew_sidebar_live{gap:8px!important;}
      .brew-primary-metrics{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));padding:0;}
      .brew-metric{display:block;text-align:center;padding:12px 6px;border-bottom:0;border-right:1px solid rgba(255,255,255,.06);}
      .brew-metric:last-child{border-right:0;}
      .brew-metric-value{justify-content:center;margin-top:4px;}
      .brew-metric-number{font-size:25px;}
      #brew_time{font-size:28px!important;}
      .brew-sidebar .card{margin:0!important;}
      .chart-header{margin-bottom:8px;}
      #dash_chart{height:300px!important;}
      .brew-stop-desktop{display:none;}
      .brew-stop-mobile{display:block;order:3;position:sticky;bottom:max(8px,env(safe-area-inset-bottom));z-index:80;min-height:60px!important;margin:0 4px;background:rgba(239,83,80,.96);color:#fff;border-color:rgba(255,255,255,.18);box-shadow:0 10px 30px rgba(0,0,0,.42);}
      .page-inner{padding:0;}
      .row{flex-direction:column;gap:10px;}
      .row>div{width:100%;min-width:0;}
      .hist-page{display:block;}
      .hist-side{max-height:none;margin-bottom:10px;}
      .hist-list{max-height:270px;}
      .hist-detail{min-height:430px;}
      #hist_chart{height:260px;}
      .hist-download{float:none;display:inline-block;margin:8px 0 0;}
      #page-sys .page-inner{max-width:none!important;}
      .offline-banner{left:10px;right:10px;bottom:10px;width:auto;text-align:center;}
      .manual-overlay{display:none!important;}
      .help-dot::after{position:fixed;left:12px;right:12px;top:auto;bottom:16px;min-width:0;max-width:none;}
    }

    @media (max-width:390px){
      .top-title{font-size:12px;}
      .connection-pill:first-child{max-width:122px;}
      .tab{min-width:84px;}
      .page{padding-left:7px;padding-right:7px;}
      .temp-gauge-wrapper{transform:scale(.76);margin:-24px auto -30px;}
      .action-row{gap:6px;}
      .dash-start-btn{font-size:14px;}
      .last-shot-label{font-size:8px;}
      .brew-metric-number{font-size:22px;}
      .brew-metric-unit{font-size:10px;}
      #brew_link_age{font-size:9px;}
    }

    /* Profiles responsive workspace: list -> timeline -> step inspector. */
    #page-prof{min-height:0;}
    .profiles-workspace{display:grid;grid-template-columns:280px minmax(0,1fr);gap:10px;height:100%;min-height:0;}
    .pf-library-pane,.pf-editor-workspace{background:#0e1116;border:1px solid var(--border);border-radius:10px;min-width:0;overflow:hidden;}
    .pf-library-pane{display:flex;flex-direction:column;min-height:0;}
    .pf-pane-head,.pf-editor-head{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:14px 15px;border-bottom:1px solid var(--border);}
    .pf-pane-head h2{font-size:17px;margin:0;}
    .pf-pane-head span{display:block;margin-top:3px;font-size:9px;color:var(--muted);}
    .pf-new-btn,.pf-import-btn,.pf-add-step{border:1px solid var(--accent)!important;background:transparent!important;color:var(--accent)!important;border-radius:6px!important;min-height:38px;font-weight:700;}
    .pf-import-btn{margin:10px!important;border-color:var(--border)!important;color:var(--muted)!important;}
    .pf-library-list{display:flex;flex-direction:column;min-height:0;overflow:auto;}
    .pf-library-row{display:flex;align-items:center;gap:10px;min-height:72px;padding:12px 13px;border-bottom:1px solid var(--border);border-left:3px solid transparent;cursor:pointer;transition:.18s ease;}
    .pf-library-row:hover{background:#131820;}
    .pf-library-row.selected{background:#101b23;border-left-color:var(--accent);}
    .pf-library-main{min-width:0;flex:1;}
    .pf-library-name{font-size:14px;font-weight:800;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
    .pf-library-meta{margin-top:5px;color:var(--muted);font-size:10px;}
    .pf-library-chevron{color:var(--muted);font-size:22px;}
    .pf-library-actions{display:flex;gap:4px;}
    .pf-library-actions button{margin:0!important;padding:5px 7px!important;background:transparent!important;color:var(--muted)!important;border:1px solid transparent!important;border-radius:5px!important;font-size:9px!important;}

    .pf-editor-workspace{display:flex;flex-direction:column;height:100%;min-height:0;}
    .pf-editor-head{min-height:58px;}
    .pf-back-btn{display:none!important;background:transparent!important;color:var(--text)!important;font-size:28px!important;padding:0!important;margin:0!important;min-width:36px;}
    .pf-editor-title{display:flex;flex-direction:column;min-width:0;}
    .pf-editor-title strong{font-size:17px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
    .pf-editor-title span{font-size:10px;color:var(--muted);margin-top:3px;}
    .pf-editor-title span.dirty{color:#ffb74d;}
    .pf-type-switch{display:flex;gap:0!important;margin-left:auto;border:1px solid var(--border);border-radius:7px;overflow:hidden;}
    .pf-type-switch button{margin:0!important;border-radius:0!important;background:transparent!important;color:var(--muted)!important;min-height:34px;padding:5px 12px!important;}
    .pf-type-switch button.active{background:var(--accent)!important;color:#001014!important;}
    .pf-breadcrumb{padding:7px 15px;border-bottom:1px solid rgba(255,255,255,.04);font-size:9px;color:var(--muted);}
    .pf-editor-grid{display:grid;grid-template-columns:minmax(360px,1fr) 370px;gap:0;flex:1;min-height:0;overflow:hidden;}
    .pf-overview-pane{padding:14px;overflow:auto;min-width:0;border-right:1px solid var(--border);}
    .pf-global-fields{display:grid;grid-template-columns:minmax(150px,1fr) 125px 115px;gap:9px;}
    .pf-global-fields label,.pf-note-field{position:relative;display:block;min-width:0;color:var(--muted);font-size:9px;text-transform:uppercase;letter-spacing:.04em;}
    .pf-global-fields input,.pf-note-field input{box-sizing:border-box;width:100%!important;min-height:38px;margin:5px 0 0!important;padding:8px 30px 8px 9px!important;border-radius:6px!important;font-size:12px!important;text-transform:none;}
    .pf-global-fields label>span{position:absolute;right:10px;bottom:11px;color:var(--muted);font-size:10px;text-transform:none;}
    .pf-note-field{margin-top:9px;}
    .pf-preview-shell{margin-top:12px;padding:11px;background:#0b0e12;border:1px solid var(--border);border-radius:8px;}
    .pf-preview-shell>div{display:flex;justify-content:space-between;gap:10px;margin-bottom:7px;font-size:10px;color:var(--muted);}
    .pf-preview-shell strong{color:var(--text);font-size:11px;}
    #pf_canvas{height:250px;background:#090c10;border:0;border-radius:4px;}
    .pf-section-head{display:flex;align-items:center;justify-content:space-between;margin:15px 0 7px;padding-bottom:7px;border-bottom:1px solid var(--border);font-size:11px;text-transform:uppercase;letter-spacing:.06em;}
    .pf-step-timeline{display:flex;flex-direction:column;min-width:0;}
    .pf-section-head span{font-size:9px;color:var(--muted);}
    .pf-step-list{display:flex;flex-direction:column;}
    .pf-step-row{position:relative;display:grid;grid-template-columns:32px minmax(0,1fr) 22px;align-items:center;gap:9px;min-height:70px;padding:9px 5px;border-bottom:1px solid var(--border);cursor:pointer;}
    .pf-step-row::before{content:"";position:absolute;left:20px;top:-1px;bottom:-1px;width:1px;background:#244653;}
    .pf-step-row:first-child::before{top:50%;}.pf-step-row:last-child::before{bottom:50%;}
    .pf-step-row.selected{background:#101b23;box-shadow:inset 3px 0 var(--accent);}
    .pf-step-index{position:relative;z-index:1;width:27px;height:27px;display:grid;place-items:center;border:1px solid #7b8791;border-radius:50%;background:#0e1116;font-size:11px;font-weight:800;}
    .pf-step-row.selected .pf-step-index{border-color:var(--accent);background:var(--accent);color:#001014;}
    .pf-step-name{color:var(--text);font-size:12px;font-weight:800;}
    .pf-step-row.selected .pf-step-name{color:var(--accent);}
    .pf-step-control,.pf-step-rules{margin-top:3px;color:var(--muted);font-size:9px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
    .pf-step-chevron{color:var(--muted);font-size:20px;}
    .pf-add-step{width:100%;margin:10px 0 0!important;}

    .pf-step-inspector{min-width:0;overflow:auto;background:#0b0e12;}
    .pf-inspector-empty{display:grid;place-items:center;min-height:100%;padding:30px;text-align:center;color:var(--muted);font-size:11px;line-height:1.5;}
    .pf-inspector-head{position:sticky;top:0;z-index:4;background:#0b0e12;border-bottom:1px solid var(--border);}
    .pf-inspector-titlebar{display:flex;align-items:center;gap:9px;padding:14px;}
    .pf-inspector-titlebar strong{flex:1;font-size:14px;text-transform:uppercase;letter-spacing:.04em;}
    .pf-inspector-done{display:none!important;background:transparent!important;color:var(--accent)!important;}
    .pf-inspector-tabs{display:grid;grid-template-columns:1fr 1fr;padding:0 12px;}
    .pf-inspector-tabs button{margin:0!important;border-radius:0!important;background:transparent!important;color:var(--muted)!important;border-bottom:2px solid transparent!important;min-height:42px;}
    .pf-inspector-tabs button.active{color:var(--accent)!important;border-bottom-color:var(--accent)!important;}
    .pf-inspector-body{padding:14px;}
    .pf-control-grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;}
    .pf-control-grid label,.pf-safety-panel label{display:block;min-width:0;color:var(--muted);font-size:9px;text-transform:uppercase;}
    .pf-control-grid label:first-child{grid-column:1 / -1;}
    .pf-control-grid input,.pf-control-grid select,.pf-rule-row input,.pf-rule-row select,.pf-safety-panel input,.pf-safety-panel select{box-sizing:border-box;width:100%!important;min-height:42px;margin:5px 0 0!important;padding:8px!important;border-radius:6px!important;font-size:12px!important;}
    .pf-mode-segment{grid-column:1 / -1;display:grid;grid-template-columns:repeat(3,1fr);border:1px solid var(--border);border-radius:7px;overflow:hidden;}
    .pf-mode-segment button{margin:0!important;border-radius:0!important;min-height:43px;background:transparent!important;color:var(--muted)!important;border-right:1px solid var(--border)!important;}
    .pf-mode-segment button:last-child{border-right:0!important;}.pf-mode-segment button.active{background:#10242d!important;color:var(--accent)!important;}
    .pf-safety-panel,.pf-exit-panel{padding:13px;border-radius:8px;}
    .pf-safety-panel{border:1px solid rgba(255,183,77,.55);background:rgba(255,183,77,.035);}
    .pf-exit-panel{margin-top:12px;border:1px solid rgba(53,201,242,.55);background:rgba(53,201,242,.025);}
    .pf-panel-title{font-size:11px;font-weight:900;text-transform:uppercase;letter-spacing:.04em;}
    .pf-safety-panel .pf-panel-title{color:#ffb74d;}.pf-exit-panel .pf-panel-title{color:var(--accent);}
    .pf-panel-help{margin:5px 0 11px;color:var(--muted);font-size:9px;line-height:1.4;}
    .pf-safety-fields{display:grid;grid-template-columns:1fr 100px;gap:8px;}
    .pf-rule-row{display:grid;grid-template-columns:22px minmax(92px,1fr) 48px 68px 26px 30px;gap:5px;align-items:end;margin-top:7px;}
    .pf-rule-drag{align-self:center;color:var(--muted);font-size:15px;}.pf-rule-unit{align-self:center;color:var(--muted);font-size:9px;pointer-events:none;text-align:left;}
    .pf-rule-remove{align-self:center!important;margin:5px 0 0!important;padding:0!important;min-height:40px;background:transparent!important;color:var(--danger)!important;font-size:17px!important;}
    .pf-add-rule{width:100%;min-height:42px;margin:10px 0 0!important;background:transparent!important;border:1px solid var(--accent)!important;color:var(--accent)!important;border-radius:6px!important;}
    .pf-step-danger{margin-top:16px;text-align:center;}.pf-step-danger button{background:transparent!important;color:var(--danger)!important;border:1px solid rgba(239,83,80,.25)!important;}
    .pf-basic-panel{display:flex;flex-direction:column;gap:13px;margin-top:14px;padding:13px;border:1px solid var(--border);border-radius:8px;background:#101319;}
    .pf-basic-panel>label{display:grid;grid-template-columns:1fr auto;min-width:0;color:var(--muted);font-size:10px;}.pf-basic-panel>label b{color:var(--accent);}.pf-basic-panel input[type=range]{grid-column:1 / -1;width:100%;margin:8px 0!important;}
    .pf-basic-safety{display:flex;align-items:center;justify-content:space-between;padding:10px;border:1px solid rgba(255,183,77,.25);border-radius:7px;}.pf-basic-safety label{min-width:0;}.pf-basic-safety input{width:auto!important;}.pf-basic-summary{padding:10px;border:1px dashed var(--border);font-size:10px;color:var(--muted);line-height:1.5;}
    .pf-editor-actions{display:grid;grid-template-columns:auto 1fr auto minmax(160px,260px);align-items:center;gap:8px;padding:10px 13px;border-top:1px solid var(--border);background:#0e1116;}
    .pf-editor-actions button{min-height:44px;margin:0!important;border-radius:6px!important;font-weight:800;}
    .pf-delete-profile{background:transparent!important;color:var(--danger)!important;border:1px solid rgba(239,83,80,.28)!important;}
    .pf-cancel-btn{background:transparent!important;color:var(--text)!important;border:1px solid var(--border)!important;}.pf-save-btn{background:var(--ok)!important;color:#031004!important;}
    #pf_msg{font-size:10px;color:var(--ok);}

    @media (max-width:1100px){
      .profiles-workspace{grid-template-columns:240px minmax(0,1fr);}
      .pf-editor-grid{grid-template-columns:minmax(330px,1fr) 330px;}
      .pf-global-fields{grid-template-columns:1fr 105px 100px;}
    }
    @media (max-width:760px){
      #page-prof{padding-bottom:0!important;}
      .profiles-workspace{display:block;height:auto;}
      .pf-library-pane,.pf-editor-workspace{border-radius:0;border-left:0;border-right:0;min-height:calc(100dvh - 101px);overflow:visible;}
      .profiles-workspace.editor-open .pf-library-pane{display:none;}
      .pf-library-list{overflow:visible;}.pf-library-row{min-height:76px;}
      .pf-library-actions{display:none;}
      .pf-import-btn{margin-top:18px!important;}
      .pf-back-btn{display:block!important;}
      .pf-editor-head{position:sticky;top:101px;z-index:55;background:rgba(9,11,14,.97);padding:9px 10px;}
      .pf-type-switch{display:none!important;}
      .pf-breadcrumb{display:none;}
      .pf-editor-grid{display:block;overflow:visible;}
      .pf-overview-pane{padding:11px;border:0;overflow:visible;}
      .pf-overview-pane{padding-bottom:140px;}
      .pf-global-fields{grid-template-columns:1fr 1fr;}.pf-global-fields label:first-child{grid-column:1 / -1;}
      .pf-note-field{display:none;}
      #pf_canvas{height:230px;}
      .pf-step-row{min-height:78px;padding-right:8px;}
      .pf-step-inspector{display:none;position:fixed;inset:0;z-index:300;background:#090b0e;overflow:auto;padding-bottom:calc(18px + env(safe-area-inset-bottom));}
      .pf-step-inspector.mobile-open{display:block;}
      .pf-inspector-head{top:0;padding-top:env(safe-area-inset-top);}
      .pf-inspector-titlebar{min-height:50px;}.pf-inspector-done{display:block!important;}
      .pf-inspector-body{padding:12px;}
      .pf-control-grid{grid-template-columns:1fr;}.pf-control-grid label:first-child{grid-column:auto;}
      .pf-mode-segment{grid-column:auto;}
      .pf-control-grid input,.pf-control-grid select,.pf-rule-row input,.pf-rule-row select,.pf-safety-panel input,.pf-safety-panel select{min-height:48px;font-size:14px!important;}
      .pf-safety-fields{grid-template-columns:1fr 110px;}
      .pf-rule-row{grid-template-columns:18px minmax(92px,1fr) 48px 68px 25px 30px;}
      .pf-editor-actions{position:sticky;bottom:0;z-index:60;grid-template-columns:1fr 1.45fr;padding:9px 10px max(9px,env(safe-area-inset-bottom));box-shadow:0 -8px 24px rgba(0,0,0,.45);}
      .pf-delete-profile,#pf_msg{grid-column:1 / -1;}.pf-delete-profile{order:0;min-height:34px!important;}.pf-cancel-btn{order:2}.pf-save-btn{order:3}
    }

    /* Mobile Safari needs an explicit document scroller and vertical gesture. */
    @media (max-width:900px){
      html{
        height:auto!important;
        min-height:100%!important;
        overflow-x:hidden!important;
        overflow-y:auto!important;
        overscroll-behavior-y:auto;
        touch-action:pan-y pinch-zoom;
        -webkit-overflow-scrolling:touch;
      }
      body{
        position:relative!important;
        inset:auto!important;
        width:100%!important;
        height:auto!important;
        min-height:100dvh!important;
        overflow:visible!important;
        overscroll-behavior-y:auto;
        touch-action:pan-y pinch-zoom;
      }
      .shell,.page.active,.dash-view.active,.layout-idle,.layout-brew{
        height:auto!important;
        overflow:visible!important;
      }
      .tabs{touch-action:pan-x pinch-zoom;}
      canvas{touch-action:pan-y pinch-zoom;}
    }

    @media (prefers-reduced-motion:reduce){
      *,*::before,*::after{animation-duration:.01ms!important;animation-iteration-count:1!important;scroll-behavior:auto!important;transition-duration:.01ms!important;}
    }
    /* ===== HUSTERY BREWVECTOR STANDBY DASHBOARD ===== */
    .layout-idle{
      grid-template-columns:minmax(0,7fr) minmax(350px,5fr);
      gap:14px;align-items:start;height:auto;min-height:0;
    }
    .dash-left-col{display:block;min-width:0;}
    .idle-center-card,.profile-card-shell{
      height:clamp(560px,calc(100svh - 150px),760px);
      min-height:560px;flex:none;
    }
    .idle-center-card{overflow:hidden;}
    .idle-header-bar{padding:13px 18px;}
    .idle-main-display{
      justify-content:flex-start;padding:24px 22px 20px;gap:24px;
    }
    .standby-temperature{padding:8px 12px 0;text-align:center;}
    .standby-temperature .big-temp{
      position:static;transform:none;display:inline-block;
      font-size:70px;line-height:1;letter-spacing:-.045em;
    }
    .standby-temperature .big-temp-unit{font-size:.34em;margin-left:5px;vertical-align:top;line-height:1.8;}
    .temp-progress{
      height:9px;margin:30px 0 10px;overflow:hidden;
      border-radius:999px;background:#29313a;
    }
    .temp-progress-fill{
      width:0;height:100%;border-radius:inherit;background:#ff6d3a;
      transition:width .5s ease,background .3s ease;
    }
    .temp-progress-meta{display:flex;justify-content:space-between;color:var(--muted);font-size:12px;}
    .temp-progress-meta span:first-child{color:var(--text);font-weight:800;}
    .standby-facts{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px;}
    .standby-fact{
      display:flex;align-items:center;gap:13px;min-width:0;padding:15px;
      text-align:left;border:1px solid var(--border);border-radius:10px;background:var(--bg-card-soft);
    }
    .standby-fact-icon{
      flex:0 0 45px;width:45px;height:45px;display:grid;place-items:center;
      color:var(--accent);border:1px solid currentColor;border-radius:50%;background:rgba(53,201,242,.045);
    }
    .standby-fact.block .standby-fact-icon{color:#ff7a2f;background:rgba(255,122,47,.05);}
    .standby-fact.yield .standby-fact-icon{color:var(--ok);background:rgba(102,187,106,.05);}
    .standby-fact-icon svg{width:23px;height:23px;fill:none;stroke:currentColor;stroke-width:1.8;stroke-linecap:round;stroke-linejoin:round;}
    .standby-fact-label{color:var(--muted);font-size:9px;font-weight:800;letter-spacing:.09em;text-transform:uppercase;}
    .standby-fact-value{margin-top:4px;color:var(--text);font-size:24px;font-weight:900;line-height:1.05;white-space:nowrap;}
    .standby-fact-value small{margin-left:2px;color:var(--muted);font-size:13px;font-weight:700;}
    .standby-fact-hint{margin-top:4px;color:var(--muted);font-size:10px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;}
    .action-row{margin-top:auto;min-height:80px;gap:12px;}
    .dash-action-btn{border-radius:10px;box-shadow:none!important;}
    .dash-flush-btn{flex:0 0 150px;border-color:var(--border);color:var(--text);}
    .dash-start-btn{flex:1;background:#ffbd24;color:#101216;border:1px solid #ffc83f;font-size:17px;}
    .dash-action-btn svg{width:27px;height:27px;}
    #dash_profile_list{
      display:grid!important;grid-template-columns:repeat(2,minmax(0,1fr));
      gap:10px!important;align-content:start;overflow-y:auto!important;
    }
    .dash-profile-card{grid-column:1/-1;}
    .dash-profile-card.utility{grid-column:auto;min-height:94px;}

    @media (max-width:900px){
      .idle-center-card,.profile-card-shell{height:auto;min-height:0;}
      .layout-idle{grid-template-columns:1fr;}
      .profile-card-shell{max-height:none;}
    }
    @media (max-width:760px){
      .idle-main-display{padding:18px 12px 14px;gap:17px;}
      .standby-temperature{padding:6px 5px 0;}
      .standby-temperature .big-temp{font-size:56px;}
      .temp-progress{margin-top:22px;}
      .standby-facts{gap:7px;}
      .standby-fact{display:block;padding:11px 8px;text-align:center;}
      .standby-fact-icon{width:36px;height:36px;display:grid;margin:0 auto 8px;}
      .standby-fact-icon svg{width:19px;height:19px;}
      .standby-fact-value{font-size:18px;}
      .action-row{display:grid;grid-template-columns:92px minmax(0,1fr);min-height:68px;}
      .dash-flush-btn{width:auto;min-width:0;}
      #dash_profile_list{grid-template-columns:repeat(2,minmax(0,1fr));}
      .dash-profile-card.utility{min-height:82px;}
    }
    @media (max-width:520px){
      .standby-facts{grid-template-columns:1fr;}
      .standby-fact{display:flex;text-align:left;}
      .standby-fact-icon{margin:0;}
    }
  </style>
</head>
<body>
<div class="shell">

  <!-- ===== TOP BAR ===== -->
  <header class="topbar">
    <div class="topbar-left">
      <div class="logo">H</div>
      <div>
        <div class="top-title">HUSTERY BREWVECTOR</div>
        <div class="top-sub">Crafted with Passion</div>
      </div>
    </div>
    <div class="topbar-right" aria-label="Connection status">
      <div id="web_connection" class="connection-pill">
        <div id="wifi_dot" class="dot"></div>
        <div><strong>Web / HMI</strong><span id="wifi_text">Connecting</span></div>
      </div>
      <div id="main_connection" class="connection-pill bad" title="Main controller connection">
        <div id="main_dot" class="dot"></div>
        <div><strong>Main controller</strong><span id="main_text">Offline</span></div>
      </div>
      <div id="scale_connection" class="connection-pill bad" title="Scale connection">
        <div id="scale_dot" class="dot"></div>
        <div><strong>Scale</strong><span id="scale_text">Not connected</span></div>
      </div>
    </div>
  </header>

  <!-- ===== TABS ===== -->
  <nav class="tabs" role="tablist" aria-label="Main navigation">
    <button class="tab active" type="button" role="tab" aria-selected="true" data-tab="dash">Dashboard</button>
    <button class="tab" type="button" role="tab" aria-selected="false" data-tab="prof">Profiles</button>
    <button class="tab" type="button" role="tab" aria-selected="false" data-tab="hist">History</button>
    <button class="tab" type="button" role="tab" aria-selected="false" data-tab="cal">Calibration</button>
    <button class="tab" type="button" role="tab" aria-selected="false" data-tab="heat">PID nhi&#7879;t</button>
    <button class="tab" type="button" role="tab" aria-selected="false" data-tab="conn">Connections</button>
    <button class="tab" type="button" role="tab" aria-selected="false" data-tab="sys">System</button>
  </nav>

  <!-- ===== PAGE: DASHBOARD ===== -->
  <section id="page-dash" class="page active">
    
    <!-- VIEW 1: IDLE (Chun b) -->
    <div id="dash_view_idle" class="dash-view active">
      <div class="layout-idle">
        
        <!-- Left Col: Status & Controls -->
        <div class="dash-left-col">
          <section class="card idle-center-card" style="flex:1;">
            <div class="idle-header-bar">
              <div style="display:flex; align-items:center; gap:12px;">
                <!-- Boiler: icon ngn la/nhit -->
                <div class="header-item">
                  <svg class="header-icon" viewBox="0 0 24 24"><path d="M13.5 0.67s0.74 2.65 0.74 4.8c0 2.06-1.35 3.73-3.41 3.73-2.07 0-3.63-1.67-3.63-3.73l.03-.36C5.21 7.51 4 10.62 4 14c0 4.42 3.58 8 8 8s8-3.58 8-8C20 8.61 17.41 3.8 13.5 0.67zM12 20c-3.31 0-6-2.69-6-6 0-1.53.57-2.96 1.61-4.07.7 1.06 1.87 1.8 3.24 1.8 1.38 0 2.56-.74 3.23-1.83.36.78.57 1.62.57 2.5 0 2.21-1.79 4-4 4z"/></svg>
                  <span id="boiler_state_pill" class="pill">Idle</span>
                </div>
              </div>
              <div id="dash_uptime" style="font-family:monospace; font-size:11px; opacity:0.5;">UP: 00:00:00</div>
            </div>

            <div id="idle_main_display_wrapper" class="idle-main-display">
              <div class="standby-temperature">
                <div id="boiler_temp" class="big-temp">--<span class="big-temp-unit">&deg;C</span></div>
                <div id="overheat_alert" style="display:none">OVERHEAT</div>
                <div class="temp-progress" aria-label="Brew temperature progress">
                  <div id="temp_progress_fill" class="temp-progress-fill"></div>
                </div>
                <div class="temp-progress-meta">
                  <span id="temp_progress_current">--&deg;C</span>
                  <span>Target 93&deg;C</span>
                </div>
              </div>

              <div class="standby-facts">
                <div class="standby-fact block">
                  <div class="standby-fact-icon"><svg viewBox="0 0 24 24"><path d="M14 14.8V5a4 4 0 0 0-8 0v9.8a6 6 0 1 0 8 0z"/><path d="M10 8v8"/></svg></div>
                  <div><div class="standby-fact-label">Block</div><div id="idle_block_temp" class="standby-fact-value">--&deg;C</div><div class="standby-fact-hint">Thermoblock</div></div>
                </div>
                <div class="standby-fact yield">
                  <div class="standby-fact-icon"><svg viewBox="0 0 24 24"><path d="M5 5h12v7a5 5 0 0 1-5 5H9a4 4 0 0 1-4-4V5z"/><path d="M17 7h2a2 2 0 0 1 0 4h-2"/><path d="M4 20h15"/></svg></div>
                  <div><div class="standby-fact-label">Stop Yield</div><div id="auto_yield_value" class="standby-fact-value">--<small>g</small></div><div class="standby-fact-hint">Next shot</div></div>
                </div>
                <div class="standby-fact scale">
                  <div class="standby-fact-icon"><svg viewBox="0 0 24 24"><path d="M12 4v16"/><path d="M6 7h12"/><path d="M7 7l-3 6h6L7 7z"/><path d="M17 7l-3 6h6l-3-6z"/></svg></div>
                  <div><div class="standby-fact-label">Scale</div><div class="standby-fact-value"><span id="idle_scale_weight">--</span> <small>g</small></div><div id="idle_scale_hint" class="standby-fact-hint">Not connected</div></div>
                </div>
              </div>

              <!-- Last Shot Info  3 ct + 2 divider -->
              <div id="last_shot_box" class="last-shot-box" style="display:none;">
                <div class="last-shot-title">LAST SHOT</div>
                <div class="last-shot-col">
                  <div class="last-shot-label">YIELD</div>
                  <div id="last_shot_yield_val" class="last-shot-val">-- g</div>
                </div>
                <div class="last-shot-divider"></div>
                <div class="last-shot-col">
                  <div class="last-shot-label">TIME</div>
                  <div id="last_shot_time_val" class="last-shot-val">-- s</div>
                </div>
                <div class="last-shot-divider"></div>
                <div class="last-shot-col">
                  <div class="last-shot-label">PROFILE</div>
                  <div id="last_shot_profile_val" class="last-shot-val" style="color:var(--accent2); font-size:12px; letter-spacing:-0.01em;">--</div>
                </div>
                <div class="last-shot-divider"></div>
                <div class="last-shot-col">
                  <div class="last-shot-label">PHÂN TÍCH</div>
                  <div style="display:flex; align-items:center; gap:8px; justify-content:center;">
                    <span class="analysis-icon">◷</span>
                    <span id="last_shot_note" class="last-shot-note">--</span>
                  </div>
                </div>
              </div>
              <div class="action-row">
                <button id="btn_dash_flush" onclick="dashFlush()" class="dash-action-btn dash-flush-btn">
                  <svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 21.5c-3.5 0-6.5-3-6.5-6.5 0-4 6.5-12 6.5-12s6.5 8 6.5 12c0 3.5-3 6.5-6.5 6.5z"/></svg>
                  <span id="btn_dash_flush_text">FLUSH</span>
                </button>
                <button id="btn_dash_start" onclick="dashStart()" class="dash-action-btn dash-start-btn">
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"><path d="M7 16c0-2 2-3 2-5s-2-3-2-5M12 16c0-2 2-3 2-5s-2-3-2-5M17 16c0-2 2-3 2-5s-2-3-2-5"/></svg>
                  <span id="btn_dash_start_text">PREHEAT</span>
                </button>
              </div>
            </div>
          </section>
        </div>

        <!-- Right Col: Profile List -->
        <section class="profile-col">
          <div class="card profile-card-shell">
            <div class="card-header profile-list-head" style="border-bottom: 1px solid rgba(255,255,255,0.03); padding-bottom: 10px; margin-bottom: 10px;">
              <div>
                <div class="card-title">Extract Profile</div>
                <div class="profile-list-hint">Chọn profile để arm shot tiếp theo</div>
              </div>
              <div id="dash_selected_profile_badge" class="profile-chip">--</div>
            </div>
            <div id="dash_profile_list" style="overflow-y:auto; flex:1; min-height:0; display:flex; flex-direction:column; gap:12px; scroll-behavior: smooth; padding-right:5px; padding-bottom:8px;">
              <div style="color:#666; font-size:11px; text-align:center; padding: 20px;">Loading profiles...</div>
            </div>
          </div>
        </section>

      </div>
    </div>

    <!-- VIEW 2: BREWING (Quan st) -->
    <div id="dash_view_brew" class="dash-view">
      <div class="layout-brew">
        
        <!-- Left: Live Metrics Sidebar -->
        <div class="brew-sidebar">
          
          <!-- LIVE SIDEBAR (Trong khi pha) -->
          <div id="brew_sidebar_live" style="display:flex; flex-direction:column; gap:10px; flex: 1;">
            <div id="brew_state_strip" class="brew-state-strip">
              <span class="brew-state-dot"></span>
              <span id="brew_state_label">LIVE BREW</span>
              <span id="brew_link_age">Main telemetry</span>
            </div>
            <!-- Shot / Step Context -->
            <div class="step-monitor-card">
              <div class="step-label">Profile</div>
              <div id="brew_profile_name" class="step-name" style="color:var(--accent2);">--</div>
              <div id="step_name_display" class="step-name">Initializing...</div>
              <div style="display:flex; justify-content:space-between; align-items:baseline; font-size:11px; color:#aaa; margin-top:8px;">
                <span id="step_time_progress">0s / 0s</span>
                <span id="step_percent">0%</span>
              </div>
              <div class="step-progress-bg">
                <div id="step_progress_bar" class="step-progress-fill"></div>
              </div>
              <div style="display:flex; justify-content:space-between; align-items:baseline; margin-top:10px;">
                <span class="brew-info-label">Timer</span>
                <span><span id="brew_time" style="font-size:25px; font-weight:900; color:var(--text); font-variant-numeric:tabular-nums;">00:00</span><span class="big-metric-unit">s</span></span>
              </div>
            </div>

            <!-- Target / Pump -->
            <div class="step-monitor-card">
              <div class="brew-info-grid">
                <div class="brew-info-cell">
                  <div class="brew-info-label">Target</div>
                  <div id="brew_target_value" class="brew-info-value" style="color:var(--accent);">--</div>
                  <div id="brew_target_mode" class="brew-info-sub">Mode: --</div>
                </div>
                <div class="brew-info-cell">
                  <div class="brew-info-label">Pump</div>
                  <div id="brew_pump_power" class="brew-info-value" style="color:#ff8c2a;">--%</div>
                  <div class="brew-info-sub">Output</div>
                </div>
              </div>
            </div>

            <!-- Grouped Metrics (Pressure, Weight, Flow) -->
            <div class="brew-primary-metrics">
              <div class="brew-metric">
                <span class="brew-metric-label">Pressure</span>
                <div class="brew-metric-value">
                  <span id="brew_pressure" class="brew-metric-number" style="color:var(--pressure);">0.0</span>
                  <span class="brew-metric-unit">bar</span>
                </div>
              </div>
              <div class="brew-metric">
                <span class="brew-metric-label">Yield</span>
                <div class="brew-metric-value">
                  <span id="brew_weight" class="brew-metric-number" style="color:var(--weight);">0.0</span>
                  <span class="brew-metric-unit">g</span>
                </div>
              </div>
              <div class="brew-metric">
                <span class="brew-metric-label">Pump Flow</span>
                <div class="brew-metric-value">
                  <span id="brew_flow" class="brew-metric-number" style="color:var(--flow);">0.0</span>
                  <span class="brew-metric-unit">ml/s</span>
                </div>
              </div>
            </div>
            
            <button id="btn_brew_stop_desktop" onclick="dashStop()" class="btn-stop-huge brew-stop-desktop" style="margin-top:auto;min-height:72px;font-size:19px;">STOP BREW</button>
          </div>

          <!-- SUMMARY SIDEBAR (Sau khi pha xong) -->
          <div id="brew_sidebar_summary" style="display:none; flex-direction:column; gap:10px; flex: 1;">
            <div class="step-monitor-card" style="text-align: center; padding: 25px 15px;">
              <div class="step-label" style="font-size: 13px; margin-bottom: 8px;">Extraction Complete</div>
              <div class="step-name" style="font-size: 22px; color: var(--ok); margin-bottom: 15px;">Enjoy your coffee! -</div>
              
              <div style="background: rgba(0,0,0,0.3); border-radius: 8px; padding: 15px; text-align: left;">
                <div style="display: flex; justify-content: space-between; border-bottom: 1px solid #333; padding-bottom: 8px; margin-bottom: 8px; gap:10px;">
                  <span style="color:var(--muted); font-size:12px; text-transform:uppercase;">Profile</span>
                  <span id="sum_profile" style="font-weight:bold; font-size:13px; color:var(--accent2); text-align:right; overflow:hidden; text-overflow:ellipsis; white-space:nowrap;">--</span>
                </div>
                <div style="display: flex; justify-content: space-between; border-bottom: 1px solid #333; padding-bottom: 8px; margin-bottom: 8px;">
                  <span style="color:var(--muted); font-size:12px; text-transform:uppercase;">Time</span>
                  <span id="sum_time" style="font-weight:bold; font-size:15px; color:var(--text);">-- s</span>
                </div>
                <div style="display: flex; justify-content: space-between; border-bottom: 1px solid #333; padding-bottom: 8px; margin-bottom: 8px;">
                  <span style="color:var(--muted); font-size:12px; text-transform:uppercase;">Yield</span>
                  <span id="sum_yield" style="font-weight:bold; font-size:15px; color:var(--weight);">-- g</span>
                </div>
                <div style="display: flex; justify-content: space-between; border-bottom: 1px solid #333; padding-bottom: 8px; margin-bottom: 8px;">
                  <span style="color:var(--muted); font-size:12px; text-transform:uppercase;">Target / Delta</span>
                  <span id="sum_delta" style="font-weight:bold; font-size:15px; color:var(--accent);">--</span>
                </div>
                <div style="display: flex; justify-content: space-between; border-bottom: 1px solid #333; padding-bottom: 8px; margin-bottom: 8px;">
                  <span style="color:var(--muted); font-size:12px; text-transform:uppercase;">Avg Flow</span>
                  <span id="sum_avg_flow" style="font-weight:bold; font-size:15px; color:var(--flow);">-- g/s</span>
                </div>
                <div style="display: flex; justify-content: space-between; margin-bottom: 8px;">
                  <span style="color:var(--muted); font-size:12px; text-transform:uppercase;">Peak Pressure</span>
                  <span id="sum_peak" style="font-weight:bold; font-size:15px; color:var(--pressure);">-- bar</span>
                </div>
              </div>
              
              <div style="margin-top: 14px; font-size: 12px; color: var(--accent); padding: 10px; border: 1px dashed var(--accent); border-radius: 8px; background: rgba(0,229,255,0.05); text-align:left;">
                <span style="display:block; font-size:10px; color:#aaa; margin-bottom:6px; text-transform:uppercase;">Phân tích</span>
                <div id="sum_note" style="display:flex; flex-direction:column; gap:4px; line-height:1.35;">...</div>
              </div>
            </div>
            
            <button onclick="goHome()" class="btn-stop-huge" style="margin-top: auto; background: rgba(255,255,255,0.1); border-color: rgba(255,255,255,0.2); color: #fff;">BACK TO HOME</button>
          </div>
          
        </div>

        <!-- Right: Real-time Chart -->
        <section class="card chart-card" style="display:flex; flex-direction:column;">
          <div class="chart-header">
            <div class="chart-live-label">
              <div class="chart-live-dot"></div>
              LIVE BREWING DATA
              <span id="chart_profile_name" style="margin-left:15px; color:var(--muted); font-weight:400; font-size:13px;"></span>
            </div>
            <div class="legend" style="margin:0;">
              <div class="legend-item" data-series="pressure"><div class="legend-swatch pressure"></div><span>P(bar)</span></div>
              <div class="legend-item" data-series="target"><div class="legend-swatch target"></div><span>Target</span></div>
              <div class="legend-item" data-series="flow"><div class="legend-swatch flow"></div><span>Pump F (ml/s)</span></div>
              <div class="legend-item" data-series="weight"><div class="legend-swatch weight"></div><span>W(g)</span></div>
              <div class="legend-item" data-series="temp"><div class="legend-swatch temp"></div><span>T(&deg;C)</span></div>
            </div>
          </div>
          <div id="dash_status_banner" style="display:none;"></div>
          <div id="chart_container" style="flex:1; display:flex; position:relative;">
            <canvas id="dash_chart" style="width:100%;"></canvas>
          </div>
        </section>

        <button id="btn_brew_stop_mobile" onclick="dashStop()" class="btn-stop-huge brew-stop-mobile">STOP BREW</button>

        <!-- Manual Floating Sidebar -->
        <div id="manual_overlay" class="manual-overlay" style="display:none;">
          <div class="drag-handle">::</div>
          <div id="manual_val_display" class="manual-val">50%</div>
          <input type="range" orient="vertical" id="manual_slider" min="0" max="100" class="vertical-slider" value="50">
        </div>

      </div>
    </div>

  </section>

  <!-- ===== PAGE: PROFILES ===== -->
  <section id="page-prof" class="page">
    <div id="profiles_workspace" class="profiles-workspace">
      <aside class="pf-library-pane" id="prof_lib_sec">
        <div class="pf-pane-head">
          <div><h2>Profiles</h2><span id="pf_library_count">0 profiles</span></div>
          <button onclick="newProfile()" class="pf-new-btn">+ New Profile</button>
        </div>
        <div id="pf_library_grid" class="pf-library-list"></div>
        <button onclick="importProfilesAdd()" class="pf-import-btn">Import profiles</button>
      </aside>

      <main id="prof_editor_sec" class="pf-editor-workspace" style="display:none;">
        <header class="pf-editor-head">
          <button onclick="closeEditor()" class="pf-back-btn" aria-label="Back to profiles">&#8249;</button>
          <div class="pf-editor-title"><strong id="pv_name">Profile Editor</strong><span id="pf_dirty_label">Saved</span></div>
          <div id="prof_tab_buttons_wrap" class="pf-type-switch">
            <button id="tab_btn_basic" onclick="switchProfTab(1)">Basic</button>
            <button id="tab_btn_adv" onclick="switchProfTab(0)">Advanced</button>
          </div>
        </header>
        <div id="pf_breadcrumb" class="pf-breadcrumb">Profiles</div>

        <div class="pf-editor-grid">
          <section class="pf-overview-pane">
            <div class="pf-global-fields">
              <label>Name<input id="pf_rename" type="text" placeholder="Profile name" oninput="updateProfileName(this.value)"></label>
              <label>Target yield<input id="pf_tot_w" type="number" step="0.5" onchange="saveProfileGlobals()"><span>g</span></label>
              <label>Max time<input id="pf_max_t" type="number" step="1" onchange="saveProfileGlobals()"><span>s</span></label>
            </div>
            <label class="pf-note-field">Note<input id="pf_note" type="text" maxlength="300" placeholder="Origin, roast date, recipe notes…" oninput="updateProfileNote(this.value)"></label>
            <div class="pf-preview-shell">
              <div><strong>Pressure &amp; Flow preview</strong><span id="pv_stats">--</span></div>
              <canvas id="pf_canvas" width="600" height="260"></canvas>
            </div>

            <div id="prof_tab_basic" class="pf-basic-panel">
              <label>Pre-infusion pressure <b id="val_preP">4.0 bar</b><input id="in_preP" type="range" min="1" max="6" step="0.1" value="4" oninput="uiUpdateBasic()"></label>
              <label>Soak time <b id="val_bloT">5 s</b><input id="in_bloT" type="range" min="0" max="30" step="1" value="5" oninput="uiUpdateBasic()"></label>
              <label>Main pressure <b id="val_mainP">9.0 bar</b><input id="in_mainP" type="range" min="6" max="12" step="0.1" value="9" oninput="uiUpdateBasic()"></label>
              <label>Target yield <b id="val_outW">36 g</b><input id="in_outW" type="range" min="10" max="60" step="1" value="36" oninput="uiUpdateBasic()"></label>
              <label>Maximum time <b id="val_maxT">45 s</b><input id="in_maxT" type="range" min="20" max="90" step="1" value="45" oninput="uiUpdateBasic()"></label>
              <div class="pf-basic-safety"><label><input type="checkbox" id="in_flowCap_en" onchange="uiUpdateBasic()"> Flow Cap</label><span id="flow_cap_val_wrap"><input id="in_flowCap_val" type="number" step="0.1" value="2.5" onchange="uiUpdateBasic()"> ml/s</span></div>
              <div id="pf_basic_summary" class="pf-basic-summary"></div>
            </div>

            <div id="prof_tab_adv" class="pf-step-timeline">
              <div class="pf-section-head"><strong>Steps</strong><span id="adv_step_count">0 / 8 steps</span></div>
              <div id="pf_container" class="pf-step-list"></div>
              <button onclick="addStep()" class="pf-add-step">+ Add Step</button>
            </div>
          </section>

          <aside id="pf_step_inspector" class="pf-step-inspector" aria-label="Step editor">
            <div id="pf_step_inspector_empty" class="pf-inspector-empty">Select a step to edit its control, safety and exit rules.</div>
            <div id="pf_step_inspector_content"></div>
          </aside>
        </div>

        <footer class="pf-editor-actions">
          <button class="pf-delete-profile" onclick="deleteProfile()">Delete Profile</button>
          <div id="pf_msg"></div>
          <button onclick="closeEditor()" class="pf-cancel-btn">Cancel</button>
          <button id="btn_save_pf" onclick="saveProfiles()" class="pf-save-btn">Save Profile</button>
        </footer>
      </main>
    </div>
  </section>

  <!-- ===== PAGE: CALIBRATION ===== -->
  <section id="page-cal" class="page">
    <div class="page-inner">
      <section class="card">
        <fieldset>
          <legend>Realtime Status (auto 1 s)</legend>
          <div class="row" style="margin-bottom:8px; gap:10px;">
            <div class="wifi-pill">
              <div id="zc_dot" class="dot"></div>
              <span id="zc_text">Zero-Cross: -- Hz</span>
            </div>
            <div id="pump_mode_pill" class="pill" style="border-radius:999px; padding:2px 10px; font-size:11px;">--</div>
          </div>
          <pre id="status">Loading...</pre>
        </fieldset>

        <fieldset>
          <legend>Brew temperature offset</legend>
          <p class="small">Nhiệt pha ước tính = nhiệt thân block − offset. Tăng offset làm nhiệt pha hiển thị thấp hơn.
          Hiệu chỉnh bằng nhiệt nước đo thực tế; giá trị ban đầu 21°C. Chỉ lưu khi máy nghỉ.</p>
          <div class="row"><div><label for="thermal_offset">Offset (°C)</label>
            <input id="thermal_offset" type="number" min="0" max="60" step="0.1" disabled></div></div>
          <button id="thermal_save" onclick="saveThermal()" disabled>Save offset</button>
          <button id="thermal_reload" onclick="loadThermal()">Reload</button>
          <span id="thermal_msg" class="small" role="status"></span>
        </fieldset>

        <fieldset>
          <legend>Pressure Calibration (2 điểm trên điện áp ADC)</legend>
          <div class="small" style="margin-bottom:8px">
            1) <b>Set Zero</b> khi xả tự do (0&nbsp;bar) để ESP đo <i>Offset (V@ADC)</i>.<br>
            2) Gắn phin mù, lên <b>bar bất kỳ</b> có từ đồng hồ cơ, nhập số rồi bấm <b>Set @ current bar</b>.<br>
            Công thức: <code>scale = bar / (V_ADC - offset)</code>.
          </div>
          <div class="row">
            <div><label>Offset (V @ 0)</label><input id="po" type="number" step="0.001" readonly></div>
            <div><label>Scale (bar/V)</label><input id="ps" type="number" step="0.001" readonly></div>
            <div><label>Current bar</label><input id="pbar" type="number" step="0.1" value="9.0"></div>
          </div>
          <div>
            <button onclick="setZero()"> Set Zero</button>
            <button onclick="setArb()"> Set @ current bar</button>
            <span id="presmsg" class="small"></span>
          </div>
        </fieldset>

        <fieldset>
          <legend>Pump Flow Calibration (Q1/Q9)</legend>
          <div class="row">
            <div>
              <label>Q1 — full power tại 1 bar (ml/s)</label>
              <input id="f_q1" type="number" min="0.5" max="20" step="0.01" oninput="previewFlowModel()">
              <div class="small">Đo lượng nước bơm trong một khoảng thời gian ổn định ở 1 bar.</div>
            </div>
            <div>
              <label>Q9 — full power tại 9 bar (ml/s)</label>
              <input id="f_q9" type="number" min="0.25" max="20" step="0.01" oninput="previewFlowModel()">
              <div class="small">Q9 phải nhỏ hơn Q1. Đây là pump flow, không phải dòng cà phê ra cốc.</div>
            </div>
          </div>

          <div style="margin-top:15px; background:rgba(0,229,255,0.05); padding:10px; border:1px dashed var(--accent); border-radius:8px;">
            <div style="font-size:12px; font-weight:bold; color:var(--accent); margin-bottom:4px;">Mô hình nội suy full-power</div>
            <div id="flow_preview" class="small">Nhập Q1 và Q9 để xem Q(1/3/6/9 bar).</div>
          </div>

          <div style="margin-top:15px">
            <button onclick="saveFlow()">Save Pump Model</button>
            <span id="fl_msg"></span>
          </div>
        </fieldset>
      </section>
    </div>
  </section>

  <!-- ===== PAGE: HISTORY ===== -->
  <section id="page-hist" class="page">
    <div class="hist-page">
      <aside class="hist-side">
        <div class="hist-side-head"><b>10 shot gần nhất</b><span id="hist_count" class="hist-count">—</span><button onclick="loadHmiHistory(true)">Làm mới</button></div>
        <div id="hist_list" class="hist-list"><div class="hist-empty">Open History to load TF records.</div></div>
      </aside>
      <article id="hist_detail" class="hist-detail"><div class="hist-empty">Choose a saved shot to inspect its control trace.</div></article>
    </div>
  </section>

  <!-- ===== PAGE: PID HEATER ===== -->
  <section id="page-heat" class="page">
    <div class="heater-shell">
      <div class="heater-head"><div><h2 class="settings-title">PID nhiệt thermoblock</h2><p class="small">Type K trên thân block. Thông số chỉ được Main chấp nhận khi heater OFF và máy ở Ready.</p></div><div id="heat_alarm" class="heater-alarm">Đang chờ Main…</div></div>
      <div class="settings-grid">
        <section class="settings-card"><h3>Trạng thái trực tiếp</h3><div id="heat_live" class="heater-live"><div class="heater-temperature"><span>Nhiệt thân</span><strong id="heat_liveTemp">— °C</strong><small>Mục tiêu <b id="heat_liveTarget">— °C</b></small></div><div><span>Công suất</span><strong id="heat_livePower">—%</strong></div><div><span>SSR</span><strong id="heat_liveSsr">—</strong></div><div><span>Trạng thái</span><strong id="heat_liveState">Đang chờ</strong></div></div><div class="heater-comp-live"><div><span>Flow đầu vào</span><b id="heat_liveFlow">— ml/s</b></div><div><span>PID</span><b id="heat_livePid">—%</b></div><div><span>Bù nhiệt</span><b id="heat_liveFf">—%</b></div><div><span>Thực tế / trần</span><b id="heat_liveTotal">—%</b></div></div><pre id="heat_applied" class="heater-pre">Chờ cấu hình Main…</pre><div class="settings-actions"><button id="heat_off" class="danger-button" onclick="HeaterTab.send(0)">TẮT GIA NHIỆT</button><button class="heat-mut" onclick="HeaterTab.send(2)">Preheat / bật PID</button><button class="heat-mut" onclick="HeaterTab.send(4)">Xóa lỗi (giữ OFF)</button></div><pre id="heat_reply" class="heater-pre"></pre></section>
        <section class="settings-card"><div class="heater-chart-head"><h3>Biểu đồ nhiệt</h3><button onclick="HeaterTab.download()">Tải CSV</button></div><div class="heater-legend"><span style="color:#ffaf54"><i></i>Nhiệt thân <b id="heat_legendTemp">— °C</b></span><span style="color:#fff"><i></i>Mục tiêu <b id="heat_legendTarget">— °C</b></span><span style="color:#62d9b0"><i></i>Công suất <b id="heat_legendPower">—%</b></span></div><canvas id="heat_plot" class="heater-plot" aria-label="Biểu đồ nhiệt độ và công suất theo thời gian"></canvas></section>
      </div>
      <section class="settings-card" style="margin-top:14px"><h3>Cấu hình PID và giới hạn</h3><div id="heat_fields" class="heater-fields"></div><div class="heater-settings-switches"><label><input id="heat_autoSleep" type="checkbox" checked> Bật tự nghỉ</label><label><input id="heat_ffEnabled" type="checkbox"> Bật bù nhiệt khi có dòng nước</label><button class="heat-mut" onclick="HeaterTab.send(1)">Lưu cấu hình</button></div><p class="heater-config-note">Flow được ước tính từ duty bơm và áp suất. Tổng PID + bù nhiệt luôn chịu giới hạn công suất đang áp dụng.</p></section>
      <details class="heater-fold"><summary>Thử công suất có giám sát</summary><div class="heater-fold-body settings-actions"><label>Công suất thử %<input id="heat_testPower" type="number" value="10" min="1" max="100"></label><label>Thời gian thử s<input id="heat_testSeconds" type="number" value="10" min="1" max="60"></label><button class="heat-mut" onclick="HeaterTab.send(3)">Chạy thử giới hạn</button></div></details>
      <details class="heater-fold"><summary>Chẩn đoán & lịch sử lỗi <span id="heat_diagBadge" class="heater-diagnostic-badge"></span></summary><div class="heater-fold-body"><pre id="heat_pidDetails" class="heater-pre">Chờ dữ liệu PID…</pre><pre id="heat_sensorDiagnostic" class="heater-pre">Chờ chẩn đoán cảm biến…</pre><pre id="heat_sensorWindow" class="heater-pre">Chờ thống kê cửa sổ 5 giây…</pre><pre id="heat_sensorPolicy" class="heater-pre">Chờ chính sách cảm biến từ Main…</pre></div></details>
    </div>
  </section>

  <!-- ===== PAGE: CONNECTIONS ===== -->
  <section id="page-conn" class="page">
    <div class="settings-page"><h2 class="settings-title">Connections</h2>
      <div class="connection-summary"><div class="connection-tile"><span>HMI Wi-Fi</span><b id="conn_wifi_state">Đang đọc…</b></div><div class="connection-tile"><span>Main Controller</span><b id="conn_main_state">Đang đọc…</b></div><div class="connection-tile"><span>Scale BLE</span><b id="conn_scale_state">Đang đọc…</b></div></div>
      <div class="settings-grid">
        <section class="settings-card"><h3>Wi-Fi</h3><div id="conn_wifi_detail" class="small">Đang đọc trạng thái mạng…</div><div class="settings-form" style="margin-top:12px"><label>Mạng tìm thấy<select id="conn_wifi_list"><option value="">-- Bấm quét mạng --</option></select></label><label>SSID<input id="conn_wifi_ssid" placeholder="Network name"></label><label>Password<input id="conn_wifi_pass" type="password" placeholder="Network password"></label><label class="checkbox-row"><input id="conn_wifi_auto" type="checkbox"> Tự động kết nối lại</label></div><div class="settings-actions"><button id="conn_wifi_scan" onclick="ConnectionsTab.scanWifi()">Quét mạng</button><button onclick="ConnectionsTab.saveWifi()">Lưu & kết nối</button><button onclick="ConnectionsTab.forgetWifi()">Quên mạng</button></div><div id="conn_wifi_msg" class="small"></div></section>
        <section class="settings-card"><h3>Cân BLE</h3><div id="conn_scale_detail" class="small">Đang đọc trạng thái cân…</div><div class="settings-actions"><button id="conn_scale_scan" onclick="ConnectionsTab.scanScale()">Quét cân</button><button onclick="ConnectionsTab.tare()">Tare thử</button><button onclick="ConnectionsTab.forgetScale()">Quên cân</button></div><div id="conn_scale_devices" class="device-list"><div class="small">Chưa quét cân.</div></div><div id="conn_scale_msg" class="small"></div></section>
      </div>
    </div>
  </section>

  <!-- ===== PAGE: SYSTEM ===== -->
  <section id="page-sys" class="page">
    <div class="settings-page"><h2 class="settings-title">System</h2>
      <section class="settings-card"><h3>Thiết bị</h3><div id="system_health" class="system-health"><div><span>HMI</span><b>Đang đọc…</b></div><div><span>Main</span><b>Đang đọc…</b></div><div><span>TF storage</span><b>Đang đọc…</b></div></div><div class="settings-actions"><button class="danger-button" onclick="SystemInfoTab.restart()">Khởi động lại HMI</button></div><details class="heater-fold"><summary>Thông tin nâng cao</summary><pre id="system_diagnostics" class="heater-pre heater-fold-body">Đang tải…</pre></details></section>
      <div id="system_firmware_mount" style="margin-top:14px"></div>
    </div>
  </section>

</div>
<div id="offline_banner" class="offline-banner">
  Mất kết nối với HMI. Kiểm tra Wi-Fi hoặc địa chỉ thiết bị.
</div>
<div id="pf_toast" role="status" aria-live="polite" style="position:fixed; bottom:70px; left:50%; transform:translateX(-50%); padding:8px 20px; border-radius:999px; font-size:13px; font-weight:600; color:#000; box-shadow:0 8px 24px rgba(0,0,0,0.5); opacity:0; pointer-events:none; transition:opacity 0.3s; z-index:9999; white-space:nowrap;"></div>

<script>
)HTML";

static const char WEB_HTML_BOTTOM[] PROGMEM = R"HTML(
</script>

</body>
</html>
)HTML";

#endif // WEB_HTML_H
