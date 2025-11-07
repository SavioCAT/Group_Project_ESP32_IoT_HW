#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>

typedef struct struct_message {
    int id;
    char msg[1024];
};

void callback_sender(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("ESP-NOW packet successfully send");
  } else {
    Serial.println("[X] ESP-NOW packet failed to send");
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_MODE_STA); // Set WiFi to Station mode

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
}

void loop() {
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  delay(10000);
}