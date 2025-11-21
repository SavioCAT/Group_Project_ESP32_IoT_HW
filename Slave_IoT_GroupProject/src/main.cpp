#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <DHT.h>

#define ID 42
#define DHTPin 4
#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */

typedef struct struct_message {
    int id;
    float humidity;
    float temp;
}struct_message; //Data struct that will be send to the master. /!\ Should be the same as the master /!\

struct_message myData; //Data that will be send. 
esp_now_peer_info_t peerInfo; // Usefull to know if the packet is correctly send. 
uint8_t masterMacAddress[] = {0x78, 0xE3, 0x6D, 0x07, 0x21, 0x60}; //Mac address of the master. 84:F7:03:12:AE:88, 78:E3:6D:07:21:60
DHT dhtSensor(DHTPin, DHT11);

void callback_sender(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("ESP-NOW packet successfully send");
  } else {
    Serial.println("[X] ESP-NOW packet failed to send");
  }
} // Call back function for when a message is send. 

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); // Set WiFi to Station mode

  if (esp_now_init() != ESP_OK) { //Check if the esp-now init is okay or not. 
    Serial.println("Error initializing ESP-NOW");
    return;
  } else {
    esp_now_register_send_cb(callback_sender); //Register the callback function for the sender. 
    memcpy(peerInfo.peer_addr, masterMacAddress, 6); //Copy the MAC address in the peer_addr field of peerInfo. 
    peerInfo.encrypt = false; //Set false to the encrypt field of peerInfo. 
    peerInfo.channel = 0; //Set the channel
    if (esp_now_add_peer(&peerInfo) != ESP_OK) { //Check if the peer is successfully added. 
      Serial.println("Error adding ESP-NOW peer");
      return;
    }
  }
  
  dhtSensor.begin();
}

void loop() {
  Serial.println(WiFi.channel());
  //delay(1000);
  myData.id = ID;
  Serial.print(dhtSensor.readTemperature());

  if (!isnan(dhtSensor.readHumidity())) {
    myData.humidity = dhtSensor.readHumidity();
  } else {
    myData.humidity = -10000; // handling the case if readHumidity return a NaN
  }

  if (!isnan(dhtSensor.readTemperature())) {
    myData.temp = dhtSensor.readTemperature();
  } else {
    myData.temp= -10000; // handling the case if readTemperature return a NaN
  }

  esp_err_t err_send = esp_now_send(masterMacAddress, (uint8_t *) &myData, sizeof(myData)); 
  if (err_send == ESP_OK) { //Check if the message is successfully send. 
    Serial.println("[+] SUCCESS");
  } else {
    Serial.println("[X] ERROR");
  }

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  esp_deep_sleep_start();
}