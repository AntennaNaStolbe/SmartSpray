#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <GyverMotor.h>

// ================= DEBUG =================
#define DEBUG_MODE 1

#if DEBUG_MODE
  #define DEBUG_PRINT(x) Serial.print(x)
  #define DEBUG_PRINTLN(x) Serial.println(x)
  #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
#endif

// ================= НАСТРОЙКИ =================

// Wi-Fi
const char* ssid     = "WiFiName";
const char* password = "WiFiPass";

// MQTT
const char* mqtt_server = "192.168.1.3";
const int   mqtt_port   = 1883;
const char* mqtt_user   = "mqttuser";
const char* mqtt_pass   = "mqttpass";

// ====== DEVICE ID ======
String device_id = "spray_bathroom";

// ================= MQTT БУФЕРЫ =================
char mqtt_topic_command[128];
char mqtt_topic_availability[128];
char mqtt_discovery_topic[128];
char mqtt_discovery_payload[512];

// ================= ДРУГОЕ =================
int motorPower = 188;
bool isMotorRun = false;

// Пины
#define DIG_PIN D2
#define PWM_PIN D1

WiFiClient espClient;
PubSubClient client(espClient);
GMotor motor(DRIVER2WIRE, DIG_PIN, PWM_PIN, HIGH);

// ================= SPRAY =================
void spray() {
  if (isMotorRun) {
    DEBUG_PRINTLN("[SPRAY] Уже выполняется, игнор");
    return;
  }

  DEBUG_PRINTLN("[SPRAY] Запуск распыления");
  isMotorRun = true;

  motor.setSpeed(motorPower - 40); delay(70);
  motor.setSpeed(motorPower - 30); delay(70);
  motor.setSpeed(motorPower - 20); delay(70);
  motor.setSpeed(motorPower - 10); delay(70);
  motor.setSpeed(motorPower);      delay(400);
  motor.setSpeed(0);

  isMotorRun = false;
  DEBUG_PRINTLN("[SPRAY] Завершено");
}

// ================= WIFI =================
void setup_wifi() {
  DEBUG_PRINTF("[WIFI] Подключение к %s\n", ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG_PRINT(".");
  }

  DEBUG_PRINTLN("");
  DEBUG_PRINTLN("[WIFI] Подключено");
  DEBUG_PRINT("[WIFI] IP: ");
  DEBUG_PRINTLN(WiFi.localIP());
}

// ================= MQTT =================
void reconnectMQTT() {
  while (!client.connected()) {

    DEBUG_PRINTLN("[MQTT] Подключение...");

    if (client.connect(
          device_id.c_str(),           // client_id
          mqtt_user,
          mqtt_pass,
          mqtt_topic_availability,     // LWT topic
          1,
          true,
          "offline"
        )) {

      DEBUG_PRINTLN("[MQTT] Подключено");

      client.subscribe(mqtt_topic_command);
      DEBUG_PRINT("[MQTT] Подписка: ");
      DEBUG_PRINTLN(mqtt_topic_command);

      client.publish(mqtt_topic_availability, "online", true);
      DEBUG_PRINTLN("[MQTT] Статус: online");

      client.publish(mqtt_discovery_topic, mqtt_discovery_payload, true);
      DEBUG_PRINTLN("[MQTT] Discovery опубликован");

    } else {
      DEBUG_PRINT("[MQTT] Ошибка, state=");
      DEBUG_PRINTLN(client.state());
      DEBUG_PRINTLN("[MQTT] Повтор через 5 сек");
      delay(5000);
    }
  }
}

// ================= CALLBACK =================
void callback(char* topic, byte* payload, unsigned int length) {
  String message;

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  DEBUG_PRINTF("[MQTT] %s -> %s\n", topic, message.c_str());

  if (String(topic) == mqtt_topic_command) {
    if (message == "SPRAY") {
      spray();
    } else {
      DEBUG_PRINTLN("[MQTT] Неизвестная команда");
    }
  }
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  DEBUG_PRINTLN("\n[BOOT] SmartSpray запускается");

  analogWriteFreq(20000);
  client.setBufferSize(1024);

  motor.setMode(AUTO);
  motor.setSpeed(0);

  // ---- Формирование топиков ----
  String base = "antennans/SmartSpray/" + device_id;

  (base + "/trigger").toCharArray(mqtt_topic_command, sizeof(mqtt_topic_command));
  (base + "/availability").toCharArray(mqtt_topic_availability, sizeof(mqtt_topic_availability));

  String discovery_topic =
    "homeassistant/button/SmartSpray_" + device_id + "_trigger/config";
  discovery_topic.toCharArray(mqtt_discovery_topic, sizeof(mqtt_discovery_topic));

  // ---- Discovery payload ----
  String payload =
    "{"
      "\"name\":\"Spraying\","
      "\"unique_id\":\"SmartSpray_" + device_id + "_trigger\","
      "\"command_topic\":\"" + base + "/trigger\","
      "\"payload_press\":\"SPRAY\","
      "\"availability_topic\":\"" + base + "/availability\","
      "\"payload_available\":\"online\","
      "\"payload_not_available\":\"offline\","
      "\"icon\":\"mdi:spray\","
      "\"device\":{"
        "\"identifiers\":[\"SmartSpray_" + device_id + "\"],"
        "\"name\":\"SmartSpray_" + device_id + "\","
        "\"manufacturer\":\"AntennaNS\","
        "\"model\":\"SmartSpray\""
      "}"
    "}";

  payload.toCharArray(mqtt_discovery_payload, sizeof(mqtt_discovery_payload));

  DEBUG_PRINTLN("[MQTT] Discovery payload сформирован");

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

// ================= LOOP =================
void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
}
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <GyverMotor.h>
