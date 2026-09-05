#include "updater.h"
#include "config.h"
#include "settings.h"
#include "mqtt.h"
#include "webserver.h"
#include <ESP8266WiFi.h>
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

void updaterInit() {
  // reserve flash info — not explicitly needed for ESPhttpUpdate
}

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
  installPending = true;
  installResult = "Update started...";
  DEBUG_PRINTLN("[UPD] Install requested");
  return true;
}

const char *updaterLastResult() {
  return installResult;
}

// Installs a previously found firmware update.
// The firmware BYTES are pulled over plain HTTP from gSettings.update_url:
// GitHub's CDN sends full-size (16KB) TLS records that BearSSL on this device
// cannot buffer (recv buffer must be >= record size, but a 16KB buffer doesn't
// fit the heap together with the 6.2KB BearSSL stack). Plain HTTP has no such
// limit and streams straight into flash with a small buffer.
UpdateCheckResult updaterInstall() {
  if (!availAvailable || availAssetUrl.isEmpty()) {
    DEBUG_PRINTLN("[UPD] No found update to install");
    return UPDATE_RESULT_ERROR;
  }
  if (gSettings.update_url[0] == '\0') {
    DEBUG_PRINTLN("[UPD] No update_url configured - set it to a plain HTTP URL of the .bin, "
                  "or install manually via web OTA");
    return UPDATE_RESULT_ERROR;
  }
  if (!String(gSettings.update_url).startsWith("http://")) {
    DEBUG_PRINTLN("[UPD] update_url must start with http:// (plain HTTP)");
    return UPDATE_RESULT_ERROR;
  }

  DEBUG_PRINTF("[UPD] Starting update from %s\n", gSettings.update_url);

  // Free heap and stop MQTT so the device gives the download a quiet run.
  mqttDisconnect();

  DEBUG_PRINTF("[UPD] heap before download: %d\n", ESP.getFreeHeap());
  WiFiClient tcpClient;   // plain HTTP: no TLS -> tiny heap, no record-size limits
  t_httpUpdate_return ret = ESPhttpUpdate.update(tcpClient, String(gSettings.update_url));
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

// Runs a requested install from the main loop.
bool updaterRunInstall() {
  if (!installPending) return false;
  installPending = false;

  // Largest heap is available when the web server is down, so stop it first.
  // On success the device reboots; on failure we bring the server back up.
  webStop();

  UpdateCheckResult r = updaterInstall();
  switch (r) {
    case UPDATE_RESULT_OK:      installResult = "Update installed. Rebooting..."; break;
    case UPDATE_RESULT_NO_UPDATES: installResult = "No update required"; break;
    case UPDATE_RESULT_ERROR:   installResult = "Update failed (see logs)"; break;
    default:                    installResult = "Update failed (see logs)"; break;
  }

  if (r != UPDATE_RESULT_OK) {
    // Recreate the working-mode web interface so the user can retry.
    mqttBegin(gSettings);
    webInitSta();
  }
  return true;
}
