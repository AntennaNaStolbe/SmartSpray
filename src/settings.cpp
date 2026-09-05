#include "settings.h"
#include "config.h"
#include <ESP8266WiFi.h>
#include <EEPROM.h>

// EEPROM range for config (ESP8266 emulates up to 4KB).
#define EEPROM_SIZE      1024
#define EEPROM_CONFIG_OFFSET  0

static bool eepromReady = false;

AppSettings gSettings;   // current config in memory

// Old AppSettings layout (before web auth was added), magic = SETTINGS_MAGIC_OLD.
struct AppSettingsOld {
  uint32_t magic;
  char wifi_ssid[SETTINGS_WIFI_SSID_MAX];
  char wifi_pass[SETTINGS_WIFI_PASS_MAX];
  char mqtt_host[SETTINGS_MQTT_HOST_MAX];
  uint16_t mqtt_port;
  char mqtt_user[SETTINGS_MQTT_USER_MAX];
  char mqtt_pass[SETTINGS_MQTT_PASS_MAX];
  uint8_t motor_power;
  uint8_t auto_update_enabled;
};

// Previous AppSettings layout (before the "name" field was added),
// magic = SETTINGS_MAGIC_PREV.
struct AppSettingsPrev {
  uint32_t magic;
  char wifi_ssid[SETTINGS_WIFI_SSID_MAX];
  char wifi_pass[SETTINGS_WIFI_PASS_MAX];
  char mqtt_host[SETTINGS_MQTT_HOST_MAX];
  uint16_t mqtt_port;
  char mqtt_user[SETTINGS_MQTT_USER_MAX];
  char mqtt_pass[SETTINGS_MQTT_PASS_MAX];
  uint8_t motor_power;
  uint8_t auto_update_enabled;
  char web_user[SETTINGS_WEB_USER_MAX];
  char web_pass[SETTINGS_WEB_PASS_MAX];
  uint8_t web_auth_enabled;
};

// Previous AppSettings layout (before the "update_url" field was added),
// magic = SETTINGS_MAGIC_PREV2.
struct AppSettingsPrev2 {
  uint32_t magic;
  char wifi_ssid[SETTINGS_WIFI_SSID_MAX];
  char wifi_pass[SETTINGS_WIFI_PASS_MAX];
  char mqtt_host[SETTINGS_MQTT_HOST_MAX];
  uint16_t mqtt_port;
  char mqtt_user[SETTINGS_MQTT_USER_MAX];
  char mqtt_pass[SETTINGS_MQTT_PASS_MAX];
  uint8_t motor_power;
  uint8_t auto_update_enabled;
  char web_user[SETTINGS_WEB_USER_MAX];
  char web_pass[SETTINGS_WEB_PASS_MAX];
  uint8_t web_auth_enabled;
  char name[SETTINGS_NAME_MAX];
};

void settingsInit() {
  if (!eepromReady) {
    EEPROM.begin(EEPROM_SIZE);
    eepromReady = true;
  }
}

bool settingsValid() {
  AppSettings s;
  return settingsLoad(s);
}

// Clamps values to allowed ranges.
static void settingsNormalize(AppSettings &s) {
  if (s.mqtt_port == 0) s.mqtt_port = 1883;
  if (s.motor_power < SETTINGS_MOTOR_POWER_MIN || s.motor_power > SETTINGS_MOTOR_POWER_MAX)
    s.motor_power = SETTINGS_MOTOR_POWER_DEFAULT;
  if (s.auto_update_enabled > 1) s.auto_update_enabled = SETTINGS_AUTO_UPDATE_DEFAULT;
  // Null-terminate text fields (in case old/corrupt data is missing \0 at the end)
  s.wifi_ssid[SETTINGS_WIFI_SSID_MAX - 1] = '\0';
  s.wifi_pass[SETTINGS_WIFI_PASS_MAX - 1] = '\0';
  s.mqtt_host[SETTINGS_MQTT_HOST_MAX - 1] = '\0';
  s.mqtt_user[SETTINGS_MQTT_USER_MAX - 1] = '\0';
  s.mqtt_pass[SETTINGS_MQTT_PASS_MAX - 1] = '\0';
  // Web auth: if enabled but data is missing/incomplete — disable it.
  if (s.web_auth_enabled > 1) s.web_auth_enabled = 0;
  s.web_user[SETTINGS_WEB_USER_MAX - 1] = '\0';
  s.web_pass[SETTINGS_WEB_PASS_MAX - 1] = '\0';
  if (s.web_auth_enabled && (s.web_user[0] == '\0'
      || strlen(s.web_pass) < SETTINGS_WEB_PASS_MIN)) {
    s.web_auth_enabled = 0;
  }
  // Null-terminate the friendly name and seed the default if empty.
  s.name[SETTINGS_NAME_MAX - 1] = '\0';
  if (s.name[0] == '\0') {
    strncpy(s.name, DEVICE_NAME_DEFAULT, SETTINGS_NAME_MAX - 1);
  }
  // Null-terminate the update URL (empty = manual web OTA only).
  s.update_url[SETTINGS_UPDATE_URL_MAX - 1] = '\0';
}

