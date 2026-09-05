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

// Resolves the browser_download_url (github.com -> 302) to the DIRECT CDN URL.
// Uses a small TLS buffer (the 302 response is tiny), because following the
// redirect inside the big-buffer download client would hold TWO 16.7KB recv
// buffers (github.com + CDN host) at once and never fit the heap.
static String githubDirectAssetUrl() {
  WiFiClientSecure c;
  c.setInsecure();
  c.setTimeout(15);
  c.setBufferSizes(8192, 512);
  c.setNoDelay(true);
  HTTPClient h;
  if (!h.begin(c, availAssetUrl)) {
    DEBUG_PRINTLN("[UPD] Resolve: failed to start HTTP");
    return "";
  }
  const char *keys[] = {"Location"};
  h.collectHeaders(keys, 1);
  h.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
  int code = h.GET();
  String loc = h.header("Location");
  h.end();
  if (code != HTTP_CODE_FOUND && code != HTTP_CODE_MOVED_PERMANENTLY) {
    DEBUG_PRINTF("[UPD] Resolve: expected 302, got %d\n", code);
    return "";
  }
  if (!loc.startsWith("https://")) {
    DEBUG_PRINTLN("[UPD] Resolve: bad Location header");
    return "";
  }
  DEBUG_PRINTF("[UPD] Direct asset URL: %s", loc.c_str());
  return loc;
}

// Installs a previously found firmware update.
// By default the firmware BYTES are downloaded straight from GitHub (the
// release asset) over TLS. GitHub's asset CDN sends full-size (~16.4KB) TLS
// records, so BearSSL needs the maximum receive buffer (16384 + 325 overhead =
// 16709B); anything smaller stalls the stream. That buffer + the 6.2KB BearSSL
// thunk stack only just fit the heap, and only because the install is deferred
// (web server + MQTT stopped first, see updaterRunInstall) and the direct CDN
// URL is resolved with a cheap small-buffer request beforehand (a redirect
// inside the big-buffer client would double the recv-buffer peak).
// gSettings.update_url is an optional override pointing at a plain-HTTP LAN
// host (kept for local installs; default empty = GitHub).
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
  DEBUG_PRINTF("[UPD] heap before download: %u\n", ESP.getFreeHeap());

  // Order matters a lot here. Constructing a WiFiClientSecure pulls in the
  // 6.2KB BearSSL stack, and holding one while resolving the CDN URL leaves the
  // heap too fragmented for the ~16.7KB receive buffer ("Unable to allocate
  // memory for SSL structures"). So: use a plain client by default, resolve +
  // defrag, and only then build the big secure client.
  WiFiClient plainClient;
  WiFiClient *client = &plainClient;
  WiFiClientSecure tlsClient;

  if (useGithub) {
    DEBUG_PRINTLN("[UPD] Installing from GitHub (TLS)");
    String direct = githubDirectAssetUrl();
    if (direct.isEmpty()) {
      DEBUG_PRINTLN("[UPD] Could not resolve the direct asset URL");
      return UPDATE_RESULT_ERROR;
    }
    url = direct;

    // Coalesce the heap: grab the largest free block and release it so the
    // BearSSL receive buffer gets a contiguous ~17KB region. Repeat in case a
    // release frees up something adjacent.
    size_t warm = ESP.getFreeHeap() - 4096;
    for (int pass = 0; pass < 2; pass++) {
      void *defrag = warm > 0 ? malloc(warm) : (void*)0;
      if (defrag) free(defrag);
      if (ESP.getMaxFreeBlockSize() >= 17000) break;
    }
    if (ESP.getMaxFreeBlockSize() < 17000) {
      DEBUG_PRINTF("[UPD] Heap too fragmented for TLS (%u free, %u max block)",
                   (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxFreeBlockSize());
      return UPDATE_RESULT_ERROR;
    }

    // Maximum BearSSL receive buffer. The CDN sends records just over 16KB;
    // the maximum settable (16384) + BearSSL overhead (325B) = 16709B covers
    // them - but a smaller buffer stalls the TLS stream.
    tlsClient.setInsecure();
    tlsClient.setTimeout(30);
    tlsClient.setBufferSizes(16384, 512);
    tlsClient.setNoDelay(true);
    client = &tlsClient;

    // Pre-flight handshake to the CDN host. Empirically required: a fresh
    // big-buffer download right after resolving can fail with "connection
    // failed (-1)"; priming the DNS + socket + TLS to the same CDN edge first
    // makes the download reliable.
    String host = direct;
    if (host.startsWith("https://")) host.remove(0, 8);
    int slash = host.indexOf('/');
    if (slash >= 0) host.remove(slash);
    char hostBuf[96];
    host.toCharArray(hostBuf, sizeof(hostBuf));

    WiFiClientSecure pre;
    pre.setInsecure();
    pre.setTimeout(15);
    pre.setBufferSizes(16384, 512);
    pre.setNoDelay(true);
    int rc = pre.connect(hostBuf, 443);
    char serr[96] = "";
    pre.getLastSSLError(serr, sizeof(serr));
    pre.stop();
    if (rc != 1) {
      DEBUG_PRINTF("[UPD] TLS pre-flight failed (%d) \"%s\"", rc, serr);
      return UPDATE_RESULT_ERROR;
    }
    DEBUG_PRINTF("[UPD] TLS pre-flight OK (%s)", hostBuf);
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
