#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <DHT.h>
// Node configurations
#define ID 44 //Unique ID of the slave during upload (42-45)
#define DHTPin 4 //DHT11 data pin
#define NEOPIXEL_PIN 27 //NeoPixel LED pin

#define uS_TO_S_FACTOR 1000000  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */
//Wi-Fi channel defined by what wifi is being used
#define CHANNEL 11
// default temperature thresholds
#define ALARM_COLD 0.0
#define ALARM_HOT 30.0
#define WARN_COLD 10.0
#define WARN_HOT 25.0

// NeoPixel strip configuration (single RGB LED)
#define NEOPIXEL_TYPE NEO_RGB + NEO_KHZ800
// Thresholds that store the default thresholds and can be update via Node-RED
float hotAlarm = ALARM_HOT;
float coldAlarm = ALARM_COLD;
//Glabal alarm state shared between slaves (via ESP-NOW)
bool globalAlarm = false; //True if any node/nodes report a high temperature change
uint8_t IDculprit = 0; //ID's the culprit that triggered the alarm
//local alarm state for this node if this node caused the alarm
bool lastlocalAlarm = false;
bool localAlarm = false;
//(unused now was for slave to slave, can be used for future usage like resend alert logic)
unsigned long lastAlertSentMs = 0;
const unsigned long ALERT_RESEND_INTERVAL_MS = 500;
//Commands sent by the master to reset one node or all nodes
typedef struct ResetCommand{
  uint8_t targetID; //targets the ID of the esp and
  uint8_t cmdRes; // resets the esp data
} ResetCommand;
//Telemetry sent from slave to master 
typedef struct struct_message {
    int id; //Node ID
    float humidity; // humidity
    float temp; // temperature
    bool alarm; //local temperature alarm flag
}struct_message; //Data struct that will be send to the master. /!\ Should be the same as the master /!
// alert broadcast from master to slave to notify of an overheating slave
typedef struct MessageAlert {
    uint8_t IDMAC; //ID of culprit node
    bool alertActive;// true = global alarm activate
}MessageAlert;//The data structure that will be sent to the other slave nodes in case of an emergency
//threshold updater broadcasted from Node-RED to master to slaves
typedef struct MessageConfigurateThreshold {
    uint8_t targetID; //Node ID that should be updated
    float HeatAlarm; // New hot alarm limit
    float ColdAlarm; //New cold alarm limit
}MessageConfigurateThreshold;//The data structure that will be sent to the other slave nodes in case of an emergency

struct_message myData; //Telemetry packet to send to the master node
esp_now_peer_info_t peerInfo; //ESP-NOW peer configuration
uint8_t masterMacAddress[] = {0x78, 0xE3, 0x6D, 0x07, 0x21, 0x60}; //Mac address of the master. 84:F7:03:12:AE:88, 78:E3:6D:07:21:60

DHT dhtSensor(DHTPin, DHT11); 
Adafruit_NeoPixel pixel = Adafruit_NeoPixel(1, NEOPIXEL_PIN, NEOPIXEL_TYPE);
void setNeoPixelLED(float t){
  //Neopixel function for changing colors based on the temperature
  //Default NeoPixel color based on the temperature 
  uint8_t r, g, b;
  b = (t < coldAlarm) ? 255 : ((t < WARN_COLD) ? 150 : 0);
  r = (t >= hotAlarm) ? 255 : ((t > WARN_HOT) ? 150 : 0);
  g = (t > coldAlarm) ? ((t <= WARN_HOT) ? 255 : ((t < hotAlarm) ? 150 : 0)) : 0;

  pixel.setPixelColor(0, r, g, b);
  pixel.show();
}

//Local alarm system that loops and flashes this node's LED Green on/off.
// Only used when "This" node's temperature is above threshold
void raiseAlarm(){
  //A function that causes the first esp that detected hot temperature to flash the color green
  //constantly goes through the if statement during the loop to create a flashing effect
  static bool toggle = false;
  toggle = !toggle;

  if(toggle){
    pixel.setPixelColor(0, 255, 0, 0); // green on
  }else{
    pixel.setPixelColor(0, 0, 0, 0); // off
  }
  pixel.show();
}

/** Receieves callback for ESP-NOW and handles the three possible packes
* -MessageAlert: global alarm state that broadcasts from the master
* -ResetCommand: reset this node or all nodes
* - MessageConfigurateThreshold: update the hot and cold thresholds
**/
void espNowcallback(const esp_now_recv_info *info, const uint8_t *data, int len){
  //Creates a callback function for gathering the data 

  //Receives the global alarm broadcast from master
  Serial.printf("data recieved, len=%d\n", len);
  if (len == sizeof(MessageAlert)){
    MessageAlert alert;
    memcpy(&alert, data, sizeof(alert));

    globalAlarm = alert.alertActive;
    IDculprit = alert.IDMAC;

    Serial.print("alert recieved");
    Serial.println(IDculprit);
  }
  //Receives the reset commands from the master/ Node-RED
  else if (len == sizeof(ResetCommand)){
    ResetCommand resMsg;
    memcpy(&resMsg, data, sizeof(resMsg));
    //Reset if addressed to the current node or broadcast (targetID == 0)
    if (resMsg.cmdRes == 1 && (resMsg.targetID == 0 || resMsg.targetID ==ID)){
      Serial.println("Recieved Reset commands, restarting system ...");
      delay(100);
      ESP.restart();
    }
    }
    //Receives the threshold update from master/ Node-RED
    else if (len == sizeof(MessageConfigurateThreshold)){
      MessageConfigurateThreshold modifyThreshold;
      memcpy(&modifyThreshold, data, sizeof(modifyThreshold));
      // Apply the thresholds if addressed to this node or broadcast
      if (modifyThreshold.targetID == 0 || modifyThreshold.targetID ==ID){
        hotAlarm = modifyThreshold.HeatAlarm;
        coldAlarm = modifyThreshold.ColdAlarm;
        Serial.println("Updated Threshold data");
        Serial.println("Hot ALARM");
        Serial.println(hotAlarm);
        Serial.println("Cold ALARM");
        Serial.println(coldAlarm);
        }
      }
    }