bool settingsLoad(AppSettings &s) {
  if (!eepromReady) return false;

  // Try the current layout first.
  EEPROM.get(EEPROM_CONFIG_OFFSET, s);

  // Old config (before web auth): migrate, preserving settings.
  if (s.magic == SETTINGS_MAGIC_OLD) {
    AppSettingsOld old;
    EEPROM.get(EEPROM_CONFIG_OFFSET, old);
    memset(&s, 0, sizeof(s));
    s.magic = SETTINGS_MAGIC;
    strncpy(s.wifi_ssid, old.wifi_ssid, SETTINGS_WIFI_SSID_MAX - 1);
    strncpy(s.wifi_pass, old.wifi_pass, SETTINGS_WIFI_PASS_MAX - 1);
    strncpy(s.mqtt_host, old.mqtt_host, SETTINGS_MQTT_HOST_MAX - 1);
    s.mqtt_port = old.mqtt_port;
    strncpy(s.mqtt_user, old.mqtt_user, SETTINGS_MQTT_USER_MAX - 1);
    strncpy(s.mqtt_pass, old.mqtt_pass, SETTINGS_MQTT_PASS_MAX - 1);
    s.motor_power = old.motor_power;
    s.auto_update_enabled = old.auto_update_enabled;
    // web auth is disabled by default
    s.web_auth_enabled = 0;
    s.web_user[0] = '\0';
    s.web_pass[0] = '\0';
    // Save immediately in the new layout.
    settingsSave(s);
  }

  // Previous config (before the "name" field): migrate, preserving everything,
  // including web auth. The friendly name is seeded by settingsNormalize.
  if (s.magic == SETTINGS_MAGIC_PREV) {
    AppSettingsPrev prev;
    EEPROM.get(EEPROM_CONFIG_OFFSET, prev);
    s.magic = SETTINGS_MAGIC;
    s.name[0] = '\0';   // back to default
    s.update_url[0] = '\0';
    // All other fields are at identical offsets; keep them as-is.
    // Save immediately in the new layout.
    settingsSave(s);
  }

  // Previous config (before the "update_url" field): migrate, preserving
  // everything. The update URL is empty by default (web OTA only).
  if (s.magic == SETTINGS_MAGIC_PREV2) {
    AppSettingsPrev2 prev;
    EEPROM.get(EEPROM_CONFIG_OFFSET, prev);
    s.magic = SETTINGS_MAGIC;
    s.update_url[0] = '\0';
    // All other fields are at identical offsets; keep them as-is.
    settingsSave(s);
  }

  if (s.magic != SETTINGS_MAGIC) {
    memset(&s, 0, sizeof(s));
    return false;
  }
  if (s.wifi_ssid[0] == '\0' || s.mqtt_host[0] == '\0') {
    return false;
  }
  settingsNormalize(s);
  return true;
}

bool settingsSave(const AppSettings &s) {
  if (!eepromReady) return false;

  AppSettings copy = s;
  copy.magic = SETTINGS_MAGIC;
  settingsNormalize(copy);

  EEPROM.put(EEPROM_CONFIG_OFFSET, copy);
  return EEPROM.commit();
}

void settingsReset() {
  if (!eepromReady) return;
  for (int i = 0; i < EEPROM_SIZE; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

// Technical device ID derived from the ESP8266 MAC address.
// Lowercase hex, colons replaced with '-' (MQTT/HA friendly), e.g. "84f3eb-2a1c00".
// Stable in hardware: never changes across OTA, USB reflash or settings reset.
String deviceId() {
  String mac = WiFi.macAddress();   // "84:F3:EB:2A:1C:00"
  mac.toLowerCase();
  mac.replace(":", "");
  return mac;
}

const char* deviceName() {
  return gSettings.name[0] != '\0' ? gSettings.name : DEVICE_NAME_DEFAULT;
}
