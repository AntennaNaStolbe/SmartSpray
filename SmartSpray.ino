#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <GyverMotor.h>

// ==== Настройки Wi-Fi ====
const char* ssid = "WIFIname"; //Имя вашей сети WIFI
const char* password = "WIFIpassword"; // Пароль вашей сети WIFI

// ==== Настройки MQTT ====
const char* mqtt_server = "192.168.1.1"; // Адрес MQTT Сервера
const int mqtt_port = 1883; // Порт MQTT сервера
const char* mqtt_user = "mqttuser"; // Логин пользователя вашего mqtt сервера
const char* mqtt_pass = "PassWord"; // Пароль пользователя вашего mqtt сервера

// ==== Топики MQTT ====
const char* mqtt_topic_command = "ans/smartspray/trigger";
const char* mqtt_topic_availability = "ans/smartspray/availability";
const char* mqtt_discovery_topic = "homeassistant/button/smartspray_spray/config";

const char* mqtt_discovery_payload =
  "{"
    "\"name\":\"spraying\","
    "\"unique_id\":\"ans_smartSpray_trigger\","
    "\"command_topic\":\"ans/smartspray/trigger\","
    "\"payload_press\":\"SPRAY\","
    "\"availability_topic\":\"ans/smartspray/availability\","
    "\"payload_available\":\"online\","
    "\"payload_not_available\":\"offline\","
    "\"icon\":\"mdi:spray\","
    "\"device\":{"
      "\"identifiers\":[\"ans_smartSpray\"],"
      "\"name\":\"SmartSpray\","
      "\"manufacturer\":\"AntennaNS\","
      "\"model\":\"ESP8266 SmartSpray v2\""
    "}"
  "}";


int motorPower = 188; // Скорость мотора (по сути регулируется мощность нажима на распылитель балона) 0-255

bool isMotorRun = false;

// ==== Пины ====
#define DIG_PIN D2
#define PWM_PIN D1

WiFiClient espClient;
PubSubClient client(espClient);

// Инициализация мотора
GMotor motor(DRIVER2WIRE, DIG_PIN, PWM_PIN, HIGH);

// ==== Функция распыления ====
void spray() {
  isMotorRun = true;
  Serial.println("Запуск spray()");
  motor.setSpeed(motorPower - 40);
  delay(70);
  motor.setSpeed(motorPower - 30);
  delay(70);
  motor.setSpeed(motorPower - 20);
  delay(70);
  motor.setSpeed(motorPower - 10);
  delay(70);
  motor.setSpeed(motorPower);
  delay(400);
  motor.setSpeed(0);
  isMotorRun = false;
}

// ==== Подключение к Wi-Fi ====
void setup_wifi() {
  delay(10);
  Serial.print("Подключение к Wi-Fi: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("Wi-Fi подключен");
  Serial.print("IP адрес: ");
  Serial.println(WiFi.localIP());
}

// ==== Подключение к MQTT ====
void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Подключение к MQTT...");

    if (client.connect(
          "ESP8266_AirFreshener",
          mqtt_user,
          mqtt_pass,
          mqtt_topic_availability, // LWT topic
          1,                        // QoS
          true,                     // retain
          "offline"                 // LWT payload
        )) {

      Serial.println("Успешно");

      // Подписка
      client.subscribe(mqtt_topic_command);
      Serial.print("Подписка на топик: ");
      Serial.println(mqtt_topic_command);

      // Сообщаем, что мы online
      client.publish(mqtt_topic_availability, "online", true);

      // Публикация MQTT Discovery (retain!)
      client.publish(mqtt_discovery_topic, mqtt_discovery_payload, true);

    } else {
      Serial.print("Ошибка подключения, код: ");
      Serial.print(client.state());
      Serial.println(". Повтор через 5 секунд");
      delay(5000);
    }
  }
}

// ==== Обработка входящих сообщений ====
void callback(char* topic, byte* payload, unsigned int length) {
  String message;

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("MQTT: ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(message);

  if (String(topic) == mqtt_topic_command) {
    if (message == "SPRAY" && !isMotorRun) {
      spray();
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Запуск ESP8266 освежителя воздуха...");

  analogWriteFreq(20000); // чтобы не пищал мотор
  client.setBufferSize(512); // буфер PubSubClient. Без этого сообщение discovery тупо не уходит, не влезает :)

  motor.setMode(AUTO);
  motor.setSpeed(0);

  setup_wifi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
}
