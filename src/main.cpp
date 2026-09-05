#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "config.h"
#include "settings.h"
#include "mqtt.h"
#include "motor.h"
#include "webserver.h"
#include "updater.h"

// Device operating modes
enum DeviceMode {
  MODE_SETUP,   // no config — access point for setup
  MODE_AP_STA,  // config exists but WiFi not connected: AP fallback + periodic network check
  MODE_STA,     // config exists, WiFi connected: working mode (MQTT + OTA interface)
};

static DeviceMode deviceMode = MODE_SETUP;
static AppSettings savedCfg;
static unsigned long lastUpdateCheck = 0;
static unsigned long lastWifiRetry = 0;
static bool mqttWarnShown = false;
static int wifiConnFailCount = 0;   // consecutive failed connections in AP fallback

// Attempt to connect to the network from config. Blocking, but with a timeout.
static bool tryConnectSTA(const AppSettings &s, unsigned long timeoutMs) {
  DEBUG_PRINTF("[WIFI] Connecting to %s\n", s.wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(s.wifi_ssid, s.wifi_pass);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(200);
    yield();
  }

  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("[WIFI] Connected");
    DEBUG_PRINT("[WIFI] IP: ");
    DEBUG_PRINTLN(WiFi.localIP().toString());
    return true;
  }

  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("[WIFI] Connection failed (no network or wrong credentials)");
  return false;
}

// Switch to working mode: disable AP, start MQTT and OTA interface.
static void enterStaMode(const AppSettings &s) {
  WiFi.enableAP(false);
  WiFi.mode(WIFI_STA);
  delay(100);

  deviceMode = MODE_STA;
  mqttBegin(s);
  webInitSta();
  lastUpdateCheck = millis();
  DEBUG_PRINTLN("[BOOT] Ready, FW v" FW_VERSION);
}

// AP fallback: config exists but WiFi not connected.
// Start an access point (to fix settings) + periodically check the network.
static void enterApConnectMode(const AppSettings &s) {
  deviceMode = MODE_AP_STA;
  savedCfg = s;
  wifiConnFailCount = 0;

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID);
  delay(100);
  webInitAp();
  lastWifiRetry = millis();

  DEBUG_PRINTLN("[WIFI] WiFi unavailable - AP fallback " AP_SSID " enabled");
  DEBUG_PRINTLN("[WIFI] Connect to this network to set up, or wait for return to your network");
}

// In AP fallback: scan the airwaves every WIFI_RETRY_MS looking for the configured network.
// If the network is found but connection fails (e.g. wrong password) —
// after WIFI_RETRY_MAX_FAILURES attempts we give up and stay on the setup AP.
static void handleApStaScan() {
  if (wifiConnFailCount >= WIFI_RETRY_MAX_FAILURES) return;

  if (millis() - lastWifiRetry < WIFI_RETRY_MS) return;
  lastWifiRetry = millis();

  if (WiFi.status() == WL_CONNECTED) {
    DEBUG_PRINTLN("[WIFI] Network available again, returning to working mode");
    wifiConnFailCount = 0;
    enterStaMode(savedCfg);
    return;
  }

  DEBUG_PRINTLN("[WIFI] Scanning networks...");
  int n = WiFi.scanNetworks();
  bool found = false;
  for (int i = 0; i < n; i++) {
    if (WiFi.SSID(i) == String(savedCfg.wifi_ssid)) {
      found = true;
      break;
    }
  }
  WiFi.scanDelete();

  if (found) {
    DEBUG_PRINTLN("[WIFI] Configured network found, connecting...");
    if (tryConnectSTA(savedCfg, WIFI_CONNECT_TIMEOUT_MS)) {
      wifiConnFailCount = 0;
      enterStaMode(savedCfg);
    } else {
      wifiConnFailCount++;
      DEBUG_PRINTF("[WIFI] Connection failed (%d/%d). Maybe wrong password.\n",
                   wifiConnFailCount, WIFI_RETRY_MAX_FAILURES);
      if (wifiConnFailCount >= WIFI_RETRY_MAX_FAILURES) {
        // giving up: stay on the setup access point, disable STA for AP reliability
        WiFi.mode(WIFI_AP);
        delay(100);
        DEBUG_PRINTLN("[WIFI] Attempt limit reached - staying on AP " AP_SSID
                      " http://" AP_IP_STR " (fix the settings)");
        // access point already running (enterApConnectMode), settings web page is served
      }
    }
  } else {
    DEBUG_PRINTF("[WIFI] Network not found yet, retry in %lu ms\n",
                 (unsigned long)WIFI_RETRY_MS);
  }
}

void setup() {
  Serial.begin(115200);
  DEBUG_PRINTLN("\n[BOOT] SmartSpray starting");
  DEBUG_PRINTF("[BOOT] Last reset reason: %s\n", ESP.getResetReason().c_str());

  analogWriteRange(255);   // 8-bit PWM for GyverMotor2
  analogWriteFreq(20000);  // high frequency to avoid audible whine

  motorInit();             // motor in "off" state
  settingsInit();
  updaterInit();

  AppSettings s;
  if (settingsLoad(s)) {
    gSettings = s;
    motorSetPower(s.motor_power);
    if (tryConnectSTA(s, WIFI_CONNECT_TIMEOUT_MS)) {
      enterStaMode(s);
    } else {
      enterApConnectMode(s);
    }
  } else {
    // initial setup — no config
    deviceMode = MODE_SETUP;
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID);
    delay(100);
    webInitAp();
    DEBUG_PRINTLN("[BOOT] AP: " AP_SSID " http://" AP_IP_STR);
  }
}

void loop() {
  // Deferred firmware install (web/MQTT set a flag; the actual download runs
  // here, after the web response is sent, in a quiet context).
  updaterRunInstall();

  webLoop();               // handles HTTP (and DNS in AP/AP_STA modes)

  if (deviceMode == MODE_STA) {
    mqttLoop();

    // Hint if MQTT is unavailable for a while (e.g. broker starting up or wrong credentials)
    if (mqttConnected()) {
      mqttWarnShown = false;
    } else if (!mqttWarnShown) {
      mqttWarnShown = true;
      DEBUG_PRINT("[WARN] MQTT unavailable. If the data is wrong - open http://");
      DEBUG_PRINT(WiFi.localIP().toString());
      DEBUG_PRINTLN(" and press Reset Settings");
    }

    // Auto-check for updates once a day.
    // Check only — installation is performed by an explicit command (web/MQTT).
    if (WiFi.status() == WL_CONNECTED
        && (millis() - lastUpdateCheck) >= UPDATE_CHECK_PERIOD_MS) {
      lastUpdateCheck = millis();
      updaterCheck();
      mqttPublishDeviceInfo();
    }

  } else if (deviceMode == MODE_AP_STA) {
    handleApStaScan();
  }
}
