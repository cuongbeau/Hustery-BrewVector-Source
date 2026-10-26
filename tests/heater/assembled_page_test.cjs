// Validate the actual streamed root document, not each JS fragment separately.
const fs=require('node:fs'),path=require('node:path'),vm=require('node:vm'),assert=require('node:assert/strict');
const root='hmi/src/web', constants=new Map();
for(const name of fs.readdirSync(root).filter(n=>n.endsWith('.h'))){
 const content=fs.readFileSync(path.join(root,name),'utf8');
 const re=/(?:static\s+)?const char\s+(\w+)\[\]\s+PROGMEM\s*=\s*R"(\w+)\(([\s\S]*?)\)\2";/g;
 for(const m of content.matchAll(re))constants.set(m[1],m[3]);
}
const packedSource=fs.readFileSync(path.join(root,'web_root_gzip.h'),'utf8');
const packed=Buffer.from(packedSource.match(/HMI_WEB_GZIP\[\].*?\{([\s\S]*?)\};/)[1].match(/\d+/g).map(Number));
const html=require('node:zlib').gunzipSync(packed).toString('utf8');
const order=['WEB_HTML_TOP','WEB_JS_CORE','WEB_JS_DASH','WEB_JS_PROFILES','WEB_JS_CALIB','WEB_JS_HISTORY','WEB_JS_CONNECTIONS','WEB_JS_HEATER','WEB_JS_SYSTEM','WEB_JS_HMI_SYSTEM','WEB_HTML_BOTTOM'];
assert.equal(html,order.map(k=>constants.get(k).replace(/\r\n?/g,'\n')).join(''),'Compressed root must match current source assets');
assert(packed.length<Buffer.byteLength(html)/2,'Root compression regressed');
let count=0;
for(const m of html.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script>/gi)){
 new vm.Script(m[1],{filename:'assembled-root-'+(++count)+'.js'});
}
assert(count>0,'No script assembled');
const withoutScripts=html.replace(/<script\b[^>]*>[\s\S]*?<\/script>/gi,'');
assert(/data-tab=['"]heat['"]/.test(withoutScripts),'PID heater must be a main tab');
assert(/data-tab=['"]conn['"]/.test(withoutScripts),'Connections must be a main tab');
assert(!/<a\b[^>]*href=['"]\/heater['"]/.test(withoutScripts),'Standalone heater link must be removed');
const tabs=[...withoutScripts.matchAll(/data-tab=['"]([^'"]+)['"]/g)].map(m=>m[1]);
assert.deepEqual(tabs.slice(0,7),['dash','prof','hist','cal','heat','conn','sys'],'Main tab order regressed');
assert(!withoutScripts.includes('HMI Display, Sound & Storage'),'Removed HMI settings card returned');
assert(!withoutScripts.includes('Chẩn đoán bơm'),'Removed pump diagnostics card returned');
assert(withoutScripts.includes('SystemInfoTab.restart()'),'System restart control is missing');
assert(withoutScripts.includes('id="heat_applied"'),'Heater applied-config output is missing');
assert(withoutScripts.includes('Bật bù nhiệt khi có dòng nước'),'Water thermal compensation label is missing');
assert(!withoutScripts.includes('Bật bù nước thử nghiệm'),'Obsolete experimental water-compensation label returned');
assert(withoutScripts.includes('id="heat_diagBadge"'),'Sensor warnings must have a diagnostics-only indicator');
assert(html.includes('Phản ứng tức thời theo sai lệch nhiệt độ'),'PID coefficient help text is missing');
assert(html.includes("fillText('Nhiệt °C'")&&html.includes("fillText('Công suất %'"),'Heater chart axes are missing');
assert(!html.includes("$('appliedConfig')"),'Heater script references the obsolete appliedConfig id');
assert(withoutScripts.includes('id="f_q1"')&&withoutScripts.includes('id="f_q9"'),'Q1/Q9 pump-flow editor is missing');
assert(!withoutScripts.includes('id="f_pstall"')&&!withoutScripts.includes('id="f_alpha"'),'Legacy flow controls returned');
assert(html.includes('q1_ml_s:q1,q9_ml_s:q9'),'Q1/Q9 calibration payload regressed');
assert(html.includes("histModeButton('flows','Lưu lượng')"),'Separate Pump/Puck Flow history view is missing');
assert(html.includes('pump_flow_ml_s')&&html.includes('puck_flow_ml_s'),'Named Pump/Puck Flow series are missing');
console.log('PASS: compressed root matches source, parses and exposes the approved tab layout.');