//old unsued code from peer to peer connection
// void sendAlertToPeers(bool alertactive){
//   //The function sends the alert to the other slaves of the temperature spike in the local node
//   MessageAlert alert;
//   alert.IDMAC = ID;
//   alert.alertActive = alertactive;

//   for (int i = 0; i < peerslaves; i++){
//     esp_err_t res = esp_now_send(peerSlaveMacs[i], (uint8_t*)&alert, sizeof(alert));
//     if (res != ESP_OK){
//       Serial.printf("Failed to alert the peers %d of the alarm\n", i);
//     }
//   }
// }
//sends a report back to the master whether the ESP-NOW was delivered
void callback_sender(const wifi_tx_info_t *info, esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.println("ESP-NOW packet successfully send");
  } else {
    Serial.println("[X] ESP-NOW packet failed to send");
  }
} // Call back function for when a message is send. 
//Setups the system
void setup() {
  Serial.begin(115200);
  //Wi-Fi station mode and fixed channel so that all ESP-NOW nodes can see each other
  WiFi.mode(WIFI_STA); // Set WiFi to Station mode
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
  //initialises the NeoPixel and clears the LED
  pixel.begin();
  pixel.show();

  //initialise ESP=NOW
  if (esp_now_init() != ESP_OK) { //Check if the esp-now init is okay or not. 
    Serial.println("Error initializing ESP-NOW");
    return;
  }else {
    esp_now_register_recv_cb(espNowcallback); //Registers recieving the espNowCallBack function information 
  }

  esp_now_register_send_cb(callback_sender); //Register the callback function for the sender. 
  
  //Register master as ESP-NOW peer
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, masterMacAddress, 6); //Copy the MAC address in the peer_addr field of peerInfo. 
  peerInfo.encrypt = false; //Set false to the encrypt field of peerInfo. 
  peerInfo.channel = 0; //Set the channel
  if (esp_now_add_peer(&peerInfo) != ESP_OK) { //Check if the peer is successfully added. 
    Serial.println("Error adding ESP-NOW peer");
  }
  //Start the DHT11 sensor
  dhtSensor.begin();
}

void loop() {
  Serial.println(WiFi.channel());

  // read the sensors and build a telemetry packet
  float humid = dhtSensor.readHumidity();
  float temp = dhtSensor.readTemperature();
  lastlocalAlarm = localAlarm;
  myData.id = ID;

  //Handle NaN humidity data if its nan then it prints -100000 (helps with debugging)
  if (!isnan(humid)) {
    myData.humidity = (humid);
  } else {
    myData.humidity = -10000; // handling the case if readHumidity return a NaN
  }

  if (!isnan(temp)) {
    myData.temp = temp;
    //updating the local alarm boolean and embed the data in the telemetry
    localAlarm = (temp> hotAlarm);
    myData.alarm = (temp > hotAlarm);
    // added a millis to help with deep sleep and detecting any signal
    unsigned long now = millis();

    //LED behaviour based on the temperature or recieving an alarm
    if (localAlarm){
      // if our own node is too hot flash green
      raiseAlarm();
    }else if (globalAlarm && ID != IDculprit){
      // else if another node is alarming then stay solid blue on this node
      pixel.setPixelColor(0,0,0,255);
      pixel.show();
    }else{
      //else return to normal operation
      setNeoPixelLED(temp);
    }
    //print the temperature
    Serial.print(temp);
  } else {
    myData.temp= -10000; // handling the case if readTemperature return a NaN

//old peer to peer code
//if statement for detecting the local alarm and alerting the other slave nodes
    // if(localAlarm){
    //   if (now -lastAlertSentMs >=ALERT_RESEND_INTERVAL_MS){
    //     Serial.println("Alert being sent");
    //     sendAlertToPeers(true);
    //     lastAlertSentMs = now;
    //   }
    // }else{
    //   if(lastlocalAlarm){
    //     Serial.println("Sending Alert");
    //     sendAlertToPeers(false);
    //     lastAlertSentMs = 0;
    //   }
    // }
    //if statement for alerting the system 
  }
  //send the telemetry data to master via ESP-NOW
  esp_err_t err_send = esp_now_send(masterMacAddress, (uint8_t *) &myData, sizeof(myData)); 
  if (err_send == ESP_OK) { //Check if the message is successfully send. 
    Serial.println("[+] SUCCESS");
  } else {
    Serial.println("[X] ERROR");
  }
  Serial.printf("Current WiFi channel: %d\n", WiFi.channel());
  //activate the deep sleep logic for energy saving
  static int awakeCycles = 0;
  const int AWAKE_CYCLES_BEFORE_SLEEP = 10;

  bool anyAlarm = globalAlarm || localAlarm;

  if(!anyAlarm){
    // Only count cycles for sleeping when system is calm and not alarm
    awakeCycles++;

    if(awakeCycles >= AWAKE_CYCLES_BEFORE_SLEEP){
      Serial.println("No alarm. Bravo 6 Going Dark...");
      delay(200);
      //configures a wake up timer and enters deep sleep routine 
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      esp_deep_sleep_start();
    }else{
      //stay awake for a bit longer to keep updated values
      delay(300);
    }
  }else{
    //if theres any alarm keep all the nodes awake and responsive
    awakeCycles = 0;
    delay(300);
  }
}
