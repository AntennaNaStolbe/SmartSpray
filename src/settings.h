#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>

// Maximum field lengths (including trailing \0)
#define SETTINGS_WIFI_SSID_MAX  32
#define SETTINGS_WIFI_PASS_MAX  64
#define SETTINGS_MQTT_HOST_MAX  64
#define SETTINGS_MQTT_USER_MAX  32
#define SETTINGS_MQTT_PASS_MAX  64
#define SETTINGS_WEB_USER_MAX   32
#define SETTINGS_WEB_PASS_MAX   64
#define SETTINGS_WEB_PASS_MIN   4
#define SETTINGS_NAME_MAX       32

// Magic number to validate the saved config.
// VERSION: when AppSettings fields change, increment so old configs
// (with a different layout) are not read as valid.
#define SETTINGS_MAGIC  0x53505233   // "SPR3"

// Magic number of the previous AppSettings layout (before the "name" field
// was added). Used for automatic migration of the saved config without re-setup.
#define SETTINGS_MAGIC_PREV  0x53505232   // "SPR2"

// Magic number of the old AppSettings layout (before web auth was added).
// Used for automatic migration of the saved config without re-setup.
#define SETTINGS_MAGIC_OLD  0x53505231   // "SPR1"

// Default values
#define SETTINGS_MOTOR_POWER_DEFAULT      188
#define SETTINGS_MOTOR_POWER_MIN          20
#define SETTINGS_MOTOR_POWER_MAX          255
#define SETTINGS_AUTO_UPDATE_DEFAULT      1

typedef struct {
  uint32_t magic;
  char wifi_ssid[SETTINGS_WIFI_SSID_MAX];
  char wifi_pass[SETTINGS_WIFI_PASS_MAX];
  char mqtt_host[SETTINGS_MQTT_HOST_MAX];
  uint16_t mqtt_port;
  char mqtt_user[SETTINGS_MQTT_USER_MAX];
  char mqtt_pass[SETTINGS_MQTT_PASS_MAX];
  uint8_t motor_power;        // 0..255 (see SETTINGS_MOTOR_POWER_*)
  uint8_t auto_update_enabled; // 0/1
  char web_user[SETTINGS_WEB_USER_MAX];  // web interface username (when web_auth_enabled)
  char web_pass[SETTINGS_WEB_PASS_MAX];  // web interface password (when web_auth_enabled)
  uint8_t web_auth_enabled;   // 0/1 - require password for the STA web interface
  char name[SETTINGS_NAME_MAX];  // friendly device name for display (HA device name / web UI)
} AppSettings;

// EEPROM init (called once in setup)
void settingsInit();

// Current config loaded into memory (from settingsLoad/settingsApply).
// Main source for main/webserver; saved when changed.
extern AppSettings gSettings;

// Is there a saved valid config (SSID and MQTT host set)?
bool settingsValid();

// Load config from EEPROM. Returns true on success.
bool settingsLoad(AppSettings &s);

// Save config to EEPROM. Returns true on success.
bool settingsSave(const AppSettings &s);

// Clear the config (reset to setup mode).
void settingsReset();

// Technical device ID derived from the ESP8266 MAC address. Stable in
// hardware: never changes across OTA, USB reflash or settings reset.
// Used for MQTT topics, client_id and HA unique_ids/identifiers.
String deviceId();

// Friendly display name (from settings), falling back to DEVICE_NAME_DEFAULT.
const char* deviceName();

#endif
