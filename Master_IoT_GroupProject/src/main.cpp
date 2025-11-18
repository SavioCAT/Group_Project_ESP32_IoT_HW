#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>


typedef struct struct_message {
    int id;
    int humidity;
    int temp;
}struct_message; //struct to receive the data. /!\ Should be the same as the slave /!\ 

struct_message myData; //Initialise the data struct to handle the incoming message from slave. 

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.println("[+] Data received ");
  Serial.printf("ID: %d\n", myData.id);
  Serial.printf("Humidity: %d\n", myData.humidity);
  Serial.printf("Temperature: %d\n", myData.temp);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); // Set WiFi to Station mode

  if (esp_now_init() != ESP_OK) { // Check if the esp-now init is okay or not.
    Serial.println("Error initializing ESP-NOW");
    return;
  } else {
    esp_now_register_recv_cb(OnDataRecv); //Set the callback function when the master receive a message. 
  }
}

void loop() {
  delay(1);
}