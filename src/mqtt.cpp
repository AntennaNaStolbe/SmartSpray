#include "mqtt.h"
#include "config.h"
#include "settings.h"
#include "motor.h"
#include "updater.h"

// ================= MQTT BUFFERS =================
static char mqtt_topic_command[128];
static char mqtt_topic_availability[128];
static char mqtt_topic_status[128];
static char mqtt_topic_version[128];
static char mqtt_topic_update_avail[128];

// HA Discovery entities. Indices:
//   0: SPRAY btn, 1: CHECK btn, 2: version sens, 3: upd-avail bin (always)
//   4: UPDATE btn (conditional — published only when an update is found)
#define MQTT_ENTITY_COUNT 5
static char mqtt_discovery_topic[MQTT_ENTITY_COUNT][96];
static char mqtt_discovery_payload[MQTT_ENTITY_COUNT][512];

static WiFiClient espClient;
static PubSubClient client(espClient);

static AppSettings mqttCfg;

// Min interval between MQTT connection attempts (non-blocking for loop)
#define MQTT_RECONNECT_INTERVAL_MS  5000
static unsigned long mqttLastAttempt = 0;

static bool mqttTryConnect() {
  DEBUG_PRINTLN("[MQTT] Connecting...");

  if (client.connect(
        deviceId().c_str(),           // client_id (stable MAC-based ID)
        mqttCfg.mqtt_user,
        mqttCfg.mqtt_pass,
        mqtt_topic_availability,      // LWT topic
        1,
        true,
        "offline"
      )) {

    DEBUG_PRINTLN("[MQTT] Connected");

    client.subscribe(mqtt_topic_command);
    DEBUG_PRINT("[MQTT] Subscribe: ");
    DEBUG_PRINTLN(mqtt_topic_command);

    client.publish(mqtt_topic_availability, "online", true);
    DEBUG_PRINTLN("[MQTT] Status: online");

    // Publish always-active entities (0..3); conditional Update button (4)
    // is published/removed by mqttPublishDeviceInfo() depending on update availability.
    for (int i = 0; i < 4; i++) {
      client.publish(mqtt_discovery_topic[i], mqtt_discovery_payload[i], true);
    }
    DEBUG_PRINTLN("[MQTT] Discovery published");

    mqttPublishDeviceInfo();
    return true;
  }

  DEBUG_PRINT("[MQTT] Error, state=");
  DEBUG_PRINTLN(client.state());
  return false;
}

// Publishes JSON with version and update availability to .../status.
static void mqttPublishStatus() {
  String payload = String("{\"version\":\"") + FW_VERSION
                 + "\",\"update_available\":\"" + (updaterAvailable() ? "true" : "false")
                 + "\",\"online\":\"true\"}";
  client.publish(mqtt_topic_status, payload.c_str());
}

// Publish version/update availability and show or hide the Update button.
void mqttPublishDeviceInfo() {
  if (!client.connected()) return;
  client.publish(mqtt_topic_version, FW_VERSION, true);
  client.publish(mqtt_topic_update_avail,
                 updaterAvailable() ? "true" : "false", true);
  // Update button is visible only when an update is found; otherwise remove it (empty retained).
  if (updaterAvailable()) {
    client.publish(mqtt_discovery_topic[4], mqtt_discovery_payload[4], true);
  } else {
    client.publish(mqtt_discovery_topic[4], "", true);
  }
}

static void callback(char* topic, byte* payload, unsigned int length) {
  String message;

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  DEBUG_PRINTF("[MQTT] %s -> %s\n", topic, message.c_str());

  if (String(topic) == mqtt_topic_command) {
    if (message == "SPRAY") {
      spray();
    } else if (message == "CHECK") {
      updaterCheck();
      mqttPublishDeviceInfo();
    } else if (message == "UPDATE") {
      updaterInstall();
    } else if (message == "STATUS") {
      mqttPublishStatus();
    } else {
      DEBUG_PRINTLN("[MQTT] Unknown command");
    }
  }
}

