#include "webserver.h"
#include "config.h"
#include "settings.h"
#include "motor.h"
#include "mqtt.h"
#include "updater.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <Updater.h>
#include <ArduinoJson.h>

static ESP8266WebServer server(WEB_SERVER_PORT);
static DNSServer dnsServer;
static IPAddress apIP(AP_IP);

static bool webUploadOk = false;
static unsigned long lastUploadBytes = 0;
static unsigned long lastUploadLog = 0;

// ============================================================
//                        HTML (Setup mode / AP)
// ============================================================
static const char HTML_CONFIG[] PROGMEM =
"<!DOCTYPE html>"
"<html lang=\"en\"><head>"
"<meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>" AP_SSID "</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh;display:flex;align-items:center;justify-content:center}"
".card{background:#16213e;padding:40px;border-radius:16px;width:90%;max-width:480px;box-shadow:0 8px 32px rgba(0,0,0,0.3)}"
"h1{font-size:22px;margin-bottom:4px}"
".subtitle{font-size:14px;color:#888;margin-bottom:24px}"
".section{font-size:12px;color:#0f3460;text-transform:uppercase;letter-spacing:1px;margin:16px 0 8px}"
".section{color:#e94560}"
"input{width:100%;padding:12px;background:#1a1a2e;border:1px solid #333;border-radius:8px;color:#eee;font-size:14px;margin-bottom:8px;outline:none}"
"input:focus{border-color:#e94560}"
".row{display:flex;gap:8px}"
".row input:first-child{flex:1}"
".row input:last-child{width:100px}"
".btn{width:100%;padding:14px;border:none;border-radius:8px;font-size:15px;font-weight:600;cursor:pointer;margin-top:16px;background:#e94560;color:#fff}"
".btn:hover{background:#d63851}"
".btn:disabled{background:#555;cursor:not-allowed}"
".status{margin-top:16px;padding:12px;border-radius:8px;font-size:14px;display:none}"
".status.info{display:block;background:rgba(15,52,96,0.3);color:#4fc3f7}"
".status.success{display:block;background:rgba(78,204,163,0.15);color:#4ecca3}"
".status.error{display:block;background:rgba(233,69,96,0.15);color:#e94560}"
".note{font-size:12px;color:#555;margin-top:12px;text-align:center}"
".auth-toggle{display:flex;align-items:center;gap:10px;font-size:14px;color:#eee;margin:6px 0 10px;cursor:pointer}"
".auth-toggle input{width:18px;height:18px;accent-color:#4ecca3;cursor:pointer;margin:0}"
".note2{font-size:12px;color:#888;margin-bottom:8px}"
"</style></head><body>"
"<div class=\"card\">"
"<h1>SmartSpray</h1>"
"<div class=\"subtitle\">Configure WiFi & MQTT</div>"
"<div class=\"section\">Device</div>"
"<input type=\"text\" id=\"name\" value=\"" DEVICE_NAME_DEFAULT "\" placeholder=\"Device name\" autocomplete=\"off\">"
"<div class=\"note2\">Friendly name shown in Home Assistant. Leave SmartSpray to use the default.</div>"
"<div class=\"section\">WiFi Network</div>"
"<input type=\"text\" id=\"ssid\" placeholder=\"SSID\" required autocomplete=\"off\">"
"<input type=\"password\" id=\"pass\" placeholder=\"Password (leave blank if open)\" autocomplete=\"off\">"
"<div class=\"section\">MQTT Broker</div>"
"<div class=\"row\">"
"<input type=\"text\" id=\"host\" placeholder=\"Host or IP\" required autocomplete=\"off\">"
"<input type=\"number\" id=\"port\" placeholder=\"Port\" value=\"1883\" min=\"1\" max=\"65535\">"
"</div>"
"<input type=\"text\" id=\"user\" placeholder=\"Username (optional)\" autocomplete=\"off\">"
"<input type=\"password\" id=\"mqtt_pass\" placeholder=\"Password (optional)\" autocomplete=\"off\">"
"<div class=\"section\">Firmware Updates</div>"
"<input type=\"text\" id=\"updateUrl\" placeholder=\"http://host/SmartSpray.bin (optional)\" autocomplete=\"off\">"
"<div class=\"note2\">Plain HTTP URL to the .bin. Replaces the manual drag &amp; drop upload. Leave empty to disable auto-install (manual web OTA only).</div>"
"<div class=\"section\">Web Interface Access</div>"
"<label class=\"auth-toggle\"><input type=\"checkbox\" id=\"webAuth\" checked> Require a password for the web interface</label>"
"<input type=\"text\" id=\"webUser\" placeholder=\"Username (e.g. admin)\" autocomplete=\"off\">"
"<input type=\"password\" id=\"webPass\" placeholder=\"Password (min 4 chars)\" autocomplete=\"off\">"
"<div class=\"note2\">The username/password will be requested when opening the main device web page.</div>"
"<button class=\"btn\" id=\"saveBtn\" onclick=\"save()\">Save & Reboot</button>"
"<div class=\"status\" id=\"status\"></div>"
"<div class=\"note\">Connect to network <b>" AP_SSID "</b> and open browser to configure</div>"
"</div>"
"<script>"
"const webAuthBox=document.getElementById('webAuth');"
"const webAuthFields=['webUser','webPass'];"
"function toggleWebAuth(){"
"const on=webAuthBox.checked;"
"webAuthFields.forEach(id=>{const el=document.getElementById(id);el.disabled=!on;el.style.opacity=on?1:0.4})"
"}"
"webAuthBox.addEventListener('change',toggleWebAuth);"
"toggleWebAuth();"
"async function save(){"
"const btn=document.getElementById('saveBtn');"
"const status=document.getElementById('status');"
"btn.disabled=true;"
"status.className='status info';status.textContent='Saving...';"
"try{"
"const res=await fetch('/api/config',{"
"method:'POST',headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({"
"name:document.getElementById('name').value,"
"wifi_ssid:document.getElementById('ssid').value,"
"wifi_pass:document.getElementById('pass').value,"
"mqtt_host:document.getElementById('host').value,"
"mqtt_port:parseInt(document.getElementById('port').value)||1883,"
"mqtt_user:document.getElementById('user').value,"
"mqtt_pass:document.getElementById('mqtt_pass').value,"
"update_url:document.getElementById('updateUrl').value,"
"web_user:document.getElementById('webUser').value,"
"web_pass:document.getElementById('webPass').value,"
"web_auth_enabled:webAuthBox.checked?1:0})});"
"if(res.ok){"
"status.className='status success';status.textContent='Saved! Rebooting...';"
"setTimeout(()=>{window.location='/'},5000)"
"}else{"
"const t=await res.text();"
"status.className='status error';status.textContent='Error: '+t;"
"btn.disabled=false"
"}"
"}catch(e){"
"status.className='status error';status.textContent='Error: '+e.message;"
"btn.disabled=false"
"}"
"}"
"</script></body></html>";

