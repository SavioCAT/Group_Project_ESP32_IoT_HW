#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

uint8_t masterMacAddress[] = {0x84, 0xF7, 0x03, 0x12, 0xAE, 0x88}; //Mac address of the master. 
//84:F7:03:12:AE:88

typedef struct struct_message {
    int id;
    char msg[64];
}struct_message; //Data struct that will be send. 

struct_message myData; //Data that will be send. 
esp_now_peer_info_t peerInfo; // Usefull to know if the packet is correctly send. 

void callback_sender(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("ESP-NOW packet successfully send");
  } else {
    Serial.println("[X] ESP-NOW packet failed to send");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); // Set WiFi to Station mode

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  } else {
    esp_now_register_send_cb(callback_sender); //Register the callback function for the sender. 
    memcpy(peerInfo.peer_addr, masterMacAddress, 6);
    peerInfo.encrypt = false;
    peerInfo.channel = 0;
    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.println("Error adding ESP-NOW peer");
      return;
    }
  }
}

void loop() {
  delay(1000);
  Serial.println("I'm the sender");
  Serial.println(WiFi.channel());
  strcpy(myData.msg, "Hello World !");
  myData.id = 1;

  esp_err_t err_send = esp_now_send(masterMacAddress, (uint8_t *) &myData, sizeof(myData));
  if (err_send == ESP_OK) {
    Serial.println("[+] SUCCESS");
  } else {
    Serial.println("[X] ERROR");
  }
}