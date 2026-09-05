#include "updater.h"
#include "config.h"
#include "settings.h"
#include "mqtt.h"
#include "webserver.h"
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266httpUpdate.h>
#include <ArduinoJson.h>

#define TAG "UPD"

// Compare two versions in "X.Y.Z" format (with optional "v" prefix).
// Returns true if a < b.
static bool versionLess(const String &a, const String &b) {
  String sa = a, sb = b;
  // strip non-digit prefix ("v"/"V")
  while (sa.length() && !isdigit(sa[0])) sa.remove(0, 1);
  while (sb.length() && !isdigit(sb[0])) sb.remove(0, 1);

  int na = 0, nb = 0;
  int ia = 0, ib = 0;
  while (true) {
    na = sa.substring(ia).toInt();
    nb = sb.substring(ib).toInt();
    if (na != nb) return na < nb;
    int da = sa.indexOf('.', ia);
    int db = sb.indexOf('.', ib);
    if (da < 0 && db < 0) return false;   // equal
    if (da < 0) return true;              // a ended first
    if (db < 0) return false;
    ia = da + 1;
    ib = db + 1;
  }
}

// Permanently holding a WiFiClientSecure (the 6.2KB BearSSL thunk stack) splits
// even the fresh boot heap into two halves, so there is NO long-lived secure
// client. The thunk is compiled only during the boot install (via the download
// client), when the heap is still monolithic.

// ==================== Found update state ====================
static bool availAvailable = false;      // new update found (not yet installed)
static String availAssetUrl = "";        // direct link to .bin of the new release
static String availVersion = "";         // new release version

bool updaterAvailable() {
  return availAvailable;
}

const String &updaterLatestVersion() {
  return availVersion;
}

// Parses release JSON, finds asset. Returns UPDATE_RESULT_AVAILABLE /
// NO_UPDATES / ERROR and fills avail* fields.
static UpdateCheckResult parseRelease(JsonDocument &doc) {
  const char *tag = doc["tag_name"] | "";
  if (!tag[0]) {
    DEBUG_PRINTLN("[UPD] No tag_name in release");
    return UPDATE_RESULT_ERROR;
  }

  DEBUG_PRINTF("[UPD] Latest tag: %s, our FW: %s\n", tag, FW_VERSION);

  String latest = String(tag);
  if (!versionLess(FW_VERSION, latest)) {
    DEBUG_PRINTLN("[UPD] Already up to date");
    return UPDATE_RESULT_NO_UPDATES;
  }

  String assetUrl = "";
  JsonArray assets = doc["assets"].as<JsonArray>();
  for (JsonObject a : assets) {
    const char *name = a["name"] | "";
    if (strcmp(name, FW_ASSET_NAME) == 0) {
      assetUrl = a["browser_download_url"] | "";
      break;
    }
  }

  if (assetUrl.isEmpty()) {
    DEBUG_PRINTLN("[UPD] Asset " FW_ASSET_NAME " not found");
    return UPDATE_RESULT_ERROR;
  }

  availAvailable = true;
  availAssetUrl = assetUrl;
  availVersion = latest;
  DEBUG_PRINTF("[UPD] Update available to %s (%s)\n", latest.c_str(), assetUrl.c_str());
  return UPDATE_RESULT_AVAILABLE;
}