// ============================================================
//                      HTML (Working mode / STA, OTA)
// ============================================================
static const char HTML_OTA[] PROGMEM =
"<!DOCTYPE html>"
"<html lang=\"en\"><head>"
"<meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>SmartSpray OTA</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:-apple-system,BlinkMacSystemFont,sans-serif;background:#1a1a2e;color:#eee;min-height:100vh;display:flex;align-items:center;justify-content:center}"
".card{background:#16213e;padding:40px;border-radius:16px;width:90%;max-width:480px;box-shadow:0 8px 32px rgba(0,0,0,0.3)}"
"h1{font-size:22px;margin-bottom:8px;color:#0f3460}"
".subtitle{font-size:14px;color:#888;margin-bottom:24px}"
".statusbar{display:flex;gap:12px;margin-bottom:20px;font-size:13px}"
".pill{flex:1;padding:8px;border-radius:8px;text-align:center;background:#1a1a2e;border:1px solid #333}"
".pill.ok{color:#4ecca3;border-color:#4ecca3}"
".pill.bad{color:#e94560;border-color:#e94560}"
".drop-zone{border:2px dashed #0f3460;border-radius:12px;padding:40px 20px;text-align:center;cursor:pointer}"
".drop-zone:hover,.drop-zone.dragover{border-color:#e94560;background:rgba(233,69,96,0.05)}"
".drop-zone.has-file{border-color:#4ecca3;background:rgba(78,204,163,0.05)}"
".drop-zone-icon{font-size:40px;margin-bottom:12px;color:#0f3460}"
".drop-zone-text{font-size:14px;color:#888}"
".drop-zone-text span{color:#e94560;text-decoration:underline}"
".file-name{font-size:13px;color:#4ecca3;margin-top:8px;display:none}"
".btn{width:100%;padding:14px;border:none;border-radius:8px;font-size:15px;font-weight:600;cursor:pointer;margin-top:16px}"
".btn:disabled{background:#555;color:#888;cursor:not-allowed}"
".btn-primary{background:#e94560;color:#fff}"
".btn-primary:disabled{background:#555;cursor:not-allowed}"
".btn-spray{background:#4ecca3;color:#123}"
".btn-check{background:#333;color:#eee;font-size:14px;margin-bottom:24px}"
".btn-reset{background:#333;color:#888;margin-top:8px;font-size:13px}"
".status{margin-top:16px;padding:12px;border-radius:8px;font-size:14px;display:none}"
".status.info{display:block;background:rgba(15,52,96,0.3);color:#4fc3f7}"
".status.success{display:block;background:rgba(78,204,163,0.15);color:#4ecca3}"
".status.error{display:block;background:rgba(233,69,96,0.15);color:#e94560}"
".ip-info{font-size:12px;color:#555;margin-top:20px;text-align:center}"
".sep{margin:20px 0;border:0;border-top:1px solid #333}"
".motor{margin-top:20px}"
".motor label.caption{display:block;font-size:12px;color:#888;line-height:1.5;margin-bottom:12px}"
".motor input[type=range]{width:100%;accent-color:#4ecca3;cursor:pointer}"
".power-row{display:flex;justify-content:space-between;align-items:center;font-size:13px;color:#888;margin-top:4px}"
".pill-sm{display:inline-block;padding:2px 8px;border-radius:6px;font-size:12px;background:#1a1a2e;border:1px solid #333}"
".pill-sm.ok{color:#4ecca3;border-color:#4ecca3}"
".pill-sm.bad{color:#e94560;border-color:#e94560}"
".btn-save{background:#4ecca3;color:#123}"
".btn-update{background:#4ecca3;color:#123}"
".upd-row{display:flex;gap:8px}"
".upd-row .btn{margin-top:0;flex:1}"
".switch-row{display:flex;align-items:center;gap:10px;font-size:14px;color:#eee;margin:18px 0 4px;cursor:pointer}"
".switch-row input{width:18px;height:18px;accent-color:#4ecca3;cursor:pointer}"
".switch-row span.note{color:#888;font-size:12px}"
".log-wrap{margin-top:20px}"
".log-head{display:flex;justify-content:space-between;align-items:center;margin-bottom:8px}"
".log-head span{font-size:12px;color:#888;text-transform:uppercase;letter-spacing:1px}"
".log-clear{background:none;border:1px solid #333;color:#888;border-radius:6px;padding:4px 10px;font-size:12px;cursor:pointer}"
".log-clear:hover{color:#eee;border-color:#e94560}"
"#logBox{background:#0f0f23;border:1px solid #333;border-radius:8px;padding:10px;height:200px;overflow-y:auto;font-family:ui-monospace,Menlo,Consolas,monospace;font-size:12px;line-height:1.5;color:#9fe8cf;white-space:pre-wrap;word-break:break-word}"
"</style></head><body>"
"<div class=\"card\">"
"<h1 id=\"devName\">SmartSpray</h1>"
"<div class=\"subtitle\">Firmware v" FW_VERSION "</div>"
"<div class=\"statusbar\">"
"<div class=\"pill\" id=\"stWifi\">WiFi …</div>"
"<div class=\"pill\" id=\"stMqtt\">MQTT …</div>"
"</div>"
"<button class=\"btn btn-spray\" onclick=\"sprayNow()\">&#x1F4A6; Spray</button>"
"<div class=\"motor\">"
"<label class=\"caption\">Motor speed (spray force). Tune it by trial: every air freshener has its own play in the mechanism. Find the lowest value that still has enough power to spray. Default is 188.</label>"
"<input type=\"range\" id=\"motorPower\" min=\"20\" max=\"255\" step=\"1\" oninput=\"document.getElementById('motorPowerVal').textContent=this.value\">"
"<div class=\"power-row\"><span>Spray force: <b id=\"motorPowerVal\"></b> / 255</span><button class=\"btn btn-save\" onclick=\"savePower()\">Save</button></div>"
"</div>"
"<hr class=\"sep\">"
"<button class=\"btn btn-check\" onclick=\"checkUpdate()\">Check for updates</button>"
"<div class=\"drop-zone\" id=\"dropZone\">"
"<div class=\"drop-zone-icon\">&#x1F4C1;</div>"
"<div class=\"drop-zone-text\">Drag & drop .bin file or <span>browse</span></div>"
"<div class=\"file-name\" id=\"fileName\"></div>"
"</div>"
"<input type=\"file\" id=\"fileInput\" accept=\".bin\" style=\"display:none\">"
"<button class=\"btn btn-update\" id=\"updBtn\" onclick=\"doUpdate()\" disabled>Update</button>"
"<div class=\"status\" id=\"status\"></div>"
"<div class=\"log-wrap\">"
"<div class=\"log-head\"><span>Logs</span><button class=\"log-clear\" onclick=\"clearLogs()\">Clear</button></div>"
"<div id=\"logBox\"></div>"
"</div>"
"<hr class=\"sep\">"
"<button class=\"btn btn-reset\" onclick=\"resetConfig()\">Reset settings</button>"
"<div class=\"ip-info\" id=\"ipInfo\"></div>"
"</div>"
"<script>"
"const dropZone=document.getElementById('dropZone');"
"const fileInput=document.getElementById('fileInput');"
"const fileName=document.getElementById('fileName');"
"const status=document.getElementById('status');"
"const updBtn=document.getElementById('updBtn');"
"let selectedFile=null;"
"let updAvailable=false;"
"let uiInit=false;"
"function syncUpd(){updBtn.disabled=!(selectedFile||updAvailable)}"
"function flagUpd(on){updAvailable=on;syncUpd()}"
"dropZone.addEventListener('click',()=>fileInput.click());"
"dropZone.addEventListener('dragover',(e)=>{e.preventDefault();dropZone.classList.add('dragover')});"
"dropZone.addEventListener('dragleave',()=>dropZone.classList.remove('dragover'));"
"dropZone.addEventListener('drop',(e)=>{e.preventDefault();dropZone.classList.remove('dragover');if(e.dataTransfer.files.length)handleFile(e.dataTransfer.files[0])});"
"fileInput.addEventListener('change',()=>{if(fileInput.files.length)handleFile(fileInput.files[0])});"
"function handleFile(file){"
"if(!file.name.endsWith('.bin')){showStatus('Only .bin files allowed','error');return}"
"selectedFile=file;fileName.textContent=file.name;fileName.style.display='block';"
"dropZone.classList.add('has-file');syncUpd()"
"}"
"function showStatus(msg,type){status.textContent=msg;status.className='status '+type}"
"async function sprayNow(){"
"showStatus('Spraying...','info');"
"try{const res=await fetch('/api/spray',{method:'POST'});"
"if(res.ok){showStatus('Spray done!','success')}else{showStatus('Failed','error')}"
"}catch(e){showStatus('Error: '+e.message,'error')}"
"}"
"async function checkUpdate(){"
"showStatus('Checking for updates...','info');flagUpd(false);"
"try{const res=await fetch('/api/check-update',{method:'POST'});const t=await res.text();"
"if(res.ok&&t.indexOf('AVAILABLE:')===0){"
"const v=t.slice(10);showStatus('New version '+v+' available. Click «Update».','success');flagUpd(true)"
"}else if(res.ok){showStatus(t,'success')}"
"else{showStatus(t,'error')}"
"}catch(e){showStatus('Error: '+e.message,'error')}"
"}"
"async function doUpdate(){"
"try{"
"if(selectedFile){"
"showStatus('Uploading firmware...','info');"
"const fd=new FormData();fd.append('firmware',selectedFile,selectedFile.name);"
"const res=await fetch('/update',{method:'POST',body:fd});"
"if(res.ok){showStatus('Success! Rebooting...','success');setTimeout(()=>window.location.reload(),5000)}"
"else{const t=await res.text();showStatus('Error: '+t,'error')}"
"return"
"}"
"if(updAvailable){"
"if(!confirm('Install the firmware update?'))return;"
"showStatus('Installing update...','info');flagUpd(false);"
"const res=await fetch('/api/update',{method:'POST'});const t=await res.text();"
"if(res.ok){showStatus(t,'success');setTimeout(()=>window.location.reload(),5000)}else{showStatus(t,'error')}"
"return"
"}"
"showStatus('No firmware to install','error')"
"}catch(e){showStatus('Error: '+e.message,'error')}"
"}"
"function savePower(){"
"const p=parseInt(document.getElementById('motorPower').value);"
"showStatus('Saving spray force...','info');"
"fetch('/api/power',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({power:p})}).then(r=>{"
"if(r.ok){showStatus('Spray force: '+p+' / 255','success');document.getElementById('motorPowerVal').textContent=p}else{showStatus('Save failed','error')}"
"}).catch(()=>showStatus('Save failed','error'))"
"}"
"async function resetConfig(){"
"if(!confirm('Reset all settings and reboot to setup mode?'))return;"
"showStatus('Resetting...','info');"
"try{const res=await fetch('/api/reset-config',{method:'POST'});"
"if(res.ok){showStatus('Rebooting to setup mode...','success');setTimeout(()=>window.location.reload(),5000)}"
"else{showStatus('Reset failed','error')}"
"}catch(e){showStatus('Error: '+e.message,'error')}"
"}"
"fetch('/api/ip').then(r=>r.text()).then(ip=>{document.getElementById('ipInfo').textContent='Device IP: '+ip+' | FW v" FW_VERSION "'}).catch(()=>{});"
"function refreshStatus(){"
"fetch('/api/status').then(r=>r.json()).then(s=>{"
"const w=document.getElementById('stWifi');w.textContent='WiFi '+(s.wifi?'✓':'✗');w.className='pill '+(s.wifi?'ok':'bad');"
"const m=document.getElementById('stMqtt');m.textContent='MQTT '+(s.mqtt?'✓':'✗');m.className='pill '+(s.mqtt?'ok':'bad');"
"if(s.upd){flagUpd(true)}"
"if(s.id){"
"document.getElementById('ipInfo').textContent=s.ip+' | Device ID: '+s.id+' | FW v" FW_VERSION "'"
"}"
"if(!uiInit){"
"document.getElementById('motorPower').value=s.power;document.getElementById('motorPowerVal').textContent=s.power;uiInit=true"
"}"
"}).catch(()=>{})"
"}"
"setInterval(refreshStatus,5000);refreshStatus();"
"let lastLog='';"
"function refreshLogs(){"
"fetch('/api/logs').then(r=>r.text()).then(t=>{"
"const box=document.getElementById('logBox');"
"const atBottom=box.scrollHeight-box.scrollTop-box.clientHeight<30;"
"if(t!==lastLog){box.textContent=t;lastLog=t;if(atBottom)box.scrollTop=box.scrollHeight}"
"}).catch(()=>{})"
"}"
"function clearLogs(){fetch('/api/logs/clear',{method:'POST'});document.getElementById('logBox').textContent='';lastLog=''}"
"setInterval(refreshLogs,3000);refreshLogs()"
"</script></body></html>";

