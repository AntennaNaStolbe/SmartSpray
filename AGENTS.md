# SmartSpray — Agent Guide

## Project

Smart air freshener for Home Assistant. ESP8266 (Wemos D1 Mini), firmware in C++ (Arduino framework) via PlatformIO. Motor control over MQTT + HA Discovery.

## Build & Flash

```bash
# Full build
pio run

# Build + upload over USB
pio run -e d1_mini -t upload

# Serial monitor
pio device monitor
```

## Boards

| Environment | Board | Note |
|-------------|-------|------|
| `d1_mini` | Wemos D1 Mini | The only supported environment |

NodeMCU support was removed; the code targets the Wemos D1 Mini.

## Pins

| Function | ESP8266 pin |
|----------|-------------|
| DIG_PIN (driver) | D2 = GPIO4 |
| PWM_PIN (driver) | D1 = GPIO5 |

## Project Structure

| File | Purpose |
|------|---------|
| `platformio.ini` | PlatformIO config (espressif8266, arduino, 1 environment) |
| `include/config.h` | Parameters: pins, motor power, FW_VERSION, AP_SSID, GitHub auto-update, DEBUG (routing to debuglog) |
| `src/main.cpp` | Entry point: AP/STA mode, WiFi connection, loop, auto-update timer |
| `src/settings.h/.cpp` | EEPROM config (WiFi/MQTT + motor_power + auto_update_enabled + web_auth_enabled/web_user/web_pass + friendly name), save/load/reset/valid, global `gSettings`, EEPROM auto-migration on load, `deviceId()` (MAC-based stable ID) / `deviceName()` |
| `src/debuglog.h/.cpp` | Ring buffer (40 lines x 100) of latest DEBUG messages; DEBUG_PRINT* macros duplicate output to Serial and buffer (for web log). Size tuned to limited heap (TLS/HTTPS check to GitHub) |
| `src/mqtt.h/.cpp` | MQTT + HA Discovery, connect/reconnect, callback (SPRAY/CHECK/UPDATE/STATUS) |
| `src/motor.h/.cpp` | GyverMotor2 + spray() (smooth motor ramp-up), configurable motorSetPower/motorGetPower |
| `src/webserver.h/.cpp` | ESP8266WebServer: AP config (captive portal), OTA page, force slider, Spray/Check/Update/Reset buttons, logs window, HTTP Basic auth (web auth) |
| `src/updater.h/.cpp` | OTA (web .bin upload via Update) + GitHub auto-update (updaterCheck/updaterInstall separate) |

## Web / OTA

- **Setup mode (AP):** when no config in EEPROM — access point `SmartSpray Setup`, captive portal, WiFi/MQTT config page at `http://192.168.4.1`
- **Working mode (STA):** control page at `http://<ip-device>/` (top to bottom):
  - WiFi/MQTT status (`GET /api/status`), auto-refreshes every 5 s
  - "Spray" button (`POST /api/spray`)
  - Spray-force slider + "Save" (`POST /api/power`, JSON `{"power":N}`, min 20 / max 255, default 188)
  - "Check for updates" button (`POST /api/check-update`)
  - Drag & drop `.bin` file zone
  - **Single "Update" button** (`#updBtn`) — gray by default, turns green if **a file is attached OR a GitHub update was found**. Click logic: file selected → multipart upload to `POST /update`; else if update → `POST /api/update`
  - **Logs** window (`GET /api/logs`, clear `POST /api/logs/clear`) — latest DEBUG messages (ring buffer 40 lines), live-refreshed every 3 s
  - "Reset settings" button → erase EEPROM and reboot to AP mode
- **Web auth:** all STA endpoints + the `/update` upload handler are gated by HTTP Basic auth when enabled (`web_auth_enabled`). Auth check happens BEFORE `Update.begin()`. Default off. Configured on the AP setup page (username + password ≥ 4 chars).

## OTA upload: important nuance (long-suffered fix)

