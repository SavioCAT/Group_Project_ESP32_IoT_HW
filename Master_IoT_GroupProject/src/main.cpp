#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
//MQTT broker configuration
#define BROKER_IP "10.70.166.49"
#define BROKER_PORT 1883
#define ESP_MASTER_PUBLISH_TOPIC "iot/master"

//ESP-NOW peer info structure
esp_now_peer_info_t peerInfo;

//declares the callback of the MQTT server
void callback(char* topic, byte* payload, unsigned int length);

//creates the MQTT connection and JSON object
WiFiClient wifiClient;
JsonDocument doc; // Object JsonDocument, usefull to manipulate Json
PubSubClient mqttAgent(BROKER_IP, BROKER_PORT, callback, wifiClient);

//----SLAVE MAC ADDRESSES (ESP-NOW peer) ----//
uint8_t peerSlaveMacs[][6] = {
  //The slave MAC addresses
  {0x78, 0xE3, 0x6D, 0x11, 0x9C, 0x24},  
  {0x08, 0xB6, 0x1F, 0x28, 0x58, 0x28},
  {0x84, 0xF7, 0x03, 0x14, 0xA7, 0xA0},
  {0x84, 0xF7, 0x03, 0x14, 0x0E, 0x6C}

};
//
const int peerslaves = sizeof(peerSlaveMacs) / sizeof(peerSlaveMacs[0]);// updates the amount of slave mac addresses the ESP will communicate with

//Command structure to reset all slaves
typedef struct ResetCommand{
  uint8_t targetID; //targets the ID of the esp and
  uint8_t cmdRes; // resets the esp data
} ResetCommand;
//Message alert sent from master to slaves
typedef struct MessageAlert {
    uint8_t IDMAC;
    bool alertActive;
}MessageAlert;//The data structure that will be sent to the other slave nodes in case of an emergency

//Threshold data that changes and modifies the threshold of the slaves from Node-RED
typedef struct MessageConfigurateThreshold {
    uint8_t targetID;
    float HeatAlarm;
    float ColdAlarm;
}MessageConfigurateThreshold;

//Structure recieved from slaves containing telemerty
typedef struct struct_message {
    int id;  
    float humidity;
    float temp;
    bool alarm; //alarm to detect if slaves has temperature over the limit
}struct_message; //struct to receive the data. /!\ Should be the same as the slave /!\ 

//global variable storing the latest recieved message
struct_message myData; //Initialise the data struct to handle the incoming message from slave. 


//void used to gather the MQTT callback data
void callback(char* topic, byte* payload, unsigned int length) {
  //if statement that listens to the reset topic 
  if (String(topic).startsWith("iot/command/reset/")){
    //extracts the ID at the end of the topic
    String tStr(topic);
    int backSlash = tStr.lastIndexOf('/');
    String res = tStr.substring(backSlash +1);

    uint8_t targetID = 0;
    if(!res.equals("all")){
      targetID = (uint8_t)res.toInt();
    }
    //build reset command structre
    ResetCommand resMsg;
    resMsg.targetID = targetID;
    resMsg.cmdRes = 1;
    
    //sends reset to all slave MACs
    for (int i = 0; i < peerslaves; i++){
      esp_now_send(peerSlaveMacs[i], (uint8_t*)&resMsg, sizeof(resMsg));
  }
  }//If statement listening for the threshold parameter updates
  else if (String(topic).startsWith("iot/config/threshold/all")){

    //extracts ID (O = broadcast to all slaves)
    String tStr(topic);
    int backSlash = tStr.lastIndexOf('/');
    String res = tStr.substring(backSlash +1);

    uint8_t targetID = 0;
    if(!res.equals("all")){
      targetID = (uint8_t)res.toInt();
    }

    // Parses the incoming JSON data from Node-RED and checks for errors
    StaticJsonDocument<128> jsonDoc;
    DeserializationError err = deserializeJson(jsonDoc, payload, length);
    // if theres an error it prints an error and notifies the system
    if (err){
      Serial.print(F("Threshold JSON parse error:"));
      Serial.println(err.c_str());
      return;
    }

    //builds the threshold message
    MessageConfigurateThreshold modifyThreshold;
    modifyThreshold.targetID = targetID;
    modifyThreshold.HeatAlarm = jsonDoc["heat"];
    modifyThreshold.ColdAlarm = jsonDoc["cold"];

    Serial.print("Detecting New thersholds: heat=");
    Serial.print(modifyThreshold.HeatAlarm);
    Serial.print(" cold=");
    Serial.println(modifyThreshold.ColdAlarm);
    
    //Send thresholds to every slave
    for (int i = 0; i < peerslaves; i++){
      esp_now_send(peerSlaveMacs[i], (uint8_t*)&modifyThreshold, sizeof(modifyThreshold));
  }
  /**
   * WIP
   * Implement the function when the Master receive a message
   */
}
}