// ============================================================
//                       HTTP handlers
// ============================================================

static void wbConfigPage() {
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html", HTML_CONFIG);
}

static void wbOtaPage() {
  server.sendHeader("Cache-Control", "no-cache");
  String html = FPSTR(HTML_OTA);
  html.replace("<title>SmartSpray OTA</title>", String("<title>") + deviceName() + " OTA</title>");
  html.replace("<h1 id=\"devName\">SmartSpray</h1>",
               String("<h1 id=\"devName\">") + deviceName() + "</h1>");
  server.send(200, "text/html", html);
}

// GET /api/status - connection status, spray force, auto-update and version
static void wbStatus() {
  String json = String("{\"wifi\":") + (WiFi.status() == WL_CONNECTED ? 1 : 0)
              + ",\"mqtt\":" + (mqttConnected() ? 1 : 0)
              + ",\"power\":" + motorGetPower()
              + ",\"upd\":" + (updaterAvailable() ? 1 : 0)
              + ",\"version\":\"" FW_VERSION "\""
              + ",\"ip\":\"" + WiFi.localIP().toString() + "\""
              + ",\"id\":\"" + deviceId() + "\"}";
  server.send(200, "application/json", json);
}

static void wbIp() {
  server.send(200, "text/plain", WiFi.localIP().toString());
}

