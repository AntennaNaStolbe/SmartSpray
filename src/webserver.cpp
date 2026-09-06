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
R"cfg(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#101419">
<title>)cfg"
AP_SSID
R"cfg(</title>
<style>
:root{
  --bg:#101419;--srf:#161c24;--srf2:#1b222d;--srf3:#232b39;
  --brd:#2a3542;--brd2:#3a4757;
  --tx:#e7ecf3;--tx2:#9fadbd;--tx3:#7d8b9c;
  --acc:#6d8cff;--acc2:#8ba5ff;
  --ok:#43d69a;--warn:#f2b544;--err:#f5646c;
  --r:12px;--r2:10px;--r3:8px;
  --sh:0 12px 34px rgba(0,0,0,.35);
}
*{box-sizing:border-box;margin:0;padding:0}
html{-webkit-text-size-adjust:100%}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;background:var(--bg);color:var(--tx);line-height:1.5;-webkit-font-smoothing:antialiased;min-height:100vh}
.wrap{max-width:480px;margin:0 auto;padding:26px 14px 60px}
.app-head{margin:8px 0 18px}
.app-head h1{font-size:23px;font-weight:700;letter-spacing:-.01em}
.subtitle{font-size:13px;color:var(--tx2);margin-top:2px}
.card{background:var(--srf);border:1px solid var(--brd);border-radius:var(--r);box-shadow:var(--sh);padding:24px 20px}
.card+.card{margin-top:16px}
.card-title{font-size:12px;font-weight:700;letter-spacing:.08em;text-transform:uppercase;color:var(--acc2);margin:4px 0 12px}
.sec{border:0;border-top:1px solid var(--brd);margin:20px 0 16px}
.hint{font-size:12.5px;color:var(--tx2);line-height:1.55;margin-top:6px}
.hint.err{color:var(--err)}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:8px;width:100%;min-height:44px;padding:0 16px;border:1px solid transparent;border-radius:var(--r2);font-family:inherit;font-size:15px;font-weight:600;letter-spacing:.01em;cursor:pointer;transition:background .15s,border-color .15s,box-shadow .15s,color .15s,transform .05s,opacity .15s;-webkit-tap-highlight-color:transparent}
.btn:active{transform:translateY(1px)}
.btn:focus-visible{outline:2px solid rgba(109,140,255,.6);outline-offset:2px}
.btn:disabled{cursor:not-allowed;opacity:.45;transform:none}
.btn-primary{background:#fff;color:#0d1117}
.btn-primary:hover:not(:disabled){background:#e9eef6}
.btn.loading{pointer-events:none}
.btn.loading::after{content:"";width:15px;height:15px;border:2px solid currentColor;border-top-color:transparent;border-radius:50%;display:inline-block;vertical-align:-2px;animation:rot .7s linear infinite;margin-left:8px;opacity:.9}
@keyframes rot{to{transform:rotate(360deg)}}
input[type=text],input[type=password],input[type=url],input[type=number]{width:100%;height:44px;padding:0 13px;background:var(--srf2);border:1px solid var(--brd);border-radius:var(--r3);color:var(--tx);font-size:14.5px;font-family:inherit;outline:none;transition:border-color .15s,box-shadow .15s,opacity .15s;margin-bottom:10px;-webkit-appearance:none}
input:focus{border-color:var(--acc);box-shadow:0 0 0 3px rgba(109,140,255,.15)}
input:disabled{opacity:.45;cursor:not-allowed}
input::placeholder{color:var(--tx3)}
input.error{border-color:var(--err)}
.row{display:flex;gap:10px}
.row .grow{flex:1}
.row input:last-child{width:110px;flex:none}
.switch-row{display:flex;align-items:center;gap:12px;cursor:pointer;-webkit-tap-highlight-color:transparent;padding:6px 0 4px;margin-bottom:10px}
.switch-row input{position:absolute;opacity:0;width:0;height:0}
.sw{position:relative;width:42px;height:24px;flex:none}
.slider{position:absolute;inset:0;border-radius:999px;background:var(--srf3);border:1px solid var(--brd2);transition:background .15s,border-color .15s}
.slider::before{content:"";position:absolute;top:2px;left:2px;width:18px;height:18px;border-radius:50%;background:var(--tx3);transition:transform .15s,background .15s}
.switch-row input:checked+.sw .slider{background:rgba(109,140,255,.25);border-color:var(--acc)}
.switch-row input:checked+.sw .slider::before{transform:translateX(18px);background:var(--acc)}
.switch-row:focus-visible .slider{outline:2px solid rgba(109,140,255,.6);outline-offset:2px}
.sw-label{font-size:14px;color:var(--tx)}
.msg{display:none;border-radius:var(--r3);padding:12px 14px;font-size:13.5px;line-height:1.5;border:1px solid;margin-top:14px}
.msg.show{display:block}
.msg.info{background:rgba(109,140,255,.1);color:#b8c6ff;border-color:rgba(109,140,255,.35)}
.msg.success{background:rgba(67,214,154,.1);color:#8df0c4;border-color:rgba(67,214,154,.35)}
.msg.error{background:rgba(245,100,108,.1);color:#ffa4aa;border-color:rgba(245,100,108,.4)}
.foot{text-align:center;font-size:12.5px;color:var(--tx3);margin-top:22px;line-height:1.7}
.foot b{color:var(--tx2)}
@media(max-width:520px){
.wrap{padding:16px 10px 40px}
.card{padding:18px 16px}
.btn{min-height:48px}
}
</style>
</head>
<body>
<main class="wrap">
  <header class="app-head">
    <h1>SmartSpray</h1>
    <div class="subtitle">Configure WiFi &amp; MQTT</div>
  </header>

  <form class="card" id="cfgForm" novalidate>
    <div class="card-title">Device</div>
    <input type="text" id="name" value=")cfg"
DEVICE_NAME_DEFAULT
R"cfg(" placeholder="Device name" autocomplete="off">
    <p class="hint">Friendly name shown in Home Assistant and the web UI.</p>

    <hr class="sec">
    <div class="card-title">WiFi Network</div>
    <input type="text" id="ssid" placeholder="SSID" required autocomplete="off">
    <input type="password" id="pass" placeholder="Password (leave blank if open)" autocomplete="off">

    <hr class="sec">
    <div class="card-title">MQTT Broker</div>
    <div class="row">
      <input type="text" class="grow" id="host" placeholder="Host or IP" required autocomplete="off">
      <input type="number" id="port" placeholder="Port" value="1883" min="1" max="65535">
    </div>
    <input type="text" id="user" placeholder="Username (optional)" autocomplete="off">
    <input type="password" id="mqtt_pass" placeholder="Password (optional)" autocomplete="off">

    <hr class="sec">
    <div class="card-title">Web Interface Access</div>
    <label class="switch-row" for="webAuth">
      <input type="checkbox" id="webAuth" checked>
      <span class="sw"><span class="slider"></span></span>
      <span class="sw-label">Username and password for the web interface</span>
    </label>
    <input type="text" id="webUser" placeholder="Username (e.g. admin)" autocomplete="off">
    <input type="password" id="webPass" placeholder="Password (min 4 chars)" autocomplete="off">
    <p class="hint">These credentials will be requested when opening the main device page.</p>

    <button class="btn btn-primary spacer" id="saveBtn" type="submit" style="margin-top:18px">Save &amp; Reboot</button>
    <div class="msg" id="status"></div>
  </form>

  <p class="foot">After saving, the device connects to the WiFi network you entered above, and the web interface becomes available at the device's IP address on your local network.</p>
</main>

<script>
(function(){
"use strict";
var webAuthBox=document.getElementById('webAuth');
var webFields=['webUser','webPass'];
function syncAuth(){
  var on=webAuthBox.checked;
  webFields.forEach(function(id){var el=document.getElementById(id);el.disabled=!on;if(id==='webPass'&&!on)el.value='';});
}
webAuthBox.addEventListener('change',syncAuth);
syncAuth();

var statusEl=document.getElementById('status');
var saveBtn=document.getElementById('saveBtn');
function setMsg(type,text){statusEl.className='msg '+type;statusEl.textContent=text;statusEl.classList.add('show')}
function clearMsg(){statusEl.classList.remove('show')}
function flagErr(id,on){document.getElementById(id).classList.toggle('error',on)}

document.getElementById('cfgForm').addEventListener('submit',function(e){
  e.preventDefault();
  if(saveBtn.disabled)return;
  saveBtn.disabled=true;saveBtn.classList.add('loading');
  setMsg('info','Saving…');
  var payload={
    name:document.getElementById('name').value.trim(),

    wifi_ssid:document.getElementById('ssid').value.trim(),
    wifi_pass:document.getElementById('pass').value,
    mqtt_host:document.getElementById('host').value.trim(),
    mqtt_port:parseInt(document.getElementById('port').value,10)||1883,
    mqtt_user:document.getElementById('user').value,
    mqtt_pass:document.getElementById('mqtt_pass').value,
    web_user:document.getElementById('webUser').value.trim(),
    web_pass:document.getElementById('webPass').value,
    web_auth_enabled:webAuthBox.checked?1:0
  };
  flagErr('host',false);flagErr('ssid',false);
  if(!payload.wifi_ssid){flagErr('ssid',true);setMsg('error','WiFi network name is required.');saveBtn.disabled=false;saveBtn.classList.remove('loading');return}
  if(!payload.mqtt_host){flagErr('host',true);setMsg('error','MQTT broker host is required.');saveBtn.disabled=false;saveBtn.classList.remove('loading');return}
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)}).then(function(r){
    return r.text().then(function(t){return {ok:r.ok,text:t}});
  }).then(function(res){
    if(res.ok){setMsg('success','Saved! Rebooting…');setTimeout(function(){window.location='/'},5000)}
    else{setMsg('error',res.text||'Save failed.');saveBtn.disabled=false;saveBtn.classList.remove('loading')}
  }).catch(function(){
    setMsg('error','Cannot reach the device.');saveBtn.disabled=false;saveBtn.classList.remove('loading');
  });
});
})();
</script>
</body>
</html>)cfg";

// ============================================================
//                      HTML (Working mode / STA, OTA)
// ============================================================
static const char HTML_OTA[] PROGMEM =
R"ota(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#101419">
<title>SmartSpray OTA</title>
<style>
:root{
  --bg:#101419;--srf:#161c24;--srf2:#1b222d;--srf3:#232b39;
  --brd:#2a3542;--brd2:#3a4757;
  --tx:#e7ecf3;--tx2:#9fadbd;--tx3:#7d8b9c;
  --acc:#6d8cff;--acc2:#8ba5ff;
  --ok:#43d69a;--warn:#f2b544;--err:#f5646c;
  --r:12px;--r2:10px;--r3:8px;
  --sh:0 12px 34px rgba(0,0,0,.35);
}
*{box-sizing:border-box;margin:0;padding:0}
html{-webkit-text-size-adjust:100%}
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;background:var(--bg);color:var(--tx);line-height:1.5;-webkit-font-smoothing:antialiased;min-height:100vh}
.wrap{max-width:560px;margin:0 auto;padding:26px 14px 60px}
.app-head{margin:8px 0 18px}
.app-head h1{font-size:23px;font-weight:700;letter-spacing:-.01em}
.subtitle{font-size:13px;color:var(--tx2);margin-top:2px}
.pills{display:flex;gap:10px;margin-top:14px}
.pill{display:inline-flex;align-items:center;gap:7px;padding:7px 12px;border-radius:999px;font-size:12px;font-weight:600;background:var(--srf2);border:1px solid var(--brd);color:var(--tx2);transition:color .15s,border-color .15s}
.pill .dot{width:8px;height:8px;border-radius:50%;background:var(--tx3);transition:background .15s}
.pill.ok{color:var(--ok);border-color:var(--ok)}
.pill.bad{color:var(--err);border-color:var(--err)}
.pill.warn{color:var(--warn);border-color:var(--warn)}
.pill.ok .dot,.pill.bad .dot,.pill.warn .dot{background:currentColor}
.card{background:var(--srf);border:1px solid var(--brd);border-radius:var(--r);box-shadow:var(--sh);padding:20px}
.card+.card{margin-top:16px}
.card-title{display:flex;align-items:center;gap:8px;font-size:14px;font-weight:700;letter-spacing:.02em;margin-bottom:14px}
.card-title svg{flex:none;opacity:.85}
.hint{font-size:12.5px;color:var(--tx2);line-height:1.55;margin-top:10px}
.btn{display:inline-flex;align-items:center;justify-content:center;gap:8px;width:100%;min-height:44px;padding:0 16px;border:1px solid transparent;border-radius:var(--r2);font-family:inherit;font-size:15px;font-weight:600;letter-spacing:.01em;cursor:pointer;transition:background .15s,border-color .15s,box-shadow .15s,color .15s,transform .05s,opacity .15s;-webkit-tap-highlight-color:transparent}
.btn:active{transform:translateY(1px)}
.btn:focus-visible{outline:2px solid rgba(109,140,255,.6);outline-offset:2px}
.btn:disabled{cursor:not-allowed;opacity:.45;transform:none}
.btn-primary{background:#fff;color:#0d1117}
.btn-primary:hover:not(:disabled){background:#e9eef6}
.btn-outline{background:var(--srf2);border-color:var(--brd2);color:var(--tx)}
.btn-outline:hover:not(:disabled){border-color:var(--acc);color:#fff}
.btn-ghost{background:transparent;border-color:transparent;color:var(--tx2);width:auto;min-height:auto;padding:6px 10px;font-size:13px;font-weight:600;border-radius:var(--r3)}
.btn-ghost:hover:not(:disabled){color:var(--tx);background:var(--srf3)}
.btn-danger{background:transparent;border-color:rgba(245,100,108,.4);color:var(--err)}
.btn-danger:hover:not(:disabled){background:rgba(245,100,108,.12);border-color:var(--err)}
.btn svg{flex:none}
.btn.loading{pointer-events:none}
.btn.loading::after{content:"";width:15px;height:15px;border:2px solid currentColor;border-top-color:transparent;border-radius:50%;display:inline-block;vertical-align:-2px;animation:rot .7s linear infinite;margin-left:8px;opacity:.9}
@keyframes rot{to{transform:rotate(360deg)}}
.toast-wrap{position:fixed;top:14px;left:50%;transform:translateX(-50%);z-index:60;display:flex;flex-direction:column;gap:8px;width:min(92vw,420px);pointer-events:none}
.toast{display:flex;align-items:flex-start;gap:10px;background:var(--srf);border:1px solid var(--brd2);border-left:3px solid var(--acc);border-radius:var(--r3);box-shadow:var(--sh);padding:11px 13px;font-size:13px;color:var(--tx);line-height:1.45;opacity:0;transform:translateY(-6px);transition:opacity .18s,transform .18s}
.toast.show{opacity:1;transform:none}
.toast::before{content:"";width:8px;height:8px;border-radius:50%;background:var(--acc);flex:none;margin-top:5px}
.toast.success::before{background:var(--ok)}
.toast.error::before{background:var(--err)}
.toast.warn::before{background:var(--warn)}
.state-bar{display:none;align-items:center;justify-content:center;gap:9px;background:rgba(109,140,255,.12);color:#b8c6ff;border:1px solid rgba(109,140,255,.4);border-radius:var(--r3);padding:12px 14px;font-size:13px;font-weight:600;margin-bottom:16px;text-align:center}
.state-bar.show{display:flex}
.state-bar .spin{width:14px;height:14px;border:2px solid currentColor;border-top-color:transparent;border-radius:50%;animation:rot .7s linear infinite;flex:none}
.offline{display:none;background:rgba(245,100,108,.12);color:#ffb4b9;border:1px solid rgba(245,100,108,.4);border-radius:var(--r3);padding:10px 14px;font-size:13px;font-weight:600;margin-bottom:16px;text-align:center}
.offline.show{display:block}
.label{display:block;font-size:12.5px;font-weight:600;color:var(--tx2);margin-bottom:6px;letter-spacing:.02em}
.value-row{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-top:4px}
.value-row .val{font-size:13px;color:var(--tx2)}
.value-row .val b{color:var(--tx)}
.pwr{font-size:12px;font-weight:600;color:var(--tx3);min-height:1em}
.pwr.busy{color:var(--warn)}
.pwr.ok{color:var(--ok)}
.range{width:100%;-webkit-appearance:none;appearance:none;height:6px;border-radius:999px;background:var(--srf3);outline:none;margin:14px 0 8px;cursor:pointer}
.range::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:#fff;border:2px solid var(--acc);box-shadow:0 2px 6px rgba(0,0,0,.4);cursor:pointer}
.range::-moz-range-thumb{width:18px;height:18px;border-radius:50%;background:#fff;border:2px solid var(--acc);box-shadow:0 2px 6px rgba(0,0,0,.4);cursor:pointer}
.range-row{display:flex;align-items:center;gap:12px}
.range-row .range{flex:1;min-width:0;width:auto}
.stepper{display:flex;gap:6px;flex:none}
.btn-step{width:42px;height:42px;border-radius:var(--r3);background:var(--srf2);border:1px solid var(--brd2);color:var(--tx);font-size:19px;font-weight:600;line-height:1;cursor:pointer;display:flex;align-items:center;justify-content:center;padding:0;transition:background .15s,border-color .15s,color .15s,transform .05s;-webkit-tap-highlight-color:transparent}
.btn-step:hover:not(:disabled){border-color:var(--acc);color:#fff}
.btn-step:active:not(:disabled){transform:translateY(1px)}
.btn-step:disabled{opacity:.45;cursor:not-allowed}
.btn-step:focus-visible{outline:2px solid rgba(109,140,255,.6);outline-offset:2px}
.or-divider{display:flex;align-items:center;gap:12px;margin:20px 0;color:var(--tx3);font-size:11.5px;font-weight:700;letter-spacing:.12em;text-transform:uppercase;user-select:none}
.or-divider::before,.or-divider::after{content:"";flex:1;height:1px;background:var(--brd2)}
.drop{padding:22px 16px;border:2px dashed var(--brd2);border-radius:var(--r2);background:var(--srf2);text-align:center;cursor:pointer;transition:border-color .15s,background .15s}
.drop:hover,.drop.over{border-color:var(--acc);background:rgba(109,140,255,.06)}
.drop.has-file{border-color:var(--ok);background:rgba(67,214,154,.06)}
.drop svg{width:32px;height:32px;color:var(--tx3);margin-bottom:8px}
.drop p{font-size:13.5px;color:var(--tx2)}
.drop p b{color:var(--acc)}
.drop .fname{display:none;margin-top:8px;font-size:13px;color:var(--ok);font-weight:600;word-break:break-all;align-items:center;justify-content:center;gap:8px}
.drop.has-file .fname{display:flex}
.drop .fname button{background:none;border:none;color:var(--ok);font-size:15px;cursor:pointer;line-height:1;padding:2px 4px}
.drop .fname button:hover{color:#fff}
.spacer{margin-top:16px}
.logbox{background:#0a0d11;border:1px solid var(--brd);border-radius:var(--r3);padding:12px;height:200px;overflow-y:auto;font-family:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;font-size:12px;line-height:1.55;color:#a5e8c9;white-space:pre-wrap;word-break:break-word}
.log-empty{color:var(--tx3);text-align:center;padding:40px 0;font-size:13px}
.foot{text-align:center;font-size:12.5px;color:var(--tx3);margin-top:22px;line-height:1.7}
.modal-bg{position:fixed;inset:0;background:rgba(5,8,12,.65);display:none;align-items:center;justify-content:center;padding:20px;z-index:50}
.modal-bg.open{display:flex}
.modal{width:100%;max-width:400px;background:var(--srf);border:1px solid var(--brd2);border-radius:var(--r);box-shadow:var(--sh);padding:22px}
.modal h3{font-size:16px;font-weight:700;margin-bottom:8px}
.modal p{font-size:14px;color:var(--tx2);margin-bottom:20px;line-height:1.55}
.modal .mbtns{display:flex;gap:10px}
.modal .mbtns .btn{margin:0}
@media(max-width:520px){
.wrap{padding:16px 10px 40px}
.card{padding:16px}
.btn{min-height:48px}
}
</style>
</head>
<body>
<main class="wrap">
  <header class="app-head">
    <h1 id="devName">SmartSpray</h1>
    <div class="subtitle">Firmware v)ota"
FW_VERSION
R"ota(</div>
    <div class="pills">
      <span class="pill" id="stWifi"><span class="dot"></span><span class="pl">WiFi · …</span></span>
      <span class="pill" id="stMqtt"><span class="dot"></span><span class="pl">MQTT · …</span></span>
    </div>
  </header>

  <div class="state-bar" id="stateBar"></div>
  <div class="offline" id="offlineBox">Connection to the device lost — waiting for it to come back…</div>

  <section class="card">
    <div class="card-title"><svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 2.69l5.66 5.66a8 8 0 1 1-11.31 0z"/></svg>Spray</div>
    <button class="btn btn-primary" id="sprayBtn" type="button">Spray</button>
    <div class="spacer"></div>
    <label class="label" for="motorPower">Spray force</label>
    <div class="range-row">
      <input type="range" class="range" id="motorPower" min="20" max="255" step="1">
      <span class="stepper">
        <button type="button" class="btn-step" id="powMinus" aria-label="Decrease spray force">−</button>
        <button type="button" class="btn-step" id="powPlus" aria-label="Increase spray force">+</button>
      </span>
    </div>
    <div class="value-row">
      <span class="val"><b id="motorPowerVal">–</b> / 255</span>
      <span class="pwr" id="pwrState"></span>
    </div>
    <p class="hint">Auto-saves while you drag. Find the lowest value that still sprays reliably — every freshener has its own play. Default is 188.</p>
  </section>

  <section class="card">
    <div class="card-title"><svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M21 12a9 9 0 1 1-2.64-6.36"/><path d="M21 3v6h-6"/></svg>Firmware update</div>
    <button class="btn btn-outline" id="chkBtn" type="button">
      <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M12 .5C5.65.5.5 5.65.5 12c0 5.08 3.29 9.39 7.86 10.91.58.11.79-.25.79-.56 0-.27-.01-1.17-.02-2.12-3.2.7-3.87-1.36-3.87-1.36-.52-1.33-1.28-1.68-1.28-1.68-1.04-.71.08-.7.08-.7 1.15.08 1.76 1.18 1.76 1.18 1.03 1.76 2.69 1.25 3.35.96.1-.75.4-1.26.72-1.55-2.55-.29-5.23-1.28-5.23-5.68 0-1.26.45-2.28 1.18-3.09-.12-.29-.51-1.46.11-3.05 0 0 .96-.31 3.15 1.18a10.9 10.9 0 0 1 2.87-.39c.97 0 1.95.13 2.87.39 2.19-1.49 3.15-1.18 3.15-1.18.62 1.59.23 2.76.11 3.05.73.81 1.18 1.83 1.18 3.09 0 4.41-2.69 5.38-5.25 5.67.41.36.78 1.06.78 2.14 0 1.55-.01 2.8-.01 3.18 0 .31.21.67.8.56A11.52 11.52 0 0 0 23.5 12C23.5 5.65 18.35.5 12 .5z"/></svg>
      Check GitHub for updates
    </button>
    <div class="or-divider">OR</div>
    <div class="drop" id="dropZone" role="button" tabindex="0" aria-label="Upload .bin firmware">
      <svg width="32" height="32" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 17v2a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-2"/><path d="M7 9l5-5 5 5"/><path d="M12 4v12"/></svg>
      <p>Drag &amp; drop a <b>.bin</b> file here, or <b>browse</b></p>
      <div class="fname" id="fileName"><span class="fn"></span><button type="button" id="fileClear" aria-label="Remove file">&times;</button></div>
    </div>
    <input type="file" id="fileInput" accept=".bin" hidden>
    <button class="btn btn-primary spacer" id="updBtn" type="button" disabled>Update</button>
    <p class="hint" id="updHint">Check GitHub for the latest release, or upload a .bin to install it manually.</p>
  </section>

  <section class="card">
    <div class="card-title"><svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 17l6-6-6-6"/><path d="M12 19h8"/></svg>Logs<button class="btn btn-ghost" id="logClear" type="button" style="margin-left:auto">Clear</button></div>
    <div class="logbox" id="logBox"><div class="log-empty">No logs yet</div></div>
    <p class="hint">Live diagnostics from the device. Refreshes automatically.</p>
  </section>

  <section class="card">
    <div class="card-title"><svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 22s8-4 8-10V5l-8-3-8 3v7c0 6 8 10 8 10z"/></svg>Maintenance</div>
    <button class="btn btn-danger" id="resetBtn" type="button">Reset settings</button>
    <p class="hint">Erase WiFi, MQTT and web credentials, then restart into the setup access point.</p>
  </section>

  <p class="foot">Device <span id="ipInfo">…</span></p>
</main>

<div class="toast-wrap" id="toastWrap" aria-live="polite"></div>

<div class="modal-bg" id="modalBg">
  <div class="modal" role="dialog" aria-modal="true">
    <h3 id="modalTitle">Are you sure?</h3>
    <p id="modalBody"></p>
    <div class="mbtns">
      <button class="btn btn-ghost" id="modalCancel" type="button">Cancel</button>
      <button class="btn btn-primary" id="modalOk" type="button">Confirm</button>
    </div>
  </div>
</div>

<script>
(function(){
"use strict";
var $=function(id){return document.getElementById(id)};
var stWifi=$('stWifi'),stMqtt=$('stMqtt');
var sprayBtn=$('sprayBtn');
var motorPower=$('motorPower'),motorPowerVal=$('motorPowerVal'),pwrState=$('pwrState'),powMinus=$('powMinus'),powPlus=$('powPlus');
var chkBtn=$('chkBtn'),updBtn=$('updBtn'),updHint=$('updHint');
var dropZone=$('dropZone'),fileInput=$('fileInput'),fileName=$('fileName'),fileClear=$('fileClear');
var logBox=$('logBox'),logClear=$('logClear');
var resetBtn=$('resetBtn'),ipInfo=$('ipInfo'),offlineBox=$('offlineBox'),stateBar=$('stateBar');
var modalBg=$('modalBg'),modalTitle=$('modalTitle'),modalBody=$('modalBody'),modalOk=$('modalOk'),modalCancel=$('modalCancel');
var toastWrap=$('toastWrap');
var selectedFile=null,updAvailable=false,availVersion='',uiInit=false,offline=0,rebooting=false,lastLog='',modalResolve=null;
var lastSaved=null,saveTimer=null,op=null;

function clearToasts(){
  while(toastWrap.firstChild) toastWrap.removeChild(toastWrap.firstChild);
  op=null;
}
function toast(type,text){
  clearToasts();
  var el=document.createElement('div');
  el.className='toast '+type;el.textContent=text;
  toastWrap.appendChild(el);
  el.classList.add('show');
  scheduleClose(el,type);
  return el;
}
function scheduleClose(el,type){
  clearTimeout(el._t);
  var dur={info:3600,success:3600,error:4500,warn:4000}[type]||3600;
  el._t=setTimeout(function(){
    if(el.isConnected) el.remove();
  },dur);
}
function opShow(type,text){
  // In-progress toast: no auto-close timer; stays until the operation ends.
  if(op&&op.isConnected){op.className='toast '+type;op.textContent=text}
  else{
    clearToasts();
    op=document.createElement('div');
    op.className='toast '+type;op.textContent=text;
    toastWrap.appendChild(op);
    op.classList.add('show');
  }
}
function opResult(type,text){
  // Deterministic result: close the progress toast, show a brand-new toast with
  // its own full timer, so a result can never be cleared by a stale timer.
  closeOp();
  toast(type,text);
}
function closeOp(){
  if(op&&op.isConnected) op.remove();
  op=null;
}
function setLoading(btn,on){
  btn.classList.toggle('loading',on);
  btn.disabled=on?true:(rebooting?true:false);
}
function syncUpd(){
  if(rebooting)return;
  if(selectedFile){
    updBtn.disabled=false;
    updBtn.textContent='Update';
    updHint.textContent='Ready to upload '+selectedFile.name+'.';
  }
  else if(updAvailable){
    updBtn.disabled=false;
    updBtn.textContent=availVersion?('Update to '+availVersion):'Update available';
    updHint.textContent=availVersion?('Version '+availVersion+' is ready to install.'):'An update is ready to install.';
  }
  else{
    updBtn.disabled=true;
    updBtn.textContent='Update';
    updHint.textContent='Check GitHub for the latest release, or upload a .bin to install it manually.';
  }
}
function setPill(pill,state,label){pill.className='pill '+state;pill.querySelector('.pl').textContent=label}

sprayBtn.addEventListener('click',function(){
  setLoading(sprayBtn,true);opShow('info','Spraying…');
  fetch('/api/spray',{method:'POST'}).then(function(r){
    setLoading(sprayBtn,false);
    opResult(r.ok?'success':'error',r.ok?'Spray sent.':'Spray failed.');
  }).catch(function(){setLoading(sprayBtn,false);opResult('error','Cannot reach the device.')});
});

function clampPower(v){return Math.max(20,Math.min(255,v))}
function syncStepBtns(){
  powMinus.disabled=parseInt(motorPower.value,10)<=20;
  powPlus.disabled=parseInt(motorPower.value,10)>=255;
}
function applyPower(v){
  var p=clampPower(v);
  motorPower.value=p;
  motorPowerVal.textContent=p;
  syncStepBtns();
  pwrState.className='pwr';pwrState.textContent='';
  clearTimeout(saveTimer);
  saveTimer=setTimeout(savePower,1000);
}
motorPower.addEventListener('input',function(){applyPower(parseInt(motorPower.value,10))});
powMinus.addEventListener('click',function(){applyPower(parseInt(motorPower.value,10)-1)});
powPlus.addEventListener('click',function(){applyPower(parseInt(motorPower.value,10)+1)});
function savePower(){
  clearTimeout(saveTimer);
  var p=parseInt(motorPower.value,10);
  pwrState.className='pwr busy';pwrState.textContent='Saving…';
  fetch('/api/power',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({power:p})}).then(function(r){
    if(r.ok){
      lastSaved=p;
      pwrState.className='pwr ok';pwrState.textContent='Saved';
      setTimeout(function(){if(pwrState.textContent==='Saved'){pwrState.className='pwr';pwrState.textContent=''}},1800);
    }else{saveFail()}
  }).catch(function(){saveFail()});
}
function saveFail(){
  pwrState.className='pwr';pwrState.textContent='';
  if(lastSaved!==null){motorPower.value=lastSaved;motorPowerVal.textContent=lastSaved;syncStepBtns()}
  opResult('error','Could not save the spray force.');
}

chkBtn.addEventListener('click',function(){
  setLoading(chkBtn,true);opShow('info','Checking GitHub…');
  fetch('/api/check-update',{method:'POST'}).then(function(r){return r.text()}).then(function(t){
    setLoading(chkBtn,false);
    if(t.indexOf('AVAILABLE:')===0){availVersion=t.slice(10);updAvailable=true;opResult('success','Update '+availVersion+' is available.')}
    else if(t.indexOf('NO UPDATE')===0){updAvailable=false;availVersion='';opResult('success','You have the latest version.')}
    else{opResult('error',t||'Check failed.')}
    syncUpd();
  }).catch(function(){setLoading(chkBtn,false);opResult('error','Cannot reach the device.')});
});

updBtn.addEventListener('click',function(){
  if(rebooting)return;
  if(selectedFile){uploadFile();return}
  if(updAvailable){
    uiConfirm({title:'Install firmware',body:availVersion?('Install version '+availVersion+' from GitHub? The device will reboot after installing.'):'Install the available update from GitHub? The device will reboot after installing.',ok:'Install'}).then(function(ok){
      if(!ok)return;
      setLoading(updBtn,true);opShow('info','Starting update…');
      fetch('/api/update',{method:'POST'}).then(function(r){return r.text()}).then(function(t){
        setLoading(updBtn,false);syncUpd();
        if(t.indexOf('Update started')===0){enterReboot('Update started. Rebooting to install…')}
        else{opResult('error',t||'Update could not be started.')}
      }).catch(function(){setLoading(updBtn,false);syncUpd();opResult('error','Cannot reach the device.')});
    });
    return;
  }
  opResult('warn','Nothing to install yet — run a check or pick a .bin.');
});

function uploadFile(){
  setLoading(updBtn,true);opShow('info','Uploading '+selectedFile.name+'…');
  var fd=new FormData();fd.append('firmware',selectedFile,selectedFile.name);
  var xhr=new XMLHttpRequest();
  xhr.open('POST','/update');
  xhr.upload.onprogress=function(e){if(e.lengthComputable&&e.total){if(op&&op.isConnected){op.className='toast info';op.textContent='Uploading: '+Math.round(e.loaded/e.total*100)+'%'}}};
  xhr.onload=function(){
    setLoading(updBtn,false);syncUpd();
    if(xhr.status===200){enterReboot('Upload complete. Rebooting…')}
    else{opResult('error',xhr.responseText||('Upload failed ('+xhr.status+')'))}
  };
  xhr.onerror=function(){setLoading(updBtn,false);syncUpd();opResult('error','Upload failed — connection lost.')};
  xhr.onabort=function(){setLoading(updBtn,false);syncUpd();opResult('error','Upload aborted.')};
  xhr.send(fd);
}

function enterReboot(msg){
  rebooting=true;offlineBox.classList.remove('show');
  closeOp();
  stateBar.className='state-bar show';
  stateBar.innerHTML='<span class="spin"></span><span class="sb-msg"></span>';
  stateBar.querySelector('.sb-msg').textContent=msg;
  document.querySelectorAll('.btn').forEach(function(b){b.disabled=true});
  var tries=0;
  var t=setInterval(function(){
    tries++;
    fetch('/api/status').then(function(r){if(r.ok){clearInterval(t);location.reload()}}).catch(function(){});
    if(tries===12){stateBar.querySelector('.sb-msg').textContent='Still no response from the device. If it stays offline, power-cycle it — this page will keep trying.'}
  },5000);
}

resetBtn.addEventListener('click',function(){
  uiConfirm({title:'Reset settings',body:'Erase all settings (WiFi, MQTT, web credentials) and restart into setup mode? This cannot be undone.',ok:'Reset',danger:true}).then(function(ok){
    if(!ok)return;
    setLoading(resetBtn,true);opShow('info','Resetting…');
    fetch('/api/reset-config',{method:'POST'}).then(function(r){
      setLoading(resetBtn,false);
      if(r.ok){enterReboot('Settings erased. Rebooting to setup…')}
      else{opResult('error','Reset failed.')}
    }).catch(function(){setLoading(resetBtn,false);opResult('error','Cannot reach the device.')});
  });
});

function handleFile(f){
  if(!f||!f.name.toLowerCase().endsWith('.bin')){opResult('error','Only .bin files are allowed.');return}
  selectedFile=f;
  fileName.querySelector('.fn').textContent=f.name;
  dropZone.classList.add('has-file');
  opResult('success','Selected: '+f.name);
  syncUpd();
}
function clearFile(){selectedFile=null;dropZone.classList.remove('has-file');fileName.querySelector('.fn').textContent='';fileInput.value='';opResult('info','File removed.');syncUpd()}
dropZone.addEventListener('click',function(e){if(e.target.id==='fileClear')return;fileInput.click()});
fileClear.addEventListener('click',function(e){e.stopPropagation();clearFile()});
fileInput.addEventListener('change',function(){if(fileInput.files.length)handleFile(fileInput.files[0])});
dropZone.addEventListener('dragover',function(e){e.preventDefault();dropZone.classList.add('over')});
dropZone.addEventListener('dragleave',function(){dropZone.classList.remove('over')});
dropZone.addEventListener('drop',function(e){e.preventDefault();dropZone.classList.remove('over');if(e.dataTransfer.files.length)handleFile(e.dataTransfer.files[0])});
dropZone.addEventListener('keydown',function(e){if(e.key==='Enter'||e.key===' '){e.preventDefault();fileInput.click()}});

logClear.addEventListener('click',function(){
  fetch('/api/logs/clear',{method:'POST'}).catch(function(){});
  lastLog='';
  renderLogs('');
  toast('success','Logs cleared.');
});
function renderLogs(t){
  if(t&&t.trim()){logBox.innerHTML='';logBox.textContent=t}
  else{logBox.innerHTML='';var d=document.createElement('div');d.className='log-empty';d.textContent='No logs yet';logBox.appendChild(d)}
}
function refreshLogs(){
  fetch('/api/logs').then(function(r){return r.text()}).then(function(t){
    if(t===lastLog)return;
    lastLog=t;
    var atBottom=logBox.scrollHeight-logBox.scrollTop-logBox.clientHeight<30;
    renderLogs(t);
    if(atBottom)logBox.scrollTop=logBox.scrollHeight;
  }).catch(function(){});
}

function refreshStatus(){
  if(rebooting)return;
  fetch('/api/status').then(function(r){if(!r.ok)throw new Error('bad');return r.json()}).then(function(s){
    offline=0;offlineBox.classList.remove('show');
    setPill(stWifi,s.wifi?'ok':'bad',s.wifi?'WiFi · Online':'WiFi · Offline');
    setPill(stMqtt,s.mqtt?'ok':'bad',s.mqtt?'MQTT · Online':'MQTT · Offline');
    if(s.upd&&!updAvailable){updAvailable=true;if(s.uver)availVersion=s.uver;syncUpd()}
    if(!uiInit){motorPower.value=s.power;motorPowerVal.textContent=s.power;lastSaved=s.power;syncStepBtns();uiInit=true}
    ipInfo.textContent=(s.ip?'IP '+s.ip+' · ':'')+'ID '+s.id;
  }).catch(function(){
    offline++;
    if(offline>=2){
      offlineBox.classList.add('show');
      setPill(stWifi,'warn','WiFi · …');
      setPill(stMqtt,'warn','MQTT · …');
    }
  });
}
setInterval(refreshStatus,5000);setInterval(refreshLogs,3000);
refreshStatus();refreshLogs();

function uiConfirm(opts){
  return new Promise(function(resolve){
    modalResolve=resolve;
    modalTitle.textContent=opts.title||'Are you sure?';
    modalBody.textContent=opts.body||'';
    modalOk.textContent=opts.ok||'Confirm';
    modalBg.classList.add('open');
    modalOk.focus();
  });
}
function closeModal(v){modalBg.classList.remove('open');if(modalResolve){modalResolve(v);modalResolve=null}}
modalOk.addEventListener('click',function(){closeModal(true)});
modalCancel.addEventListener('click',function(){closeModal(false)});
modalBg.addEventListener('click',function(e){if(e.target===modalBg)closeModal(false)});
document.addEventListener('keydown',function(e){if(e.key==='Escape')closeModal(false)});
})();
</script>
</body>
</html>)ota";

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
              + ",\"uver\":\"" + updaterLatestVersion() + "\""
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
