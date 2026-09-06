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

- Both pages (`HTML_CONFIG` AP, `HTML_OTA` STA) are **raw string literals** `R"cfg(...)cfg"` / `R"ota(...)ota"` in `src/webserver.cpp`. Same dark design-token palette (`:root` CSS vars: `--bg #101419`, `--srf #161c24`, accent `--acc #6d8cff`, ok `#43d69a`, warn `#f2b544`, err `#f5646c`), shared button/message/card styles, responsive (`<=520px`).
- **Preprocessor macros live OUTSIDE the raw literals** (they don't expand inside `R"..."`): the literals are spliced at each macro point, e.g. `R"cfg(<title>)cfg" AP_SSID R"cfg(</title>)cfg"`. Markers: `AP_SSID` (title), `DEVICE_NAME_DEFAULT` (name field), `FW_VERSION` (OTA subtitle). Keep server-side replace markers `<title>SmartSpray OTA</title>` and `<h1 id="devName">SmartSpray</h1>` intact (`wbOtaPage` swaps in `deviceName()`).
- **Setup mode (AP):** when no config in EEPROM — access point `SmartSpray Setup`, captive portal, WiFi/MQTT config page at `http://192.168.4.1` (single card: Device name, WiFi, MQTT, "Web Interface Access" switch **"Username and password for the web interface"** that enables/disables the user/pass fields; firmware-update source field was removed — updates always come from GitHub; client-side validation marks `.error` fields; `POST /api/config` then reboots, so `update_url` is always cleared to ""). Footer explains the web UI will be reachable at the device's IP on the local network.
- **Working mode (STA):** control page at `http://<ip-device>/` (top to bottom):
  - Header: device name, `Firmware v<FW_VERSION>`, WiFi/MQTT status **pills** (with dot, ok/bad) from `GET /api/status`, auto-refreshes every 5 s
  - Offline banner shown after 2 consecutive failed `/api/status` polls
  - **"Spray" card**: Spray button (`POST /api/spray`) + force slider with **live value + auto-save** (`POST /api/power`, JSON `{"power":N}`, min 20 / max 255, default 188) and **− / + stepper buttons** (`#powMinus`/`#powPlus`, step 1, disabled at the bounds) right of the slider for precise tuning — no Save button; saving fires on a ~1 s debounce after the last change, with a small inline "Saved" state and a toast on failure
  - **"Firmware update" card**: "Check GitHub for updates" (`POST /api/check-update`, GitHub octocat icon), an **OR** divider, drag & drop `.bin` zone, and a **single "Update" button** (`#updBtn`). Button text reflects state (after `syncUpd()`): `Update to vX.Y.Z` / `Update available` when a GitHub update was found (`/api/status` also reports it as `upd` + `uver`), plain `Update` when a `.bin` is attached, gray `Update` (disabled) otherwise. Click logic: file → multipart upload to `POST /update` (XHR with progress %); update → confirm modal then `POST /api/update`
  - **"Logs" card** (`GET /api/logs`, clear `POST /api/logs/clear`), live-refreshed every 3 s, empty-state text
  - **"Maintenance" card**: "Reset settings" (danger style, confirm modal) → erase EEPROM and reboot to AP mode
  - Footer: `IP <ip> · ID <device_id>`
- **Notifications:** transient feedback uses **toasts** (top-center in `#toastWrap`; a **single slot** — every new toast instantly clears all previous ones and appears in the same fixed spot, so the stack never grows; auto-dismiss: info/success ~3.6 s, warn ~4 s, error ~4.5 s; no fade-away, removal is instant). An in-flight toast (`opShow`, no auto-close) sticks while an operation runs; `opResult` closes it and shows a **fresh** toast with its own full timer, so a result can never be cleared by a stale timer. Toasts replace per-card message boxes. Long reboot flows (upload/update/reset) switch into `enterReboot()`: a persistent **state bar** (`#stateBar`, spinner) shows the reason, all buttons disable, and the page polls `/api/status` every 5 s and auto-reloads when the device is back.
- **Web auth:** all STA endpoints + the `/update` upload handler are gated by HTTP Basic auth when enabled (`web_auth_enabled`). Auth check happens BEFORE `Update.begin()`. Default off. Configured on the AP setup page (username + password ≥ 4 chars).

## OTA upload: important nuance (long-suffered fix)

- In `webserver.cpp` handler `wbUpdateUpload` uses `Update.begin(ESP.getFreeSketchSpace()...)` (not `up.totalSize` — in multipart that's the HTTP request size, not the file size).
- **Send firmware via OTA ONLY as `multipart/form-data`** (`FormData`, field `firmware`), NOT as raw `application/octet-stream`. ESP8266WebServer reads raw-POST into `plainBuf` (String) entirely in `_parseRequest` (Parsing-impl.h) → large files don't fit in heap / aren't read within the timeout → connection drops (~148KB), the upload handler is **never invoked**. Multipart is streamed in 4KB chunks via the upload callback — works.

## Resilience to an unavailable network

- **Config exists, but WiFi unavailable** (router boots slower than the device) → AP fallback: `SmartSpray Setup` is brought up (WIFI_AP_STA mode), every `WIFI_RETRY_MS` (30 s) the airwaves are scanned (`WiFi.scanNetworks()`) for the configured SSID; once found — connect and return to working mode. If network found but connect fails (e.g. wrong password) — after `WIFI_RETRY_MAX_FAILURES` (3) attempts give up, disable STA and stay on AP config; counter resets on reboot/re-config
- **No config** → pure AP setup mode (no scanning)
- **MQTT unavailable** → reconnect is bounded and spaced out: `MQTT_RECONNECT_INTERVAL_MS` (20 s) between attempts, each attempt capped by `MQTT_CONNECT_TIMEOUT_MS` (3 s, DNS + TCP via `espClient.setTimeout()` + CONNACK via `PubSubClient::setSocketTimeout()`), so the loop — and the web page — never stalls when the broker is unreachable
- Parameters in `include/config.h`: `WIFI_CONNECT_TIMEOUT_MS`, `WIFI_RETRY_MS`, `MQTT_CONNECT_TIMEOUT_MS`, `MQTT_RECONNECT_INTERVAL_MS`

## GitHub auto-update (check + download straight from GitHub over TLS)

- Firmware version — `FW_VERSION` in `include/config.h`
- Device compares `FW_VERSION` against the latest release tag in `AntennaNaStolbe/SmartSpray` (`api.github.com/.../releases/latest`), looks for asset `SmartSpray.bin`
- **Check and install are separate**: `updaterCheck()` only checks (remembers the available version), `updaterInstall()` installs. Install only on an explicit command (`#updBtn` in web / MQTT command `UPDATE`)
- Daily check once a day (if auto-updates enabled in settings) + on button
- HTTPS to GitHub (check only) via `WiFiClientSecure.setInsecure()` with an 8KB TLS recv buffer — small responses fit fine
- **The firmware BYTES are downloaded directly from the GitHub release asset over TLS** — no user-configured update URL. The user only presses Update. Full pipeline verified E2E (v3.0.6 release, self-update → v3.0.7 test):
  - Install runs on the **clean boot heap**: `/api/update` persists `update_pending` + `update_asset_url` to EEPROM, reboots (`updaterRunInstall()`), and at boot `updaterRunPendingOnBoot()` (before web/MQTT start) installs. Flag cleared first so a failed install can't reboot-loop — after a failure the device continues to normal STA mode.
  - One `WiFiClientSecure` with the **maximum BearSSL receive buffer** (`setBufferSizes(16384,512)` = ~16.7KB; the CDN sends full-size ~16.4KB TLS records, smaller stalls "Stream Read Timeout") is reused for both hops. The 6.2KB BearSSL thunk stack (`StackThunk.cpp`) only fits because the install is deferred (web + MQTT stopped → monolithic heap).
  - **github.com redirect handled MANUALLY** — ESP8266httpUpdate does NOT follow the release 302 (aborts with `HTTP_UE_SERVER_FILE_NOT_FOUND` -104). Our loop: hop1 `GET browser_download_url` → 302 (`getLocation()`, `HTTPC_DISABLE_FOLLOW_REDIRECTS`), hop2 reconnects the SAME client to the CDN and `GET`s the asset. hop1 is `h.end()`-ed before hop2 connects, so the single 16.7KB buffer rotates in one heap region — no second big buffer, no separate CDN-URL resolution step.
  - **Streaming is manual**, NOT `HTTPClient::writeToStream` (its internal `sendSize` can block indefinitely on a stalled TLS read). Our loop reads plaintext via `getStreamPtr()` → `available()`/`read()` with `tlsClient.setTimeout(8)` and feeds `UpdateStream` (`Update.write()`), guarded by `h.connected() && total < h.getSize()`. Retries (3×) rebuild the client so heap is fully released.
  - ESP8266httpUpdate (and `webStop()` + defrag + pre-flight handshake from earlier iterations) is now the **plain-HTTP `update_url` override path only**; the GitHub path is the manual hop2 stream above. `githubDirectAssetUrl()`, the two-pass defrag and the pre-flight handshake were removed.
- `gSettings.update_url` remains as an optional **plain-HTTP (http://) override** (kept for local installs); default empty = GitHub-direct
- `/api/update` sets a flag + persists; the actual download runs from `loop()` (`updaterRunInstall()`); on success the new firmware reboots on its own, on failure the device works on normally (`mqttBegin(webInitSta)` restores the working mode)
- To release an update: bump `FW_VERSION`, build `pio run`, upload the firmware **named `SmartSpray.bin`** (gh's `file#label` rename does not work — the asset name = local file name) to a GitHub Release tagged `v<FW_VERSION>`. Delete stale asset first: `gh release delete-asset --yes <tag> SmartSpray.bin` (API reflects the change with a few seconds' lag).

## MQTT Topics (base `antennans/SmartSpray/<device_id>/…`)

- `<device_id>` is the **technical device ID**, derived automatically from the ESP8266 MAC address (lowercased, colons stripped) via `deviceId()` in `src/settings.cpp`. It is stable in hardware — never changes across OTA, USB reflash or a settings reset — so MQTT topics and HA `unique_id`s are never recreated.
- The **friendly display name** is `gSettings.name` (deviceName() fallback to `DEVICE_NAME_DEFAULT`); used only for the HA device name and web UI title. Stored in EEPROM (resettable), set on the AP setup page ("Device name", prefilled with the default).

- `trigger` — commands: `SPRAY`, `CHECK` (check updates + republish status), `UPDATE` (install found update), `STATUS` (republish status)
- `status` — JSON status (`version`, `update_available`, `online`)
- `availability` — LWT online/offline
- `version` — retained, current version (HA sensor)
- `update_available` — retained, `true`/`false` (HA binary sensor)
- HA Discovery (5 entities in one device): `homeassistant/button/..._trigger/config` (Spraying → SPRAY), `homeassistant/button/..._check/config` (Check updates → CHECK), `homeassistant/button/..._update/config` (Update → UPDATE, published only when an update is found; its `name` is built at publish time as `Update to vX.Y.Z` from `updaterLatestVersion()`), `homeassistant/sensor/..._version/config`, `homeassistant/binary_sensor/..._update_available/config`

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