// Checks for a new release without installing.
UpdateCheckResult updaterCheck() {
  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("[UPD] No WiFi, skipping update check");
    return UPDATE_RESULT_ERROR;
  }

  DEBUG_PRINTLN("[UPD] Checking GitHub: " FW_GITHUB_API);
  DEBUG_PRINTF("[UPD] heap before connect: %u\n", ESP.getFreeHeap());

  WiFiClientSecure client;
  client.setInsecure();                    // don't verify certificate
  client.setTimeout(15);                   // SNI is set automatically from the URL host
  // Default TLS recv buffer is 16KB — on ESP8266 with limited heap the
  // handshake fails (connection drops, GET returns -1). Reducing to 8KB
  // is enough for both the handshake and reading the response body.
  client.setBufferSizes(8192, 512);
  client.setNoDelay(true);

  HTTPClient http;
  if (!http.begin(client, FW_GITHUB_API)) {
    DEBUG_PRINTLN("[UPD] Failed to start HTTP");
    return UPDATE_RESULT_ERROR;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    DEBUG_PRINTF("[UPD] GitHub replied %d\n", code);
    http.end();
    return UPDATE_RESULT_ERROR;
  }

  // Parse only needed fields via ArduinoJson filter, reading directly from the
  // response stream (no full-body String held in RAM). The release JSON
  // (~5KB) no longer fits the device heap via getString() — it pre-reserves the
  // whole Content-Length up front and fails on the ESP8266's scarce heap.
  StaticJsonDocument<256> filter;
  filter["tag_name"] = true;
  JsonArray filterAssets = filter.createNestedArray("assets");
  JsonObject filterAsset = filterAssets.createNestedObject();
  filterAsset["name"] = true;
  filterAsset["browser_download_url"] = true;

  DynamicJsonDocument doc(1536);
  DeserializationError err =
      deserializeJson(doc, *http.getStreamPtr(), DeserializationOption::Filter(filter));
  http.end();

  if (err) {
    DEBUG_PRINTF("[UPD] GitHub stream parse error: %s (doc=%d bytes, heap=%u)\n",
                 err.c_str(), (int)doc.capacity(), ESP.getFreeHeap());
    return UPDATE_RESULT_ERROR;
  }

  return parseRelease(doc);
}

// ==================== Pending install (deferred to loop) ====================
static bool installPending = false;
static const char *installResult = "";

bool updaterRequestInstall() {
  if (!availAvailable || availAssetUrl.isEmpty()) {
    installResult = "No update found to install";
    return false;
  }
  // Persist the pending flag AND the asset URL so the update survives a reboot.
  // The download itself is deferred to the next boot: the device reboots now
  // (see updaterRunInstall) and updaterRunPendingOnBoot() installs on the clean
  // boot heap, BEFORE the web server / MQTT start fragmenting it. Storing the
  // URL means the boot install needs no re-check (no extra TLS client on the
  // fresh heap - each one leaves small BearSSL pieces that cap the largest free
  // block below what the 16.7KB TLS receive buffer needs).
  gSettings.update_pending = 1;
  strncpy(gSettings.update_asset_url, availAssetUrl.c_str(),
          SETTINGS_UPDATE_URL_MAX - 1);
  gSettings.update_asset_url[SETTINGS_UPDATE_URL_MAX - 1] = '\0';
  settingsSave(gSettings);
  installPending = true;
  installResult = "Update started...";
  DEBUG_PRINTLN("[UPD] Install requested (reboot pending)");
  return true;
}

const char *updaterLastResult() {
  return installResult;
}

// Tiny Stream adapter that swallows the TLS payload straight into the OTA
// flash buffer: our download loop reads plaintext segments from the connected
// client and feeds them here; write() calls Update.write() chunk-wise so no
// extra RAM is needed for the firmware bytes.
class UpdateStream : public Stream {
  public:
    bool init() { return Update.begin(ESP.getFreeSketchSpace(), U_FLASH); }
    bool finish() { return Update.end(true); }
    size_t bytes() const { return _n; }
    bool err() const { return _err != 0; }

    size_t write(uint8_t b) override {
      uint8_t u = b;
      int r = Update.write(&u, 1);
      if (r != 1) { _err = r; return 0; }
      _n++;
      return 1;
    }
    size_t write(const uint8_t *buf, size_t size) override {
      int r = Update.write((uint8_t *)buf, size);
      if (r <= 0) { _err = r; return 0; }
      _n += (size_t)r;
      return (size_t)r;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}

  private:
    size_t _n = 0;
    int    _err = 0;
};