// GET /api/logs - last log lines
static void wbLogs() {
  server.send(200, "text/plain; charset=utf-8", logGet());
}

// POST /api/logs/clear - clear the log buffer
static void wbLogsClear() {
  logClear();
  server.send(200, "text/plain", "OK");
}

// POST /api/spray - "Spray" button
static void wbSpray() {
  spray();
  server.send(200, "text/plain", "OK");
}

// POST /api/power - set the spray force ({"power":N})
static void wbPower() {
  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) { server.send(400, "text/plain", "Invalid JSON"); return; }

  int p = doc["power"] | -1;
  if (p < SETTINGS_MOTOR_POWER_MIN || p > SETTINGS_MOTOR_POWER_MAX) {
    server.send(400, "text/plain", "Power out of range");
    return;
  }

  gSettings.motor_power = (uint8_t)p;
  settingsSave(gSettings);
  motorSetPower((uint8_t)p);
  server.send(200, "text/plain", "OK");
}

// POST /api/check-update - check GitHub (check only, no install)
static void wbCheckUpdate() {
  switch (updaterCheck()) {
    case UPDATE_RESULT_AVAILABLE:
      mqttPublishDeviceInfo();
      server.send(200, "text/plain", "AVAILABLE:" + updaterLatestVersion());
      break;
    case UPDATE_RESULT_NO_UPDATES:
      mqttPublishDeviceInfo();
      server.send(200, "text/plain", "NO UPDATE FOUND. The latest version is installed.");
      break;
    case UPDATE_RESULT_ERROR:
    default:
      server.send(500, "text/plain", "Error checking for updates");
      break;
  }
}

