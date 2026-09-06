# SmartSpray — Smart Air Freshener for Home Assistant

**English** | [Русский](README_RU.md)

> 💡 **This project is developed with the assistance of AI (assisted coding).** Features, fixes and this documentation are produced with the help of AI agents and reviewed by the author.

---

## 📖 Description
SmartSpray is a project to control an air freshener motor from an **ESP8266** board, integrated with **Home Assistant** via **MQTT**. It lets you trigger a spray remotely (from HA, the web interface, or by timer/motion automation).

---

## Video of the freshener in action
https://github.com/user-attachments/assets/e8b85208-5a7e-4761-8341-9d37cfbdbc90

---

## ⚙ Requirements
- **ESP8266 Wemos D1 Mini** board (or compatible)
- **TA6586** motor driver ([ChipDip link](https://www.chipdip.ru/product/ta6586-drayver-dvigatelya-iout-7a-dip-8-rz-9000663635))
- **AirWick** automatic air freshener ([link](https://market.yandex.ru/product--air-wick-aerozol-dikii-granat-avtomaticheskii-so-smennym-ballonom-250-ml/1779356002?sku=439482020&uniqueId=8530975&do-waremd5=rUQvalKJ0Glrsh-DDBF9-Q&ogV=-3))
- **Home Assistant** with a configured MQTT broker
- [PlatformIO](https://platformio.org/) (`pio` CLI) with ESP8266 support
- 5V power supply. A battery pack is possible, but Wi-Fi is power-hungry, so battery life while idle is short

---

## 📚 Used libraries
The following Arduino libraries are used:

| Library              | Purpose                                        | Link |
|----------------------|------------------------------------------------|------|
| **ESP8266WiFi**      | Connect ESP8266 to a Wi-Fi network             | [GitHub](https://github.com/esp8266/Arduino) |
| **PubSubClient**     | MQTT (send/receive messages)                   | [GitHub](https://github.com/knolleary/pubsubclient) |
| **GyverMotor2**      | Motor control                                  | [GitHub](https://github.com/GyverLibs/GyverMotor2) |
| **ESP8266WebServer** | Web interface (WiFi/MQTT setup, OTA)           | [GitHub](https://github.com/esp8266/Arduino) |
| **DNSServer**        | Captive portal in setup mode                   | [GitHub](https://github.com/esp8266/Arduino) |
| **ESP8266HTTPClient**| HTTP requests to the GitHub API                | [GitHub](https://github.com/esp8266/Arduino) |
| **ESP8266httpUpdate**| Firmware update from GitHub                    | [GitHub](https://github.com/esp8266/Arduino) |
| **ArduinoJson**      | Parsing the GitHub API response when checking updates | [GitHub](https://github.com/bblanchon/ArduinoJson) |

> Libraries are pulled in automatically by PlatformIO (`lib_deps` in `platformio.ini`). Third-party ones (PubSubClient, GyverMotor2, ArduinoJson) are declared explicitly; the ESP8266 core libraries (WiFi, WebServer, HTTPClient, httpUpdate, DNSServer, EEPROM, Update) ship with the `espressif8266` platform.

---

## 🔌 Wiring
- **DIG_PIN (D2)** — digital pin for motor direction control
- **PWM_PIN (D1)** — PWM pin for motor speed control
- Motor power — from an external power supply (recommended), with common **GND** with the ESP8266
- The **TA6586** driver is used to control the motor

![SmartSpray scheme](https://github.com/AntennaNaStolbe/SmartSpray/blob/main/scheme.png)

---

## 🔧 Code configuration
WiFi and MQTT credentials are **not stored in the code** — they are entered through the web interface at first power-on (see below). Only non-secret parameters are configured in `include/config.h`:

```cpp
// ====== DEVICE ID ======
// The technical device ID is derived automatically from the ESP8266 MAC
// address at runtime — no config needed. It is stable in hardware, so MQTT
// topics and HA entities are never recreated (even after OTA / settings reset).
// Friendly display name (shown in HA / web UI), configurable via the web page.
#define DEVICE_NAME_DEFAULT  "SmartSpray"

// Firmware version — also the GitHub release tag for auto-update ("v3.0.8")
#define FW_VERSION "3.0.8"

// Motor speed. Tuned by trial, since each freshener has its own mechanical play.
// The default (188) usually works.
const int MOTOR_POWER = 188;
```

The web interface username/password (optional security) is **not** set here either — it is configured during first-time setup (see below).

---

## 🚀 First flash (step by step, for beginners)

### 1. Install VS Code + PlatformIO
1. Install [VS Code](https://code.visualstudio.com/).
2. In VS Code open the **Extensions** panel and install **PlatformIO IDE** (the one by "PlatformIO").
3. Open the project folder (`SmartSpray`) — PlatformIO will load it automatically.

> If you don't want to use VS Code, you can install [PlatformIO Core](https://platformio.org/install) (`pio` CLI) only.

### 2. (Optional) Set a friendly device name
The technical device ID is generated automatically from the chip's MAC address — you don't need to configure anything for it to work (topics and HA entities are unique and stable out of the box, even for many devices). If you want a nicer name in Home Assistant, set it on the web setup page ("Device name"). To change the default used when the field is left blank, edit `DEVICE_NAME_DEFAULT` in `include/config.h`.

### 3. Connect the board via USB
Connect the **Wemos D1 Mini** to your computer with a **data** USB cable (charge-only cables won't work). You may need to install the CH340 driver (it is usually already present on macOS/Windows).

### 4. Build & upload
- **CLI:** run in the project folder:
  ```
  pio run -e d1_mini -t upload
  ```
- **VS Code:** the `d1_mini` environment appears in the PlatformIO toolbar — select it and click the **Upload** (→) button.

The firmware will compile and be written to the board.

### 5. Watch the serial output (optional)
- **CLI:**
  ```
  pio device monitor
  ```
- **VS Code:** PlatformIO → **Serial Monitor**.

Once the code boots without config, the device starts an access point **`SmartSpray Setup`** so you can proceed to the first-time setup below.

---

## 🛜 First-time setup (WiFi / MQTT / web password)
On first power-on (or after resetting settings) the device starts an access point **`SmartSpray Setup`**.

1. Connect to this Wi-Fi network from your phone/computer (a captive portal should open; otherwise open `http://192.168.4.1` manually).
2. Fill in:
   - **Device name** (optional) — friendly name shown in Home Assistant. Prefilled with `SmartSpray`; the technical ID (used in MQTT topics/HA) is generated automatically from the MAC address and needs no entry.
   - **WiFi Network** — SSID and password of your home network
   - **MQTT Broker** — broker address/port, username and password (if required)
   - **Web Interface Access** (optional security) — tick "Username and password for the web interface" and set a username/password (password ≥ 4 chars). Leave unticked for no password.
3. Press **Save & Reboot** — the device saves settings to EEPROM, reboots and connects to your network.

Settings are stored in flash (EEPROM) and survive reboots **and** OTA updates.

> When web auth is enabled, the browser will ask for the username/password when you open the device's main page (HTTP Basic auth). All controls (spray, power, update, OTA, reset) are then protected.

---

## 🌐 Web interface and OTA
After connecting to your Wi-Fi network, open `http://<ip-device>/` in a browser — the control page is available (top to bottom):

- **Status** — WiFi ✓/✗ and MQTT ✓/✗ (auto-refreshes every 5 s)
- **Spray 💦** button — trigger a spray manually
- **Spray force (motor speed)** slider — the value updates live and auto-saves to EEPROM a second after the last change (no Save button); **−** / **+** steppers next to the slider let you fine-tune it (20–255, default 188). Find the lowest value that still sprays reliably.
- **Check GitHub for updates** button — manually check GitHub for a new release (see below)
- **Drag & drop zone** — attach a `.bin` file to flash over the air (OTA)
- **Update** button — **disabled** by default; it becomes active (and shows what will be installed) when **a file is attached** (plain `Update`) or **a GitHub update is found** (`Update to vX.Y.Z`). On click: if a file was selected — flashes it via OTA; otherwise, if an update exists — installs it from GitHub
- **Logs** window — the last device debug messages (ring buffer of 40 lines), auto-refresh every 3 s, with a **Clear** button
- **Reset settings** button — wipe settings, reboot into access-point mode and return to first-time setup

### 👉 How to flash over OTA (file)
1. Build locally: `pio run` → the file `.pio/build/d1_mini/firmware.bin`
2. On the device page attach it (drag & drop or via the file dialog)
3. The **Update** button turns green — press it and wait for the device to reboot

> ⚠️ OTA upload works via `multipart/form-data` (the web page does this automatically). Sending the firmware as a raw `application/octet-stream` (e.g. `curl --data-binary`) is **not supported** — the device will drop the connection.

---

## 🔌 Behavior when the network is unavailable (power cut / router booting)

The device tolerates temporary network outages and needs no reconfiguration:

- **WiFi unavailable** (router boots slower than the device): the device raises the **`SmartSpray Setup`** access point as an **AP fallback** and scans the airwaves every 30 s for the configured network. As soon as your network appears — it connects automatically, the AP turns off and the device returns to working mode.
- If you entered WiFi/MQTT data incorrectly — connect to the `SmartSpray Setup` AP fallback, fix the settings and press **Save & Reboot**. The device does not "hang".
- **WiFi present but the MQTT broker is still booting** (e.g. HA takes 15 minutes): the device periodically tries to connect to MQTT (bounded reconnect — max once per 20 s, each attempt capped at 3 s) and connects as soon as the broker is ready. The web page stays fast and responsive meanwhile.

> The network check interval is configured in `include/config.h` (`WIFI_RETRY_MS`).

---

## 🔄 Update from GitHub (check + manual install)

The device checks the latest release of `AntennaNaStolbe/SmartSpray` **daily**. This is only a **check** — if a new version exists it offers an update, but **installation only happens on an explicit command** (the **Update** button on the web page or the MQTT command `UPDATE`). This protects against a brick from a failed overnight auto-flash.

You can also press **Check for updates** anytime to check manually.

### How to release an update
1. In `include/config.h` bump the version, e.g. `#define FW_VERSION "3.1.0"`
2. Build: `pio run`
3. Create a GitHub **Release** with tag `v3.1.0` (= `FW_VERSION` with a `v` prefix)
4. Attach the **`SmartSpray.bin`** asset to the release — it's the file `.pio/build/d1_mini/firmware.bin`

Devices will learn about the new version within a day (or via the button) and offer to install it.

> ⚠️ Requests to GitHub are made over HTTPS via `WiFiClientSecure.setInsecure()`. The channel is encrypted, but the certificate is not verified — acceptable for a home OTA scenario.

---

## 🏠 Home Assistant integration

MQTT Discovery is used to register the device in HA automatically. When connected to MQTT, the device appears in the **MQTT** integration as device **SmartSpray** (or the friendly name you set) with entities:

- **Spraying** (button) — trigger a spray
- **Check updates** (button) — check GitHub for updates
- **Update** (button, appears only when an update is available, named e.g. **"Update to v3.0.8"**) — install the found update
- **Version** (sensor) — current firmware version
- **Update available** (binary sensor) — whether an update is available

If the device did not appear in HA automatically, check that "Enable device discovery" is enabled in the MQTT integration settings.

### MQTT topics
Base for device **`<device_id>`** (`device_id` = lowercased MAC without colons, stable): `antennans/SmartSpray/<device_id>/…`

| Topic | Purpose |
|-------|---------|
| `trigger` | Commands: `SPRAY`, `CHECK`, `UPDATE`, `STATUS` |
| `status` | JSON status (`version`, `update_available`, `online`) |
| `availability` | LWT online/offline |
| `version` | Retained, current version (HA sensor) |
| `update_available` | Retained, `true`/`false` (HA binary sensor) |

---

## 🤖 Automation ideas for Home Assistant
After you set up the freshener to trigger on motion or any other trigger, the following is recommended:

1. Add a **Counter** helper with a minimum of 0 and a maximum of 2700.
2. Add an automation that increments this counter by +1.

This way you can track the remaining freshener canister volume (a full canister is about 2700 "sprays").

Implementing such a counter in the sketch is deliberately avoided:
1. The real number of "sprays" per canister may differ from 2700 — it's an approximate average. It's easier to tune in HA.
2. Resetting the counter to 0 is also easier from HA.

---

## 📝 License
The project is distributed freely and may be used for personal and educational purposes.

---

## 💡 Ideas for improvements
Open to your ideas.

Planned:
1. Possibly add a "sprays" counter inside the sketch, saved to EEPROM on each trigger, and send the remaining canister % to HA.
2. Extend the web UI: reset the spray counter, set the target canister %, etc. (done so far: WiFi/MQTT setup, Spray, live force slider with auto-save and −/+ steppers, logs window, OTA, GitHub update check/install, web password).

---

## ✌️ Contact the author
If you have questions or suggestions, feel free to write in Telegram: [@antenna_na_stolbe](https://t.me/antenna_na_stolbe)

I'll help ❤️