// Installs a previously found firmware update.
// By default the firmware BYTES are downloaded straight from GitHub over TLS:
// github.com returns a 302 to the CDN, which is then streamed to the flash on
// the same 16.7KB-buffered TLS client (see the GitHub branch below). The
// install runs on the clean boot heap (reboot scheme). gSettings.update_url is
// an optional override pointing at a plain-HTTP LAN host (kept for local
// installs; default empty = GitHub).
UpdateCheckResult updaterInstall() {
  if (!availAvailable || availAssetUrl.isEmpty()) {
    DEBUG_PRINTLN("[UPD] No found update to install");
    return UPDATE_RESULT_ERROR;
  }

  String url = availAssetUrl;
  bool useGithub = (gSettings.update_url[0] == '\0');
  if (!useGithub) {
    url = String(gSettings.update_url);
    if (!url.startsWith("http://")) {
      DEBUG_PRINTLN("[UPD] update_url must start with http:// (plain HTTP)");
      return UPDATE_RESULT_ERROR;
    }
  }

  // The web server is already stopped by updaterRunInstall(); drop MQTT too so
  // the TLS download gets the biggest possible heap.
  mqttDisconnect();
  DEBUG_PRINTF("[UPD] heap before download: %u max=%u\n", ESP.getFreeHeap(),
               (unsigned)ESP.getMaxFreeBlockSize());

  // Order matters a lot here. Constructing a WiFiClientSecure pulls in the
  // 6.2KB BearSSL stack, and holding one while resolving the CDN URL leaves the
  // heap too fragmented for the ~16.7KB receive buffer ("Unable to allocate
  // memory for SSL structures"). So: use a plain client by default; in the
  // GitHub branch build the big secure client FIRST (it grabs the biggest
  // contiguous region) and only then resolve, inside the branch.
  WiFiClient plainClient;
  WiFiClient *client = &plainClient;

  if (useGithub) {
    DEBUG_PRINTLN("[UPD] Installing from GitHub (TLS)");

    // One WiFiClientSecure with the maximum BearSSL receive buffer is reused
    // for both hops: hop1 gets the github.com 302 (redirects are NOT followed
    // by HTTPClient here - ESP8266httpUpdate does not follow them at all and
    // aborts with HTTP_UE_SERVER_FILE_NOT_FOUND (-104)), hop2 reconnects to the
    // CDN and streams the firmware body to the flash. hop1 is fully freed
    // before hop2 connects, so the single ~16.7KB buffer rotates in the same
    // heap region. Live streaming uses explicit per-read timeouts (Stream's
    // sendSize can block indefinitely). Every failed attempt rebuilds the
    // client so a broken one releases its heap fully.
    for (int attempt = 1; attempt <= 3; attempt++) {
      WiFiClientSecure tlsClient;
      tlsClient.setInsecure();
      tlsClient.setTimeout(30);
      tlsClient.setBufferSizes(16384, 512);
      tlsClient.setNoDelay(true);

      DEBUG_PRINTF("[UPD] Try %d: updating from %s\n", attempt, url.c_str());

      {
        HTTPClient h;
        h.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        h.setTimeout(30000);
        if (h.begin(tlsClient, url)) {
          int code1 = h.GET();
          String loc = h.getLocation();
          DEBUG_PRINTF("[UPD] hop1 code=%d\n", code1);
          h.end();
          if (code1 == HTTP_CODE_FOUND && loc.startsWith("https://")) {
            if (h.begin(tlsClient, loc)) {
              int code2 = h.GET();
              DEBUG_PRINTF("[UPD] hop2 code=%d\n", code2);
              if (code2 == HTTP_CODE_OK) {
                WiFiClient *s = h.getStreamPtr();
                const int body = h.getSize();
                tlsClient.setTimeout(8);
                UpdateStream us;
                DEBUG_PRINTF("[UPD] flash begin, free=%u\n",
                             (unsigned)ESP.getFreeSketchSpace());
                us.init();
                int total = 0;
                while (h.connected() && total < body) {
                  while (s->available() > 0 && total < body) {
                    uint8_t buf[512];
                    int n = s->read(buf, sizeof buf);
                    if (n > 0) {
                      int w = us.write(buf, (size_t)n);
                      if (w != n) {
                        DEBUG_PRINTF("[UPD] flash write error %d\n", w);
                      }
                      total += n;
                    } else if (n < 0) {
                      DEBUG_PRINTLN("[UPD] tls read error");
                      total = body;   // force exit
                    }
                  }
                  delay(1);
                }
                DEBUG_PRINTF("[UPD] download done: %d/%d conn=%d\n",
                             total, body, (int)h.connected());
                if (total == body && !us.err()) {
                  if (us.finish()) {
                    DEBUG_PRINTF("[UPD] Updated (%u bytes)! Rebooting...\n",
                                 (unsigned)us.bytes());
                    h.end();
                    delay(600);
                    yield();
                    ESP.restart();
                    return UPDATE_RESULT_OK;
                  }
                  DEBUG_PRINTF("[UPD] Update.end failed (%d), keeping old firmware\n",
                               (int)Update.getError());
                } else {
                  DEBUG_PRINTF("[UPD] download aborted: %d/%d\n", total, body);
                }
                h.end();
                delay(1200);
              } else {
                DEBUG_PRINTF("[UPD] CDN error: code=%d\n", code2);
              }
              h.end();
            }
          } else {
            DEBUG_PRINTF("[UPD] unexpected hop1: code=%d\n", code1);
          }
        } else {
          DEBUG_PRINTLN("[UPD] failed to start request");
        }
        h.end();
      }
      tlsClient.stop();
      delay(1500);
      yield();
    }
    return UPDATE_RESULT_ERROR;
  }
  DEBUG_PRINTF("[UPD] Starting update from %s\n", url.c_str());
  t_httpUpdate_return ret = ESPhttpUpdate.update(*client, url);
  switch (ret) {
    case HTTP_UPDATE_OK:
      DEBUG_PRINTLN("[UPD] Updated! Rebooting...");
      return UPDATE_RESULT_OK;
    case HTTP_UPDATE_NO_UPDATES:
      DEBUG_PRINTLN("[UPD] No updates");
      availAvailable = false;
      availAssetUrl = "";
      availVersion = "";
      return UPDATE_RESULT_NO_UPDATES;
    case HTTP_UPDATE_FAILED:
      DEBUG_PRINTF("[UPD] Update error: %s (%d)\n",
                   ESPhttpUpdate.getLastErrorString().c_str(),
                   ESPhttpUpdate.getLastError());
      return UPDATE_RESULT_ERROR;
  }
  return UPDATE_RESULT_ERROR;
}