// POST /api/update - install the found update
static void wbUpdate() {
  // Just set a flag; the real download runs from loop() after we respond, so
  // web handling stays responsive and the download runs in a quiet context.
  if (updaterRequestInstall()) {
    server.send(200, "text/plain", "Update started...");
  } else {
    server.send(500, "text/plain", "No available update");
  }
}

// POST /api/reset-config - reset to setup mode
static void wbResetConfig() {
  settingsReset();
  server.send(200, "text/plain", "OK");
  delay(1000);
  ESP.restart();
}

// POST /api/config - save settings from the config page (AP mode)
static void wbConfigSave() {
  String body = server.arg("plain");
  if (body.length() == 0) {
    server.send(400, "text/plain", "No data");
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }

  AppSettings s;
  memset(&s, 0, sizeof(s));

  strncpy(s.name, doc["name"] | DEVICE_NAME_DEFAULT, SETTINGS_NAME_MAX - 1);
  strncpy(s.wifi_ssid, doc["wifi_ssid"] | "", SETTINGS_WIFI_SSID_MAX - 1);
  strncpy(s.wifi_pass, doc["wifi_pass"] | "", SETTINGS_WIFI_PASS_MAX - 1);
  strncpy(s.mqtt_host, doc["mqtt_host"] | "", SETTINGS_MQTT_HOST_MAX - 1);
  strncpy(s.mqtt_user, doc["mqtt_user"] | "", SETTINGS_MQTT_USER_MAX - 1);
  strncpy(s.mqtt_pass, doc["mqtt_pass"] | "", SETTINGS_MQTT_PASS_MAX - 1);
  uint16_t port = doc["mqtt_port"] | 1883;
  s.mqtt_port = (port == 0) ? 1883 : port;

  // Web auth for the web interface (working mode)
  strncpy(s.web_user, doc["web_user"] | "", SETTINGS_WEB_USER_MAX - 1);
  strncpy(s.web_pass, doc["web_pass"] | "", SETTINGS_WEB_PASS_MAX - 1);

  // Plain-HTTP firmware URL (empty = manual web OTA only).
  strncpy(s.update_url, doc["update_url"] | "", SETTINGS_UPDATE_URL_MAX - 1);
  uint8_t webAuth = doc["web_auth_enabled"] | 0;
  if (webAuth) {
    // Auth enabled: username and password required, password not shorter than minimum.
    if (s.web_user[0] == '\0' || strlen(s.web_pass) < SETTINGS_WEB_PASS_MIN) {
      server.send(400, "text/plain", "Web auth: username and password (min 4 chars) are required");
      return;
    }
    s.web_auth_enabled = 1;
  } else {
    s.web_auth_enabled = 0;
  }

  if (s.wifi_ssid[0] == '\0' || s.mqtt_host[0] == '\0') {
    server.send(400, "text/plain", "SSID and MQTT host are required");
    return;
  }

  if (!settingsSave(s)) {
    server.send(500, "text/plain", "Save failed");
    return;
  }

  server.send(200, "text/plain", "OK");
  delay(500);
  ESP.restart();
}

