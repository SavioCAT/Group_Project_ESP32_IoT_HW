#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <DHT.h>

#define ID 42
#define DHTPin 4
#define NEOPIXEL_PIN 27
#define uS_TO_S_FACTOR 1000000  // Conversion factor for micro seconds to seconds
#define TIME_TO_SLEEP  5        // Time ESP32 will go to sleep (in seconds)
#define CHANNEL 6

#define ALARM_COLD 0.0
#define ALARM_HOT 30.0
#define WARN_COLD 10.0
#define WARN_HOT 25.0
#define NEOPIXEL_TYPE NEO_RGB + NEO_KHZ800
#define AWAKE_CYCLES_BEFORE_SLEEP 10

bool globalAlarm = false;
uint8_t IDculprit = 0;
bool lastlocalAlarm = false;
bool localAlarm = false;
unsigned long lastAlertSentMs = 0;
const unsigned long ALERT_RESEND_INTERVAL_MS = 500;
typedef struct struct_message {
    int id;
    float humidity;
    float temp;
    // unsigned char r = 0; // LED RED value
    // unsigned char g = 0; // LED Green value
    // unsigned char b = 0; // LED Blue value
    // bool ledOverriden = false;
}struct_message; //Data struct that will be send to the master. /!\ Should be the same as the master /!\

typedef struct MessageAlert {
    uint8_t IDMAC;
    bool alertActive;
}MessageAlert;

struct_message myData; //Data that will be send. 
esp_now_peer_info_t peerInfo; // Usefull to know if the packet is correctly send. 
uint8_t masterMacAddress[] = {0x78, 0xE3, 0x6D, 0x07, 0x21, 0x60}; //Mac address of the master. 84:F7:03:12:AE:88, 78:E3:6D:07:21:60
uint8_t peerSlaveMacs[][6] = {
  {0x78, 0xE3, 0x6D, 0x11, 0x9C, 0x24},  
  // {0x08, 0xB6, 0x1F, 0x28, 0x58, 0x28},
  {0x84, 0xF7, 0x03, 0x12, 0xAE, 0x88},
  {0x84, 0xF7, 0x03, 0x14, 0x0E, 0x6C}

};

const int peerslaves = sizeof(peerSlaveMacs) / sizeof(peerSlaveMacs[0]);
DHT dhtSensor(DHTPin, DHT11); 
Adafruit_NeoPixel pixel = Adafruit_NeoPixel(1, NEOPIXEL_PIN, NEOPIXEL_TYPE);

void setNeoPixelLED(float t){
  uint8_t r, g, b;

  b = (t < ALARM_COLD) ? 255 : ((t < WARN_COLD) ? 150 : 0);
  r = (t >= ALARM_HOT) ? 255 : ((t > WARN_HOT) ? 150 : 0);
  g = (t > ALARM_COLD) ? ((t <= WARN_HOT) ? 255 : ((t < ALARM_HOT) ? 150 : 0)) : 0;

  pixel.setPixelColor(0, r, g, b);
  pixel.show();
}

void raiseAlarm(){
  static bool toggle = false;
  toggle = !toggle;

  if(toggle){
    pixel.setPixelColor(0, 255, 0, 0);
  }else{
    pixel.setPixelColor(0, 0, 0, 0);
  }
  pixel.show();
}

void callback_receive(const uint8_t *info, const uint8_t *data, int len){
  Serial.printf("data recieved, len=%d\n", len);
  if (len == sizeof(MessageAlert)){
    MessageAlert alert;
    memcpy(&alert, data, sizeof(alert));

    globalAlarm = alert.alertActive;
    IDculprit = alert.IDMAC;

    Serial.print("alert recieved");
    Serial.println(IDculprit);
  }
}

void sendAlertToPeers(bool alertactive){
  MessageAlert alert;
  alert.IDMAC = ID;
  alert.alertActive = alertactive;

  for (int i = 0; i < peerslaves; i++){
    esp_err_t res = esp_now_send(peerSlaveMacs[i], (uint8_t*)&alert, sizeof(alert));
    if (res != ESP_OK){
      Serial.printf("Failed to alert the peers %d of the alarm\n", i);
    }
  }
}