// Rebooting is the safest way to get a monolithic heap: the running web
// server / MQTT client leave ~2KB of small used blocks right between the two
// large free areas, so the largest free block caps at ~12KB (under the 16.7KB
// BearSSL receive buffer needs) no matter how the heap is defragmented. A fresh
// boot has a single large free region, so the install (done by
// updaterRunPendingOnBoot in setup) reliably fits.
bool updaterRunInstall() {
  if (!installPending) return false;
  installPending = false;

  DEBUG_PRINTLN("[UPD] Rebooting to install on a clean heap...");
  // Give the "Update started..." HTTP response a moment to flush to the browser
  // (and the MQTT ack) before the WDT-safe restart.
  delay(400);
  yield();
  ESP.restart();
  return true;
}

// Called from setup() once WiFi is connected, BEFORE the web server and MQTT
// start. If the user pressed Update last session, install the remembered update
// now on the clean boot heap. Clears the flag first so a failed install cannot
// loop the device rebooting forever.
bool updaterRunPendingOnBoot() {
  if (!gSettings.update_pending) return false;
  gSettings.update_pending = 0;
  settingsSave(gSettings);

  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTLN("[UPD] Pending update, but no WiFi - skipping");
    return true;
  }

  if (gSettings.update_asset_url[0] == '\0') {
    DEBUG_PRINTLN("[UPD] Pending update, but asset URL missing - skipping");
    return true;
  }

  DEBUG_PRINTLN("[UPD] Pending update found, installing...");
  // Reuse the found-update state so updaterInstall() takes the GitHub path.
  availAvailable = true;
  availAssetUrl = gSettings.update_asset_url;
  availVersion = "pending";
  updaterInstall();   // on success updaterInstall() reboots
  return true;
}