// ============================================================
//                 Web interface authorization (STA)
// ============================================================
// If auth is enabled (web_auth_enabled), check the HTTP Basic credentials.
static bool webAuthOk() {
  if (!gSettings.web_auth_enabled) return true;
  return server.authenticate(gSettings.web_user, gSettings.web_pass);
}

// If not authorized, request Basic auth and return false (the handler should exit).
static bool webRequireAuth() {
  if (webAuthOk()) return true;
  server.requestAuthentication(BASIC_AUTH);
  return false;
}

// ============================================================
//                    Web firmware upload (STA)
// ============================================================
static void wbUpdateUpload() {
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    // CRITICAL: check auth BEFORE Update.begin(), because the upload handler
    // runs before the completion handler and could start writing to flash.
    if (!webAuthOk()) {
      DEBUG_PRINTLN("[WEB] Upload rejected: not authorized");
      webUploadOk = false;
      return;
    }
    DEBUG_PRINTF("[WEB] Upload: %s (%u bytes), heap=%u\n",
                 up.filename.c_str(), (unsigned)up.totalSize, ESP.getFreeHeap());
    webUploadOk = false;
    lastUploadBytes = 0;
    lastUploadLog = 0;
    // With multipart upload, totalSize = size of the HTTP request, not the file,
    // so take the whole available app size (as ESP8266HTTPUpdateServer does).
    uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
    if (!Update.begin(maxSketchSpace)) {
      DEBUG_PRINTF("[WEB] Update.begin: %s\n", Update.getErrorString().c_str());
    }
  } else if (up.status == UPLOAD_FILE_WRITE) {
    size_t writen = Update.write(up.buf, up.currentSize);
    if (writen != up.currentSize) {
      DEBUG_PRINTF("[WEB] Update write error: %s\n", Update.getErrorString().c_str());
    }
    // progress: log every ~64KB
    lastUploadBytes += up.currentSize;
    if (lastUploadBytes - lastUploadLog >= 65536) {
      lastUploadLog = lastUploadBytes;
      DEBUG_PRINTF("[WEB] ...%u/%u bytes, heap=%u\n",
                   (unsigned)lastUploadBytes, (unsigned)up.totalSize, ESP.getFreeHeap());
    }
  } else if (up.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      DEBUG_PRINTLN("[WEB] Firmware received");
      webUploadOk = true;
    } else {
      DEBUG_PRINTF("[WEB] Update.end: %s\n", Update.getErrorString().c_str());
      webUploadOk = false;
    }
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    DEBUG_PRINTLN("[WEB] Upload aborted (client disconnected)");
  }
  delay(0);
}

