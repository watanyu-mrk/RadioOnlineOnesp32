#include "web/web_setup.h"

void WebSetup::begin(StationManager *stations) {
  stations_ = stations;
  WiFi.persistent(false);
  // AP_STA (not plain AP): scanning for networks while the portal is up needs the STA
  // interface, and in AP-only mode the scan can drop the phone's connection.
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(SETUP_AP_SSID, SETUP_AP_PASS);
  // Also try the saved network (non-blocking): the page then works from a laptop on the
  // same LAN as well as from a phone on the setup AP. Connecting is optional - the AP
  // alone is enough to finish setup.
  staStarted_ = stations_ && stations_->hasWifi();
  staSince_ = millis();
  if (staStarted_) WiFi.begin(stations_->ssid(), stations_->pass());
  routes();
  server_.begin();
  running_ = true;
  Serial.printf("setup     : AP \"%s\" pass \"%s\" ip=%s\n", SETUP_AP_SSID, SETUP_AP_PASS,
                WiFi.softAPIP().toString().c_str());
}

void WebSetup::loop() {
  if (!running_) return;
  if (staStarted_ && !staReported_) {
    if (WiFi.status() == WL_CONNECTED) {
      staReported_ = true;
      Serial.printf("setup     : also on LAN http://%s\n",
                    WiFi.localIP().toString().c_str());
    } else if (millis() - staSince_ > 20000) {
      staReported_ = true;  // give up quietly, the AP is still up
      Serial.println("setup     : saved network unreachable, AP only");
    }
  }
  server_.handleClient();
  if (rebootAt_ && millis() > rebootAt_) {
    Serial.println("setup     : rebooting on request");
    delay(200);
    ESP.restart();
  }
}

String WebSetup::jsonEscape(const char *s) {
  String out;
  if (!s) return out;
  for (const char *p = s; *p; p++) {
    char c = *p;
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n' || c == '\r') {
      out += ' ';
    } else {
      out += c;
    }
  }
  return out;
}