- In `webserver.cpp` handler `wbUpdateUpload` uses `Update.begin(ESP.getFreeSketchSpace()...)` (not `up.totalSize` — in multipart that's the HTTP request size, not the file size).
- **Send firmware via OTA ONLY as `multipart/form-data`** (`FormData`, field `firmware`), NOT as raw `application/octet-stream`. ESP8266WebServer reads raw-POST into `plainBuf` (String) entirely in `_parseRequest` (Parsing-impl.h) → large files don't fit in heap / aren't read within the timeout → connection drops (~148KB), the upload handler is **never invoked**. Multipart is streamed in 4KB chunks via the upload callback — works.

## Resilience to an unavailable network

- **Config exists, but WiFi unavailable** (router boots slower than the device) → AP fallback: `SmartSpray Setup` is brought up (WIFI_AP_STA mode), every `WIFI_RETRY_MS` (30 s) the airwaves are scanned (`WiFi.scanNetworks()`) for the configured SSID; once found — connect and return to working mode. If network found but connect fails (e.g. wrong password) — after `WIFI_RETRY_MAX_FAILURES` (3) attempts give up, disable STA and stay on AP config; counter resets on reboot/re-config
- **No config** → pure AP setup mode (no scanning)
- **MQTT unavailable** → non-blocking reconnect (`mqttLoop()` at most once per 5 s, doesn't block webLoop); web page always responds
- Parameters in `include/config.h`: `WIFI_CONNECT_TIMEOUT_MS`, `WIFI_RETRY_MS`

## GitHub auto-update (check on GitHub, download over plain HTTP)

- Firmware version — `FW_VERSION` in `include/config.h`
- Device compares `FW_VERSION` against the latest release tag in `AntennaNaStolbe/SmartSpray` (`api.github.com/.../releases/latest`), looks for asset `SmartSpray.bin`
- **Check and install are separate**: `updaterCheck()` only checks (remembers the available version), `updaterInstall()` installs. Install only on an explicit command (`#updBtn` in web / MQTT command `UPDATE`)
- Daily check once a day (if auto-updates enabled in settings) + on button
- HTTPS to GitHub (check only) via `WiFiClientSecure.setInsecure()` with an 8KB TLS recv buffer — small responses fit fine
- **The firmware BYTES are downloaded from `gSettings.update_url` (plain `http://`, set on the AP setup page) — NOT from GitHub's CDN.** Reason (hard limit, verified): `release-assets.githubusercontent.com` sends full-size (~16.4KB) TLS records, but BearSSL needs a recv buffer ≥ record size; a 16KB buffer doesn't fit the ESP8266 heap together with the 6.2KB BearSSL thunk stack (`StackThunk.cpp`), and any smaller buffer stalls on the oversized records. Plain HTTP has no such limit (tiny heap, no record-size constraint). `update_url` empty ⇒ auto-install disabled (`/api/update` returns an error) and the web drag&drop OTA is used instead. Keep `SmartSpray.bin` current on that host when releasing.
- When `update_url` is set, `/api/update` sets a flag; the actual download runs from `loop()` (`updaterRunInstall()`): web server is closed (`webStop()`), MQTT disconnected, `ESPhttpUpdate.update(WiFiClient, url)` streams into flash, device reboots on success; on failure `mqttBegin(webInitSta)` restore the working mode
- To release an update: bump `FW_VERSION`, build `pio run`, upload `SmartSpray.bin` to a GitHub Release tagged `v<FW_VERSION>`, and update the file served at `update_url`

## MQTT Topics (base `antennans/SmartSpray/<device_id>/…`)

- `<device_id>` is the **technical device ID**, derived automatically from the ESP8266 MAC address (lowercased, colons stripped) via `deviceId()` in `src/settings.cpp`. It is stable in hardware — never changes across OTA, USB reflash or a settings reset — so MQTT topics and HA `unique_id`s are never recreated.
- The **friendly display name** is `gSettings.name` (deviceName() fallback to `DEVICE_NAME_DEFAULT`); used only for the HA device name and web UI title. Stored in EEPROM (resettable), set on the AP setup page ("Device name", prefilled with the default).

- `trigger` — commands: `SPRAY`, `CHECK` (check updates + republish status), `UPDATE` (install found update), `STATUS` (republish status)
- `status` — JSON status (`version`, `update_available`, `online`)
- `availability` — LWT online/offline
- `version` — retained, current version (HA sensor)
- `update_available` — retained, `true`/`false` (HA binary sensor)
- HA Discovery (5 entities in one device): `homeassistant/button/..._trigger/config` (Spraying → SPRAY), `homeassistant/button/..._check/config` (Check updates → CHECK), `homeassistant/button/..._update/config` (Update → UPDATE, published only when an update is found), `homeassistant/sensor/..._version/config`, `homeassistant/binary_sensor/..._update_available/config`

## Material

- Motor driver: TA6586
- Motor: from the AirWick automatic freshener

## Workflow

### Commits
- Write in English, short (what changed and why)
- Format: `action: short description` (examples: `add: X support`, `fix: ...`, `update: README`)
- Don't commit .DS_Store, .pio/, secrets
- Don't commit without an explicit user request

### Push
- Push only on user request
- Remote: `origin`
- Base branch: `main`
