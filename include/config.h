#ifndef CONFIG_H
#define CONFIG_H

#include "debuglog.h"

// ================= DEBUG =================
// DEBUG_PRINT* output goes to both Serial and the ring buffer (see debuglog)
// for viewing logs in the web interface (/api/logs).
#define DEBUG_MODE 1

#if DEBUG_MODE
  #define DEBUG_PRINT(x)    ::logPrint(String(x).c_str())
  #define DEBUG_PRINTLN(x)  ::logPrintln(String(x).c_str())
  #define DEBUG_PRINTF(...) ::logPrintf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// ================= FIRMWARE VERSION =================
// Compared with the GitHub release tag (e.g. "v3.0.0") for auto-update.
// When releasing a new version — update here AND create a release with the same tag.
#define FW_VERSION         "3.0.5"
#define FW_VERSION_PREFIX  "v"        // release tags are in "v3.0.0" format

// ================= DEVICE ID =================
// The technical device ID is derived automatically from the ESP8266 MAC
// address (see settings.h: deviceId()). It is stable in hardware and never
// changes across OTA updates, USB reflashes or a settings reset, so MQTT
// topics and Home Assistant entities are never recreated.
// The friendly "name" (user-editable, stored in settings) is used only for
// display (HA device name / web UI). Default when not set:
#define DEVICE_NAME_DEFAULT  "SmartSpray"

// ================= PINS =================
// Wemos D1 Mini / NodeMCU: D1 = GPIO5 (PWM), D2 = GPIO4 (DIG)
#define DIG_PIN D2
#define PWM_PIN D1

// ================= MOTOR =================
// Motor speed. Tuned by trial, since each air freshener has its own mechanical play.
// Find the minimum value from 0 to 255 that provides enough power to spray.
// The default (188) will likely work
const int MOTOR_POWER = 188;

// ================= WEB / OTA =================
#define AP_SSID           "SmartSpray Setup"   // access point name (for setup and AP fallback)
#define AP_IP             192, 168, 4, 1
#define AP_IP_STR         "192.168.4.1"
#define WEB_SERVER_PORT   80
#define OTA_PORT_HTTP     80

// ================= WEB (web interface access) =================
// Limits for web interface login/password (STA mode).
#define WEB_AUTH_USER_MAX  32   // max username length (incl. \0)
#define WEB_AUTH_PASS_MAX  64   // max password length (incl. \0)
#define WEB_AUTH_PASS_MIN  4    // min password length if auth is enabled

// ================= WIFI / AP FALLBACK =================
// If config exists but WiFi connection failed — raise AP fallback and
// scan the airwaves every WIFI_RETRY_MS looking for the configured network.
#define WIFI_CONNECT_TIMEOUT_MS  15000   // initial connection attempt (before AP fallback)
#define WIFI_RETRY_MS            30000   // network scan interval in AP fallback
// How many consecutive connection retries to attempt in AP fallback (network found but wrong password),
// after which give up and stay on the setup access point.
#define WIFI_RETRY_MAX_FAILURES  3

// ================= GITHUB (auto-update) =================
// Repository from which firmware is fetched for auto-update.
#define FW_REPO_OWNER  "AntennaNaStolbe"
#define FW_REPO_NAME   "SmartSpray"
#define FW_ASSET_NAME  "SmartSpray.bin"
#define FW_GITHUB_HOST "api.github.com"
#define FW_GITHUB_API  "https://" FW_GITHUB_HOST "/repos/" FW_REPO_OWNER "/" FW_REPO_NAME "/releases/latest"

// Auto-update check: once per day
#define UPDATE_CHECK_PERIOD_MS   (24UL * 60 * 60 * 1000)

#endif
