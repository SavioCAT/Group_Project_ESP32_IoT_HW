#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define BROKER_IP "172.20.10.4"
#define BROKER_PORT 1883
#define ESP_MASTER_PUBLISH_TOPIC "iot/master"

void callback(char* topic, byte* payload, unsigned int length);
WiFiClient wifiClient;
JsonDocument doc; // Object JsonDocument, usefull to manipulate Json
PubSubClient mqttAgent(BROKER_IP, BROKER_PORT, callback, wifiClient);


typedef struct struct_message {
    int id;
    float humidity;
    float temp;
}struct_message; //struct to receive the data. /!\ Should be the same as the slave /!\ 

void callback(char* topic, byte* payload, unsigned int length) {
  /**
   * WIP
   * Implement the function when the Master receive a message
   */
}

struct_message myData; //Initialise the data struct to handle the incoming message from slave. 

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.println("[+] Data received ");
  Serial.printf("ID: %d\n", myData.id);
  Serial.printf("Humidity: %f\n", myData.humidity);
  Serial.printf("Temperature: %f\n", myData.temp);
  doc["ID_ESP"] =  myData.id;
  doc["Temp"] =  myData.temp;
  doc["Humidity"] =  myData.humidity;

  String jsonString; // Create a String object to hold the serialized JSON
  serializeJson(doc, jsonString); // Serialize JSON to string object
  mqttAgent.publish(ESP_MASTER_PUBLISH_TOPIC, jsonString.c_str()); // Publish Object String converted to c string to MQTT topic
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); // Set WiFi to Station mode
  
  char* ssid = "iPhone de Savio"; // SSID of the WLAN
  char* password = "Alpha123!"; // Password for the WLAN 
  WiFi.begin(ssid, password); //Connect to the WiFi network
  while(WiFi.status() != WL_CONNECTED){ //Wait until connected
    Serial.println("Trying to connect...");
    delay(100);
  }

  if (esp_now_init() != ESP_OK) { // Check if the esp-now init is okay or not.
    Serial.println("Error initializing ESP-NOW");
    return;
  } else {
    esp_now_register_recv_cb(OnDataRecv); //Set the callback function when the master receive a message. 
  }

  if (mqttAgent.connect("1", NULL, NULL)) {
    Serial.println("MQTT Connected");
    // mqttAgent.subscribe(""); //Add topic to subscribe
  } else {
    Serial.println("MQTT Failed to connect!");
    delay(1000);
    ESP.restart();
  }
}

void loop() {
  if (!mqttAgent.connected()) { // Verify if MQTT publish is still connected, if not, try to reconnect.
    while (!mqttAgent.connected()) {
      if (mqttAgent.connect("1", NULL, NULL)) {
        Serial.println("MQTT Reconnected");
        //mqttAgent.subscribe(""); //Add topic to subscribe
      } else {
        delay(1000);
      }
    }
  }
  Serial.printf("Current WiFi channel: %d\n", WiFi.channel());
  Serial.printf("Mac address: %s\n", WiFi.macAddress());
  mqttAgent.loop();
  delay(1000);
}