static void wbUpdateComplete() {
  if (webUploadOk) {
    server.send(200, "text/plain", "OK");
    delay(1000);
    ESP.restart();
  } else {
    server.send(500, "text/plain", "UPDATE FAILED");
  }
}

static void wbNotFound() {
  server.sendHeader("Location", "http://" + apIP.toString(), true);
  server.send(302, "text/plain", "");
}

// ============================================================
//                       Modes
// ============================================================
void webInitAp() {
  server.on("/", HTTP_GET, wbConfigPage);
  server.on("/api/config", HTTP_POST, wbConfigSave);
  server.onNotFound(wbNotFound);

  server.begin();
  DEBUG_PRINTLN("[WEB] Setup server started");

  dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
  dnsServer.start(53, "*", apIP);
}

void webInitSta() {
  // All working-mode endpoints are protected by HTTP Basic auth (if auth enabled).
  server.on("/", HTTP_GET, []() { if (webRequireAuth()) wbOtaPage(); });
  server.on("/api/status", HTTP_GET, []() { if (webRequireAuth()) wbStatus(); });
  server.on("/api/ip", HTTP_GET, []() { if (webRequireAuth()) wbIp(); });
  server.on("/api/logs", HTTP_GET, []() { if (webRequireAuth()) wbLogs(); });
  server.on("/api/logs/clear", HTTP_POST, []() { if (webRequireAuth()) wbLogsClear(); });
  server.on("/api/spray", HTTP_POST, []() { if (webRequireAuth()) wbSpray(); });
  server.on("/api/power", HTTP_POST, []() { if (webRequireAuth()) wbPower(); });
  server.on("/api/check-update", HTTP_POST, []() { if (webRequireAuth()) wbCheckUpdate(); });
  server.on("/api/update", HTTP_POST, []() { if (webRequireAuth()) wbUpdate(); });
  server.on("/api/reset-config", HTTP_POST, []() { if (webRequireAuth()) wbResetConfig(); });
  server.on("/update", HTTP_POST,
            []() { if (webAuthOk()) wbUpdateComplete(); else server.requestAuthentication(BASIC_AUTH); },
            wbUpdateUpload);

  server.begin();
  DEBUG_PRINTLN("[WEB] OTA server started");
}

void webLoop() {
  server.handleClient();
  dnsServer.processNextRequest();
}

void webStop() {
  server.close();
  dnsServer.stop();
  DEBUG_PRINTF("[WEB] Server stopped, heap=%d\n", ESP.getFreeHeap());
}
