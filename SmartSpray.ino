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

const char* mqtt_topic_sub = "home-assistant/airfreshener/trigger";

int motorPower = 150; // Скорость мотора (по сути регулируется мощность нажима на распылитель балона)

bool isMotorRun = false;
bool isFirstMessage = true;         // Флаг первого сообщения

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
  delay(300);
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
    if (client.connect("ESP8266_AirFreshener", mqtt_user, mqtt_pass)) {
      Serial.println("Успешно");
      client.subscribe(mqtt_topic_sub);
      Serial.print("Подписка на топик: ");
      Serial.println(mqtt_topic_sub);
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
  // Сохраняем сообщение посимвольно (хз почему так)
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  // При поключении к mqtt получаем текущее состояние системы
  Serial.print("Получено сообщение по MQTT: ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(message);
  if (message == "ON" && isMotorRun == false) {
    spray();
    Serial.println("Остановка мотора");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Запуск ESP8266 освежителя воздуха...");

  //analogWriteFreq(4000);
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