void WebSetup::routes() {
  server_.on("/", HTTP_GET, [this]() { server_.send_P(200, "text/html", PAGE); });

  server_.on("/api/status", HTTP_GET, [this]() {
    Serial.printf("http      : GET /api/status from %s\n",
                  server_.client().remoteIP().toString().c_str());
    String j = "{\"ssid\":\"" + jsonEscape(stations_ ? stations_->ssid() : "") +
               "\",\"stations\":" + String(stations_ ? stations_->count() : 0) + "}";
    server_.send(200, "application/json", j);
  });

  server_.on("/api/scan", HTTP_GET, [this]() {
    Serial.println("http      : GET /api/scan");
    int n = WiFi.scanNetworks();
    String j = "[";
    for (int i = 0; i < n; i++) {
      if (i) j += ",";
      j += "{\"ssid\":\"" + jsonEscape(WiFi.SSID(i).c_str()) +
           "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    }
    j += "]";
    WiFi.scanDelete();
    server_.send(200, "application/json", j);
  });

  server_.on("/api/wifi", HTTP_POST, [this]() {
    String ssid = server_.arg("ssid");
    String pass = server_.arg("pass");
    if (ssid.length() == 0) {
      server_.send(400, "application/json", "{\"ok\":false,\"error\":\"ssid required\"}");
      return;
    }
    if (stations_) stations_->setWifi(ssid.c_str(), pass.c_str());
    Serial.printf("setup     : wifi saved ssid=\"%s\"\n", ssid.c_str());
    server_.send(200, "application/json", "{\"ok\":true}");
    rebootAt_ = millis() + 1200;
  });

  server_.on("/api/stations", HTTP_GET, [this]() {
    String j = "[";
    uint8_t n = stations_ ? stations_->count() : 0;
    for (uint8_t i = 0; i < n; i++) {
      if (i) j += ",";
      j += "{\"name\":\"" + jsonEscape(stations_->station(i).name) + "\",\"url\":\"" +
           jsonEscape(stations_->station(i).url) + "\"}";
    }
    j += "]";
    server_.send(200, "application/json", j);
  });

  server_.on("/api/station/add", HTTP_POST, [this]() {
    Serial.println("http      : POST /api/station/add");
    String name = server_.arg("name");
    String url = server_.arg("url");
    int idx = stations_ ? stations_->add(name.c_str(), url.c_str()) : -1;
    if (idx < 0) {
      server_.send(400, "application/json",
                   "{\"ok\":false,\"error\":\"list full or url empty\"}");
      return;
    }
    Serial.printf("setup     : station added \"%s\"\n", name.c_str());
    server_.send(200, "application/json", "{\"ok\":true}");
  });

  server_.on("/api/station/del", HTTP_POST, [this]() {
    int idx = server_.arg("idx").toInt();
    bool ok = stations_ && idx >= 0 && stations_->remove((uint8_t)idx);
    server_.send(ok ? 200 : 400, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
  });

  server_.on("/api/reboot", HTTP_POST, [this]() {
    server_.send(200, "application/json", "{\"ok\":true}");
    rebootAt_ = millis() + 600;
  });

  server_.onNotFound([this]() { server_.send(404, "text/plain", "not found"); });
}

const char WebSetup::PAGE[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Retro Radio Setup</title><style>
*{box-sizing:border-box}
body{background:#0c0906;color:#ffecb6;font-family:system-ui,-apple-system,sans-serif;margin:0;padding:14px;max-width:640px;margin:0 auto}
h1{font-size:20px;color:#ffba4a;letter-spacing:3px;text-transform:uppercase;margin:14px 0}
h2{font-size:13px;color:#ffba4a;letter-spacing:2px;text-transform:uppercase;margin:0 0 8px}
.card{border:1px solid #d67832;background:#1e140c;padding:12px;margin:12px 0;border-radius:6px}
label{display:block;font-size:12px;color:#9e846a;margin-top:10px}
input{width:100%;padding:9px;background:#0c0906;border:1px solid #684018;color:#ffecb6;border-radius:4px;font-size:14px}
button{background:#ffba4a;color:#140f08;border:0;padding:9px 14px;border-radius:4px;font-weight:700;margin-top:12px;font-size:14px}
button.ghost{background:transparent;color:#ffba4a;border:1px solid #d67832}
button.mini{padding:4px 8px;margin:0;font-size:12px}
table{width:100%;border-collapse:collapse;font-size:13px}
td{border-bottom:1px solid #3a2410;padding:6px 4px;word-break:break-all;vertical-align:top}
td.x{width:34px;text-align:right}
.small{font-size:12px;color:#9e846a;margin-top:6px}
.net{padding:6px;border-bottom:1px solid #2a1a0c;cursor:pointer}
.net:hover{background:#2a1a0c}
</style></head><body>
<h1>Retro Radio</h1>
<div class="card">
<h2>1 &middot; Wi-Fi</h2>
<div class="small" id="cur">loading...</div>
<label>Network name (SSID)</label><input id="ssid" maxlength="32" autocomplete="off">
<label>Password</label><input id="pass" type="password" maxlength="63">
<button onclick="scan()">Scan networks</button>
<div id="nets" class="small"></div>
<button onclick="saveWifi()">Save &amp; reboot</button>
<div class="small" id="wmsg"></div>
</div>
<div class="card">
<h2>2 &middot; Stations</h2>
<table id="tbl"><tbody></tbody></table>
<label>Name</label><input id="sname" maxlength="39" placeholder="My station">
<label>Stream URL</label><input id="surl" maxlength="159" placeholder="http://...">
<button onclick="addStation()">Add station</button>
<div class="small" id="smsg"></div>
</div>
<div class="card">
<button class="ghost" onclick="reboot()">Reboot to player</button>
</div>
<script>
const $=id=>document.getElementById(id);
async function api(u,o){const r=await fetch(u,o);try{return await r.json()}catch(e){return {ok:false}}}
async function load(){
 const s=await api('/api/status');
 $('cur').textContent=s.ssid?('Saved network: '+s.ssid):'No Wi-Fi saved yet';
 const list=await api('/api/stations');
 const t=$('tbl').querySelector('tbody');
 t.innerHTML=list.length?'':'<tr><td class="small">No stations yet</td><td></td></tr>';
 list.forEach((st,i)=>{
  const tr=document.createElement('tr');
  tr.innerHTML='<td><b>'+esc(st.name)+'</b><div class="small">'+esc(st.url)+'</div></td>'+
   '<td class="x"><button class="mini" onclick="delStation('+i+')">x</button></td>';
  t.appendChild(tr);
 });
}
function esc(s){return (s||'').replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]))}
async function scan(){
 $('nets').textContent='scanning...';
 const n=await api('/api/scan');
 const box=$('nets'); box.innerHTML='';
 if(!n.length){box.textContent='no networks found';return}
 n.sort((a,b)=>b.rssi-a.rssi).forEach(x=>{
  const d=document.createElement('div');
  d.className='net';
  d.textContent=(x.ssid||'(hidden)')+'  '+x.rssi+' dBm';
  d.onclick=()=>{$('ssid').value=x.ssid;$('pass').focus()};
  box.appendChild(d);
 });
}
async function saveWifi(){
 const b=new FormData();b.append('ssid',$('ssid').value.trim());b.append('pass',$('pass').value);
 const r=await api('/api/wifi',{method:'POST',body:b});
 $('wmsg').textContent=r.ok?'saved - rebooting...':('error: '+(r.error||'failed'));
}
async function addStation(){
 const b=new FormData();b.append('name',$('sname').value.trim());b.append('url',$('surl').value.trim());
 const r=await api('/api/station/add',{method:'POST',body:b});
 if(!r.ok){$('smsg').textContent='error: '+(r.error||'failed');return}
 $('sname').value='';$('surl').value='';$('smsg').textContent='added';load();
}
async function delStation(i){
 const b=new FormData();b.append('idx',i);
 await api('/api/station/del',{method:'POST',body:b});load();
}
async function reboot(){await api('/api/reboot',{method:'POST'});$('cur').textContent='rebooting...'}
load();
</script></body></html>)HTML";