void mqttBegin(const AppSettings &s) {
  mqttCfg = s;

  // ---- Build topics (technical ID = stable MAC-based deviceId) ----
  String id = deviceId();
  String base = String("antennans/SmartSpray/") + id;

  (base + "/trigger").toCharArray(mqtt_topic_command, sizeof(mqtt_topic_command));
  (base + "/availability").toCharArray(mqtt_topic_availability, sizeof(mqtt_topic_availability));
  (base + "/status").toCharArray(mqtt_topic_status, sizeof(mqtt_topic_status));
  (base + "/version").toCharArray(mqtt_topic_version, sizeof(mqtt_topic_version));
  (base + "/update_available").toCharArray(mqtt_topic_update_avail, sizeof(mqtt_topic_update_avail));

  // ---- HA Discovery: 5 entities in one device ----
  // The device name is the friendly user-set name; identifiers/unique_ids use
  // the technical ID (stable) so entities are never recreated.
  String dev = String("\"device\":{")
             + "\"identifiers\":[\"SmartSpray_" + id + "\"],"
             + "\"name\":\"" + deviceName() + "\","
             + "\"manufacturer\":\"AntennaNS\","
             + "\"model\":\"SmartSpray\""
             + "}";
  String avail = String("\"availability_topic\":\"") + base + "/availability"
               + "\",\"payload_available\":\"online\",\"payload_not_available\":\"offline\",";

  String topics[MQTT_ENTITY_COUNT];
  String payloads[MQTT_ENTITY_COUNT];

  // 0: "Spraying" button -> SPRAY
  topics[0] = String("homeassistant/button/SmartSpray_") + id + "_trigger/config";
  payloads[0] = String("{") + "\"name\":\"Spraying\","
    + "\"unique_id\":\"SmartSpray_" + id + "_trigger\","
    + "\"command_topic\":\"" + base + "/trigger\",\"payload_press\":\"SPRAY\","
    + avail + "\"icon\":\"mdi:spray\"," + dev + "}";

  // 1: "Check updates" button -> CHECK
  topics[1] = String("homeassistant/button/SmartSpray_") + id + "_check/config";
  payloads[1] = String("{") + "\"name\":\"Check updates\","
    + "\"unique_id\":\"SmartSpray_" + id + "_check\","
    + "\"command_topic\":\"" + base + "/trigger\",\"payload_press\":\"CHECK\","
    + avail + "\"icon\":\"mdi:cloud-sync\"," + dev + "}";

  // 2: "Version" sensor
  topics[2] = String("homeassistant/sensor/SmartSpray_") + id + "_version/config";
  payloads[2] = String("{") + "\"name\":\"Version\","
    + "\"unique_id\":\"SmartSpray_" + id + "_version\","
    + "\"state_topic\":\"" + base + "/version\","
    + avail + "\"icon\":\"mdi:counter\"," + dev + "}";

  // 3: binary_sensor "Update available"
  topics[3] = String("homeassistant/binary_sensor/SmartSpray_") + id + "_update_available/config";
  payloads[3] = String("{") + "\"name\":\"Update available\","
    + "\"unique_id\":\"SmartSpray_" + id + "_update_available\","
    + "\"state_topic\":\"" + base + "/update_available\","
    + "\"payload_on\":\"true\",\"payload_off\":\"false\","
    + avail + "\"device_class\":\"update\"," + dev + "}";

  // 4: "Update" button -> UPDATE (conditional: published only when an update is found)
  topics[4] = String("homeassistant/button/SmartSpray_") + id + "_update/config";
  payloads[4] = String("{") + "\"name\":\"Update\","
    + "\"unique_id\":\"SmartSpray_" + id + "_update\","
    + "\"command_topic\":\"" + base + "/trigger\",\"payload_press\":\"UPDATE\","
    + avail + "\"icon\":\"mdi:update\"," + dev + "}";

  for (int i = 0; i < MQTT_ENTITY_COUNT; i++) {
    topics[i].toCharArray(mqtt_discovery_topic[i], sizeof(mqtt_discovery_topic[i]));
    payloads[i].toCharArray(mqtt_discovery_payload[i], sizeof(mqtt_discovery_payload[i]));
  }

  DEBUG_PRINTLN("[MQTT] Discovery payloads built");

  client.setBufferSize(1024);
  client.setServer(mqttCfg.mqtt_host, mqttCfg.mqtt_port);
  client.setCallback(callback);
}

void mqttLoop() {
  if (!client.connected()) {
    // non-blocking reconnection: no more than once per MQTT_RECONNECT_INTERVAL_MS
    if (millis() - mqttLastAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
      mqttLastAttempt = millis();
      mqttTryConnect();
    }
  }
  client.loop();
}

bool mqttConnected() {
  return client.connected();
}
