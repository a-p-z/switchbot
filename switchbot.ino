#include <EEPROM.h>
#include <PubSubClient.h>
#include <Servo.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "MQTT.h"  // it defines MQTT configurations
#include "Wpa.h"   // it defines WiFi WSSID and WPA

#define SERVO_PIN 13
#define EEPROM_SIZE 2

Servo servo; 
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void connectMttqClient();
void connectToWifi();
void onServoTopic(char* topic, byte* payload, unsigned int length);
int readFromEEPROM(); 
void writeToEEPROM(int value);

void setup() {
  Serial.begin(115200);
  
  EEPROM.begin(EEPROM_SIZE);
  
  WiFi.setHostname(HOSTNAME);
  WiFi.setSleep(true);
  
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(onServoTopic);
  
  servo.attach(SERVO_PIN);
}

void loop() {
  if(WiFi.status() != WL_CONNECTED) {
    connectToWifi();    
  }
  
  if (!mqttClient.connected()) {
    connectMqttClient();
    mqttClient.subscribe(MQTT_SERVO_TOPIC);
    mqttClient.publish(MQTT_LWT_TOPIC, "Online", true);
    Serial.printf("MQTT: %s = Online\n", MQTT_LWT_TOPIC);  
    int value = readFromEEPROM();
    value = value > 0 ? value : 0;
    value = value < 180 ? value : 180;
    servo.write(value);
    char buffer[40];
    sprintf(buffer, "{\"angle\": %d}", value);
    mqttClient.publish(MQTT_STATE_TOPIC, buffer);
    Serial.printf("MQTT: %s = %s\n", MQTT_STATE_TOPIC, buffer);
  }
  
  mqttClient.loop();
}

void connectToWifi() {
  Serial.printf("WIFI: Connecting to '%s'\n", WSSID);
  WiFi.begin(WSSID, WPA);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.printf("WIFI: Connected\n");
  
  Serial.print("WIFI: IP address is ");
  Serial.println(WiFi.localIP());
}

void connectMqttClient() {
  while (!mqttClient.connected()) {
    Serial.printf("MQTT: Attempting connection to '%s:%d'...\n", MQTT_SERVER, MQTT_PORT);
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      Serial.printf("MQTT: Connected\n");
      
    } else {
      Serial.printf("MQTT: Failed, rc=%s\n", mqttClient.state());
      delay(5000);
    }
  }
}

void onServoTopic(char* topic, byte* payload, unsigned int length) {
  Serial.printf("MQTT: %s = ", MQTT_SERVO_TOPIC);
  for (int i = 0; i < length; i++) {
    Serial.print((char) payload[i]);
  }
  Serial.println();
  StaticJsonDocument <256> doc;
  deserializeJson(doc, payload);
  int value = doc["angle"];
  long ms = doc["time"];
  int previousValue = servo.read();
  servo.write(value);
  char buffer[40];
  sprintf(buffer, "{\"angle\": %d}", value);
  mqttClient.publish(MQTT_STATE_TOPIC, buffer);
  Serial.printf("MQTT: %s = %s\n", MQTT_STATE_TOPIC, buffer);

  if (ms == 0) {
    writeToEEPROM(value);
    return;
  }
  
  delay(ms);

  servo.write(previousValue);
  sprintf(buffer, "{\"angle\": %d}", previousValue);
  mqttClient.publish(MQTT_STATE_TOPIC, buffer);
  Serial.printf("MQTT: %s = %s\n", MQTT_STATE_TOPIC, buffer);
}

int readFromEEPROM() {
  Serial.println("EEPROM: Reading value ");
  return (EEPROM.read(0) << 8) + EEPROM.read(1);
}

void writeToEEPROM(int value) {
  Serial.print("EEPROM: Saving value ");
  Serial.println(value);
  EEPROM.write(0, value >> 8);
  EEPROM.write(1, value & 0xFF);
  EEPROM.commit();
}
