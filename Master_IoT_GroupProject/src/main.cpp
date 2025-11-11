#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>


typedef struct struct_message {
    int id;
    char msg[1024];
} struct_message; //struct to receive the data.

struct_message myData;

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.println("[+] Data received");
  Serial.printf("ID: %d\n", myData.id);
  Serial.printf("Msg: %s\n", myData.msg);
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_MODE_STA); // Set WiFi to Station mode

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  } else {
    esp_now_register_recv_cb(OnDataRecv);
  }
}

void loop() {
  delay(1);
}