void callback_sender(const uint8_t *info, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("ESP-NOW packet successfully send");
  } else {
    Serial.println("[X] ESP-NOW packet failed to send");
  }
} // Call back function for when a message is send. 

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); // Set WiFi to Station mode
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);

  pixel.begin();
  pixel.show();

  if (esp_now_init() != ESP_OK) { //Check if the esp-now init is okay or not. 
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(callback_receive); //Register the callback function when it receive a message 
  esp_now_register_send_cb(callback_sender); //Register the callback function when it send a message  
  
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, masterMacAddress, 6); //Copy the MAC address in the peer_addr field of peerInfo. 
  peerInfo.encrypt = false; //Set false to the encrypt field of peerInfo. 
  peerInfo.channel = 0; //Set the channel
  if (esp_now_add_peer(&peerInfo) != ESP_OK) { //Check if the peer is successfully added. 
    Serial.println("Error adding ESP-NOW peer");
  }

  for (int i =0; i< peerslaves; i++){
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, peerSlaveMacs[i], 6);
    peerInfo.encrypt = false; //Set false to the encrypt field of peerInfo. 
    peerInfo.channel = 0; //Set the channel
    if (esp_now_add_peer(&peerInfo) != ESP_OK) { //Check if the peer is successfully added. 
      Serial.println("Error adding ESP-NOW peer");
  }
  }
  
  dhtSensor.begin();
}

void loop() {
  Serial.println(WiFi.channel());
  myData.id = ID;

  float humidity = dhtSensor.readHumidity(); // Read humidity
  float temp = dhtSensor.readTemperature(); // Read temperature as Celsius

  if (!isnan(humidity)) { // Check if readHumidity return a valid number
    myData.humidity = humidity;
  } else {
    myData.humidity = -10000; // handling the case if readHumidity return a NaN
  }

  if (!isnan(temp)) { // Check if readTemperature return a valid number
    myData.temp = temp;
    localAlarm = (temp > ALARM_HOT); // Check if local alarm should be raised by verifying if the temperature is above the ALARM_HOT threshold
    unsigned long now = millis(); //Time since the program start

    if(localAlarm){
      if (now - lastAlertSentMs >= ALERT_RESEND_INTERVAL_MS){ // Check if enough time has passed since the last alert was sent
        Serial.println("Alert being sent");
        sendAlertToPeers(true);
        lastAlertSentMs = now;
      }
    }else{
      if(lastlocalAlarm){
        Serial.println("Sending Alert");
        sendAlertToPeers(false);
        lastAlertSentMs = 0;
      }
    }

    lastlocalAlarm = localAlarm;
    if (localAlarm){
      raiseAlarm();
    }else if (globalAlarm && ID != IDculprit){
      pixel.setPixelColor(0,0,0,255);
      pixel.show();
    }else{
      setNeoPixelLED(temp);
    }
    Serial.print(temp);
  } else {
    myData.temp= -10000; // handling the case if readTemperature return a NaN
  }

  esp_err_t err_send = esp_now_send(masterMacAddress, (uint8_t *) &myData, sizeof(myData)); // Send the data to the master with ESP-NOW protocol.
  if (err_send == ESP_OK) { //Check if the message is successfully send. 
    Serial.println("[+] SUCCESS");
  } else {
    Serial.println("[X] ERROR");
  }
  Serial.printf("Current WiFi channel: %d\n", WiFi.channel());

  static int awakeCycles = 0;
  bool anyAlarm = globalAlarm || localAlarm;

  if(!anyAlarm){
    awakeCycles++;

    if(awakeCycles >= AWAKE_CYCLES_BEFORE_SLEEP){
      Serial.println("No alarm. Bravo 6 Going Dark...");
      delay(200);
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
    }else{
      delay(300);
    }
  }else{
    awakeCycles = 0;
    delay(300);
  }
}