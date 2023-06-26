#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "./config/enterprise-wifi-config.h"
#include "./config/mqtt-broker-config.h"

#define BUTTON_PIN 15
#define LED_PIN 10
#define LDR_SENSOR_PIN A0

// WiFi Cred
extern "C" {
#include "user_interface.h"
#include "wpa2_enterprise.h"
#include "c_types.h"
}
char ssid[] = WIFI_SSID;
char identity[] = WIFI_IDENTITY;
char username[] = WIFI_USER;
char password[] = WIFI_PASSWORD;
uint8_t target_esp_mac[6] = {0x24, 0x0a, 0xc4, 0x9a, 0x58, 0x28};

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
int buttonRead = 0;
long lastMeasure = 0;

void setup_wifi() {

  WiFi.mode(WIFI_STA);
  delay(1000);
  Serial.setDebugOutput(true);
  Serial.printf("SDK version: %s\n", system_get_sdk_version());
  Serial.printf("Free Heap: %4d\n",ESP.getFreeHeap());
  
  // Setting ESP into STATION mode only (no AP mode or dual mode)
  wifi_set_opmode(STATION_MODE);

  struct station_config wifi_config;

  memset(&wifi_config, 0, sizeof(wifi_config));
  strcpy((char*)wifi_config.ssid, ssid);
  strcpy((char*)wifi_config.password, password);

  wifi_station_set_config(&wifi_config);
  wifi_set_macaddr(STATION_IF,target_esp_mac);
  

  wifi_station_set_wpa2_enterprise_auth(1);

  // Clean up to be sure no old data is still inside
  wifi_station_clear_cert_key();
  wifi_station_clear_enterprise_ca_cert();
  wifi_station_clear_enterprise_identity();
  wifi_station_clear_enterprise_username();
  wifi_station_clear_enterprise_password();
  wifi_station_clear_enterprise_new_password();
  
  wifi_station_set_enterprise_identity((uint8*)identity, strlen(identity));
  wifi_station_set_enterprise_username((uint8*)username, strlen(username));
  wifi_station_set_enterprise_password((uint8*)password, strlen((char*)password));

  
  wifi_station_connect();
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

// SUB Action
void callback(String topic, byte* message, unsigned int length) {
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }

  Serial.println("[SUB] Topic: " + String(topic) + ". Msg: " + String(messageTemp));

  if (topic == "iot-2/cmd/outdoor_lights/fmt/txt") {
    if (messageTemp == "1") {
      digitalWrite(LED_PIN, LOW);
    }
    else if (messageTemp == "0") {
      digitalWrite(LED_PIN, HIGH);
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
  pinMode(0, INPUT_PULLUP);
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  delay(100);

  if (!client.connected()) {
   reconnect();
  }
  if (!client.loop()) {
   reconnect();
  }

  now = millis();

  buttonRead = digitalRead(0);
  if (buttonRead != buttonState) {
    delay(40);
    client.publish("iot-2/evt/entry_bell/fmt/txt", String(buttonRead).c_str());
    buttonState = buttonRead;
    Serial.println("Bell pressed.");
  }
  
  // Publishes new data every 5 seconds
  if (now - lastMeasure > 5000) {
    lastMeasure = now;
    ldrSensorValue = analogRead(LDR_SENSOR_PIN) / 10.24;

    if (isnan(ldrSensorValue)) {
      Serial.println("Failed to read from LDR sensor!");
      return;
    }

    client.publish("iot-2/evt/outdoor_ldr/fmt/txt", String(ldrSensorValue).c_str());
    
    Serial.println("[PUB] Topic: iot-2/evt/outdoor_ldr/fmt/txt. Msg: " + String(ldrSensorValue));
  }
} 
