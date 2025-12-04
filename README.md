# Group Project ESP32 IoT HW
Group project for IoT lecture Heriot Watt

> [!NOTE]
> GROUP MEMBERS
> 1. Savio BOISSINOT H00513570
> 2. Olayinka Abiodun
> 3. Abdul Bahir Abdul Abbas H00507264
> 4. Joseph William Abdo
> 5.
> 6.

<!-- <img width="1648" height="836" alt="image" src="https://github.com/user-attachments/assets/297872e3-2616-4f4f-b5c8-9f6528b0ab8d" /> -->

## Abstract

<p>The main objective of this project was to design a ESP32 network, in order to recover environmental data. In our network we have one master and 4 slaves who are transmitting temperature and humidity data throught ESP-NOW protocol to the master. The main purpose of using this protocol was to make a simple, reliable and energy efficient network.</p><br>

## Code

<p>In our code implementation we used a master-slave model. Where each slave node sends periodical updates about its data to the master node. Each slave node has an identical program except for its identifier. The master centralizes all environmental data and displays it in series and send it to the MQTT broker. Communication between slave and master use the ESP-NOW protocol, enabling fast, low-power transmission.</p>

<ul>
    <li>Master node</li>
    <ul>
        <li>Receive ESP-NOW messages from the slave nodes</li>
        <li>Send update to the MQTT broker</li>
        <li>Show logs on the serial interface</li>
        <li>Collect the alert from the slave node</li>
    </ul>
    <li>Slave node</li>
    <ul>
        <li>Send ESP-NOW message to the master node</li>
        <li>Collect the environmental data from the DHT11 sensor</li>
        <li>Update the LED colour depending on the environmental data</li>
        <li>Send alert to the master node if there is an abnormal data value</li>
    </ul>
</ul> 
<br>

### Mutual code

<p>For the data transmission, we use a C structure crafted in the slave node to handle the message. This C structure is then send by ESP-NOW protocol to the master node. This C structure contain different information such as the identifier of the slave node and the value of the temperature and humidity.</p>

```cpp
typedef struct struct_message {
    int id;
    float humidity;
    float temp;
}struct_message;
```

<p>After we've setup the data structure to handle the ESP-NOW packet between the different nodes. we've to initialise the ESP-NOW protocol on every node. In order to achieve this, we need to set the nodes to WiFi station mode, to set a common WiFi channel between the nodes and start the ESP-NOW on the nodes</p>

```cpp
WiFi.mode(WIFI_STA); // Set WiFi to Station mode
esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE); //Set the Wi-Fi channel 
```

```cpp
if (esp_now_init() != ESP_OK) { //Start and check if the esp-now init is okay or not. 
    Serial.println("Error initializing ESP-NOW");
    return;
}
```

### Slave code

<p>Each node periodically reads its environmental data, creates the data structure, and then sends it to the master via ESP-NOW. A confirmation message is displayed if the transmission is successful. After a successful cycle, the ESP enters deep sleep mode. Each slave ESP is assigned a unique identifier during initialisation, which is useful for the NodeRed dashboard to identify the provenance of the data.</p>

<p>In order to send/receive message by ESP-NOW we have to register callback function to configure the behaviour of the ESP when it have to send or receive a message.</p>

```cpp
esp_now_register_recv_cb(callback_receive); //Register the callback function when it receive a message 
esp_now_register_send_cb(callback_sender); //Register the callback function when it send a message  
```
<p>Once the functions registered, the ESP should do environmental data measurements, check the format of these data, and if it correct, send it to the master</p>

```cpp
float humidity = dhtSensor.readHumidity(); // Read humidity
float temp = dhtSensor.readTemperature(); // Read temperature as Celsius

if (!isnan(humidity)) { // Check if readHumidity return a valid number
    myData.humidity = humidity;
} else {
    myData.humidity = -10000; // handling the case if readHumidity return a NaN
}

if (!isnan(temp)) { // Check if readTemperature return a valid number
    myData.temp = temp;
    /**
     * Code shortened for a better comprehension
     */ 
} else {
    myData.temp= -10000; // handling the case if readTemperature return a NaN
}

esp_err_t err_send = esp_now_send(masterMacAddress, (uint8_t *) &myData, sizeof(myData)); // Send the data to the master with ESP-NOW protocol.
if (err_send == ESP_OK) { //Check if the message is successfully send. 
    Serial.println("[+] SUCCESS");
} else {
    Serial.println("[X] ERROR");
}
```

### Master code

<p>Regarding the code running on the ESP master, we had to connect the master to a Wi-Fi network in order to incorporate this ESP into a WLAN, and thus send messages to the MQTT broker in order to get data visualisation on the Node-RED dashboard.</p>

```cpp
char* ssid = "SSID"; // SSID of the WLAN
char* password = "Password"; // Password for the WLAN 
WiFi.begin(ssid, password); //Connect to the WiFi network
```

<p>Now that the ESP is connected, like the slave code, we have to register the callback function for when the ESP receives ESP-NOW messages. The callback function permits us to process the data structure that the master receives and extract the different elements of it. Once the different elements are extracted, the master crafts a JSON in order to send the acquired data to the MQTT broker.</p>

```cpp
void OnDataRecv(const uint8_t * info, const uint8_t *incomingData, int len) {
  memcpy(&myData, incomingData, sizeof(myData));

  bool alarmHigh = (myData.temp > ALARM_HOT);

  Serial.println("[+] Data received ");
  Serial.printf("ID: %d\n", myData.id);
  Serial.printf("Humidity: %f\n", myData.humidity);
  Serial.printf("Temperature: %f\n", myData.temp);

  doc.clear();
  doc["id"] =  myData.id;
  doc["temp"] =  myData.temp;
  doc["humidity"] =  myData.humidity;
  
  doc["alarm"] = alarmHigh;

  String jsonString; // Create a String object to hold the serialized JSON
  serializeJson(doc, jsonString); // Serialize JSON to string object
  mqttAgent.publish(ESP_MASTER_PUBLISH_TOPIC, jsonString.c_str()); // Publish Object String converted to c string to MQTT topic

  if (alarmHigh){

  }
}
```

## NodeRed interface

<p></p>

## Energy optimisation

<p></p>

## Demonstration

<p></p>

## Physical measurements

<p></p>
