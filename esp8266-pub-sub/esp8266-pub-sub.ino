#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "./config/wifi-config.h"
#include "./config/mqtt-broker-config.h"

#define BUTTON_PIN 12
#define LED_PIN 14
#define LDR_SENSOR_PIN A0

// WiFi Cred
const char* ssid = WIFI_SSID;
const char* pwd = WIFI_PASSWORD;

// MQTT Cred
const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;
const char* mqtt_client_id = MQTT_CLIENT_ID;
const char* mqtt_user = MQTT_USER;
const char* mqtt_pwd = MQTT_PASSWORD;

// Instantiate clients
WiFiClient outdoorEspClient;
PubSubClient client(outdoorEspClient);

// Timer aux vars
long now = millis();

int ldrSensorValue = 0;
int buttonState = 0;
long lastMeasure = 0;

void setup_wifi() {
  delay(1000);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pwd);
  while (WiFi.status() != 3) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("WiFi connected - Device Local IP address: ");
  Serial.println(WiFi.localIP());
}

// SUB Action
void callback(String topic, byte* message, unsigned int length) {
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }

  Serial.println("[SUB] Topic: " + String(topic) + ". Msg: " + String(messageTemp));

  if (topic == "iot-2/cmd/entry_bell/fmt/txt") {
    if (messageTemp == "1") {
      digitalWrite(LED_BUILTIN, LOW);
    }
    else if (messageTemp == "0") {
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
}

// Reconnect to MQTT Broker
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_client_id, mqtt_user, mqtt_pwd)) {
      Serial.println("connected");  
      client.subscribe("iot-2/cmd/+/fmt/+");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(". Trying again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  setup_wifi();
  pinMode(LDR_SENSOR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {

  if (!client.connected()) {
   reconnect();
  }
  if (!client.loop()) {
   reconnect();
  }

  now = millis();
  // Publishes new data every 5 seconds
  if (now - lastMeasure > 5000) {
    lastMeasure = now;
    ldrSensorValue = analogRead(LDR_SENSOR_PIN) / 10.24;

    if (isnan(ldrSensorValue)) {
      Serial.println("Failed to read from LDR sensor!");
      return;
    }

    client.publish("iot-2/evt/outdoor_ldr/fmt/txt", String(ldrSensorValue).c_str());
    
    Serial.println("[PUB] Topic: outdoor/ldr. Msg: " + String(ldrSensorValue));
  }
} 
