"""Local browser QA only: serves actual gzip assets with a simulated Main.
Never proxies or sends anything to the espresso machine.
"""
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path
from urllib.parse import urlparse, parse_qs
import json, re, time

ROOT = Path(__file__).resolve().parents[2]
WEB = ROOT / 'hmi/src/web'
START = time.monotonic()
jobs = {}
profile = json.loads((ROOT / 'profiles' / 'Adaptive_v2_BrewVector_38g.json').read_text(encoding='utf-8'))

class Handler(BaseHTTPRequestHandler):
    def reply(self, data, code=200, content_type='application/json', headers=None):
        if not isinstance(data, bytes):
            data = json.dumps(data, ensure_ascii=False).encode() if isinstance(data, (dict, list)) else data.encode()
        self.send_response(code)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(data)))
        self.send_header('Cache-Control', 'no-store')
        for k, v in (headers or {}).items(): self.send_header(k, v)
        self.end_headers()
        try: self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError): pass

    def do_GET(self):
        url = urlparse(self.path)
        if url.path == '/':
            src = (WEB/'web_root_gzip.h').read_text(encoding='utf-8')
            data = bytes(map(int, re.search(r'HMI_WEB_GZIP\[\].*?\{(.*?)\};', src, re.S)[1].replace('\n', '').strip(',').split(',')))
            return self.reply(data, content_type='text/html; charset=utf-8', headers={'Content-Encoding': 'gzip'})
        if url.path == '/heater':
            page = re.search(r'PAGE\[\].*?R"HTML\((.*?)\)HTML"', (WEB/'routes/heater_routes.cpp').read_text(encoding='utf-8'), re.S)[1]
            return self.reply(page, content_type='text/html; charset=utf-8')
        if url.path == '/api/heater':
            time.sleep(4)
            return self.reply(dict(online=True, ticket=0, result=0, mode=0, fault=0, uptime=int(time.monotonic()*1000), sampleAge=25,
              raw=77.75, filtered=77.955, power=0, p=0, i=0, d=0, relay=0, valid=1, target=115, ceiling=125,
              kp=2, ki=.02, kd=5, maxPower=30, waterMaxPower=100, activeMaxPower=30, autoSleep=True,
              idleMinutes=15, idleRemaining=0, sleepDelaySeconds=0, stableSeconds=10, heaterW=1300,
              inletC=25, ffScale=.25, ffEnabled=False, ready=False, sensorDiagnosticsFresh=False))
        if url.path == '/profiles':
            job = len(jobs)+1; jobs[job] = time.monotonic()
            return self.reply(dict(job=job), 202)
        if url.path == '/api/hmi/job':
            job = int(parse_qs(url.query)['id'][0])
            if time.monotonic()-jobs[job]<3: return self.reply(dict(job=job), 202)
            return self.reply(dict(ok=True, profiles=[profile]))
        if url.path == '/dash/profiles':
            return self.reply(dict(ok=True, profiles=[dict(idx=0,name='OFF',steps=0),dict(idx=1,name='MANUAL',steps=0),dict(idx=2,name='Bài kiểm thử',steps=1)]))
        if url.path == '/dash/live':
            return self.reply(dict(ok=True, ts=int((time.monotonic()-START)*1000), state='IDLE',machineState='READY',profileName='Bài kiểm thử',profileIdx=2,
              heaterFresh=True,compatible=True,isBrewing=False,pressure=0,target=0,flow=0,weight=0,hasScale=False,pumpPower=0,curStep=1,totStep=1,boilerTemp=77.95,blockTempC=77.95,
              heaterOnline=True,heaterMode=0,heaterFault=0,heaterReady=False,heaterPaused=False,
              p=0,f=0,w=0,t=0,duty=0,wifi=True))
        if url.path == '/favicon.ico': return self.reply(b'',204)
        return self.reply(dict(error='Mock endpoint unavailable'),404)

    def do_POST(self):
        self.rfile.read(int(self.headers.get('Content-Length', 0)))
        self.reply(dict(error='QA preview: commands disabled'),409)

print('HMI QA preview http://127.0.0.1:8768 — simulated Main only',flush=True)
ThreadingHTTPServer(('127.0.0.1', 8768), Handler).serve_forever()
