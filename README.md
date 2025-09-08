# SmartSpray — Умный освежитель воздуха для Home Assistant

## 📖 Описание
SmartSpray — это проект для управления мотором освежителя воздуха с помощью платы **ESP8266** и интеграции через **MQTT** в Home Assistant.  
Устройство позволяет удалённо запускать распыление освежителя.

---

## ⚙ Требования
- **Плата ESP8266** (например, NodeMCU, Wemos D1 Mini и т.п.)
- **Драйвер для управления мотором TA6586** [Тык](https://www.chipdip.ru/product/ta6586-drayver-dvigatelya-iout-7a-dip-8-rz-9000663635)
- **Освежитель воздуха AirWick [Тык](https://market.yandex.ru/product--air-wick-aerozol-dikii-granat-avtomaticheskii-so-smennym-ballonom-250-ml/1779356002?sku=439482020&uniqueId=8530975&do-waremd5=rUQvalKJ0Glrsh-DDBF9-Q&ogV=-3)
- **Home Assistant** с настроенным MQTT брокером
- Arduino IDE для прошивки микроконтроллера

---

## 📚 Используемые библиотеки
В проекте применяются следующие библиотеки Arduino:

| Библиотека            | Назначение | Ссылка |
|-----------------------|------------|--------|
| **ESP8266WiFi**       | Подключение ESP8266 к Wi-Fi сети | [GitHub](https://github.com/esp8266/Arduino) |
| **PubSubClient**      | Работа с MQTT (отправка и получение сообщений) | [GitHub](https://github.com/knolleary/pubsubclient) |
| **GyverMotor**        | Управление двигателем | [GitHub](https://github.com/AlexGyver/GyverMotor) |

> ⚠️ Убедитесь, что все библиотеки установлены через **Library Manager** в Arduino IDE.

---

## 🔌 Подключение
- **DIG_PIN (D2)** — цифровой пин управления направлением мотора  
- **PWM_PIN (D1)** — ШИМ-пин для управления скоростью мотора  
- Питание мотора — через внешний источник питания (рекомендуется), с общей землей (`GND`) с ESP8266
- Для управления мотором используется драйвер TA6586 - [Тык](https://www.chipdip.ru/product/ta6586-drayver-dvigatelya-iout-7a-dip-8-rz-9000663635)

---

## 🔧 Настройка кода
Перед прошивкой в файле `.ino` укажите свои параметры:

```cpp
// Настройки Wi-Fi
const char* ssid = "WIFIname";       // Имя вашей сети Wi-Fi
const char* password = "WIFIpassword"; // Пароль от Wi-Fi

// Настройки MQTT
const char* mqtt_server = "192.168.1.1"; // IP-адрес MQTT сервера
const int mqtt_port = 1883;              // Порт MQTT (по умолчанию 1883)
const char* mqtt_user = "mqttuser";      // Логин
const char* mqtt_pass = "PassWord";      // Пароль

// Топик, по которому принимаются команды
const char* mqtt_topic_sub = "home-assistant/airfreshener/trigger";
```

---

## 🏠 Интеграция с Home Assistant
В Home Assistant можно настроить автоматизацию или кнопку для управления освежителем.  
Пример MQTT-сервиса в `configuration.yaml`:

```yaml
switch:
  - platform: mqtt
    name: "Освежитель воздуха"
    state_topic: "home-assistant/airfreshener/status"
    command_topic: "home-assistant/airfreshener/trigger"
    payload_on: "ON"
    payload_off: "OFF"
    qos: 0
    retain: false
```

После этого в интерфейсе Home Assistant появится переключатель для активации устройства.

---

## 🚀 Заливка прошивки
1. Установите **Arduino IDE** и добавьте поддержку **ESP8266**:
   - Файл → Настройки → *Additional Board Manager URLs*:  
     ```
     http://arduino.esp8266.com/stable/package_esp8266com_index.json
     ```
   - Инструменты → Плата → Менеджер плат → **ESP8266** → Установить

2. Установите все библиотеки:
   - Sketch → Include Library → Manage Libraries → выполните поиск по названиям из списка выше

3. Подключите плату ESP8266 по USB и выберите её в **Tools → Board**.

4. Загрузите проект на плату:
   ```
   Sketch → Upload
   ```

---

## 📂 Структура проекта
```
smartSpray/
│
├── 2a79b19b-5bd3-4184-b790-9e617101b5a2.ino    # Основной скетч
└── README.md                                    # Описание проекта
```

---

## 📝 Лицензия
Проект распространяется свободно и может быть использован для личных и образовательных целей.

## Оправдания :)
Это мой первый проект, выложенный на GitHub
Пожалуйста относитесь с пониманием :)
Устройство делалось для себя - успешно работает уже 3 месяца, по этому решил поделиться своими наработками
Можно ли было использовать транзистор вместо драйвера? - Да, но я решил сделать так

В будущем хочу добавить отслеживание остатка балона, по количеству "пшиков"