//Void that handles the recieved data from the ESP-NOW slaves 
void OnDataRecv(const esp_now_recv_info * info, const uint8_t *incomingData, int len) {
  
  //copy the raw bytes to structure
  memcpy(&myData, incomingData, sizeof(myData));

  //Gets the alarm state from slave
  bool alarmHigh = myData.alarm;

  //prints the telemetry to serial for debugging issues
  Serial.println("[+] Data received ");
  Serial.printf("ID: %d\n", myData.id);
  Serial.printf("Humidity: %f\n", myData.humidity);
  Serial.printf("Temperature: %f\n", myData.temp);

  //The entire code below publishes to the MQTT and Node-RED with the JSON object
  doc.clear();
  doc["id"] =  myData.id;
  doc["temp"] =  myData.temp;
  doc["humidity"] =  myData.humidity;
  doc["alarm"] = alarmHigh;

  String jsonString; // Create a String object to hold the serialized JSON
  serializeJson(doc, jsonString); // Serialize JSON to string object
  mqttAgent.publish(ESP_MASTER_PUBLISH_TOPIC, jsonString.c_str()); // Publish Object String converted to c string to MQTT topic

  //Broadcasts any alerts of high temperatures to the slaves 
  MessageAlert alert;
  alert.IDMAC = myData.id;
  alert.alertActive = alarmHigh;

  for (int i = 0; i < peerslaves; i++){
    esp_now_send(peerSlaveMacs[i], (uint8_t*)&alert, sizeof(alert));
  }
}

// Setup of the code
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA); // Set WiFi to Station mode
  
  char* ssid = "Mohammed_5G_joe"; // SSID of the WLAN
  char* password = "Dubai2020"; // Password for the WLAN 
  WiFi.begin(ssid, password); //Connect to the WiFi network
  Serial.print("WiFi Channel: ");
  Serial.println(WiFi.channel());
  Serial.print("Connecting to: ");

  mqttAgent.setCallback(callback);//Makes sure the system connects before subscribing to anything

  while(WiFi.status() != WL_CONNECTED){ //Wait until connected
    Serial.println("Trying to connect...");
    delay(100);
  
  }

  Serial.println("Connected !");

  if (esp_now_init() != ESP_OK) { // Check if the esp-now init is okay or not.
    Serial.println("Error initializing ESP-NOW");
    return;
  } else {
    //Gathers the recieved data from the slaves and Node-RED
    esp_now_register_recv_cb(OnDataRecv); 
  }
  memset(&peerInfo, 0, sizeof(peerInfo));

  //adds all the slaves as ESP-NOW peers
  for (int i =0; i< peerslaves; i++){
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, peerSlaveMacs[i], 6);//Copies the SLAVE addresses in the peer_addr field of peerInfo
    peerInfo.encrypt = false; //Set false to the encrypt field of peerInfo. 
    peerInfo.channel = 0; //Set the channel
    
    if (esp_now_add_peer(&peerInfo) != ESP_OK) { //Check if the peer is successfully added. 
      Serial.println("Error adding ESP-NOW peer");
      Serial.println(i);
    }

  }
  //connects the master to the MQTT broker and subscribes to the topics for detecting
  // the new thresholds and reset data
  if (mqttAgent.connect("1", NULL, NULL)) {
    Serial.println("MQTT Connected");
    mqttAgent.subscribe("iot/command/reset/#");
    mqttAgent.subscribe("iot/config/threshold/all");
    // mqttAgent.subscribe(""); //Add topic to subscribe
  } else {
    Serial.println("MQTT Failed to connect!");
    delay(1000);
    ESP.restart();
  }

}
//void loop that loops through all the code
void loop() {
  // Verify if MQTT publish is still connected, if not, try to reconnect.
  if (!mqttAgent.connected()) { 
    while (!mqttAgent.connected()) {
      if (mqttAgent.connect("1", NULL, NULL)) {
        Serial.println("MQTT Reconnected");
        mqttAgent.subscribe("iot/command/reset/#");
        mqttAgent.subscribe("iot/config/threshold/all");
      } else {
        delay(1000);
      }
    }
  }
  //shows that the wifi has connected and handles the MQTT messages
  Serial.println(WiFi.channel());
  mqttAgent.loop();
  delay(50);
}
