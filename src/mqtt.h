#ifndef MQTT_H
#define MQTT_H

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "settings.h"

// Set MQTT parameters from config (host/port/user/pass) and build topics.
// Call once after successful WiFi connection.
void mqttBegin(const AppSettings &s);

// Maintain MQTT: reconnection + loop. Call in every loop() iteration.
void mqttLoop();

// Whether the MQTT client is currently connected
bool mqttConnected();

// Publish fresh device state (version + update availability) to HA topics.
// Called on connect and after update check/install.
void mqttPublishDeviceInfo();

// Disconnect and free the MQTT client socket/buffer. Called before a firmware
// download to maximize free heap.
void mqttDisconnect();

#endif
