<img width="1360" height="1010" alt="image" src="https://github.com/user-attachments/assets/ad028eb9-fe3d-4084-8c68-5f80b1f5149b" />

# Group Project ESP32 IoT HW

Group project for IoT lecture Heriot Watt

> [!NOTE]
> GROUP MEMBERS
> 1. Savio BOISSINOT H00513570
> 2. Olayinka Abiodun
> 3. Abdul Bahir Abdul Abbas H00507264
> 4. Joseph William Abdo H00389925
> 5.
> 6.

## Abstract

<p>The main objective of this project was to design an ESP32 network to recover environmental data. In our network, we have one master and four slaves that transmit temperature and humidity data via the ESP-NOW protocol to the master. The main purpose of using this protocol was to make a simple, reliable and energy-efficient network.</p>

## Code

<p>In our code implementation, we used a master-slave model. Where each slave node sends periodical updates about its data to the master node, each slave node has an identical program except for its identifier. The master centralises all environmental data and displays it in series, and sends it to the MQTT broker. Communication between slave and master uses the ESP-NOW protocol, enabling fast, low-power transmission.</p>

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
### Updated Implementation

<p>In the updated implementation of the code, the temperature threshold is no longer fixed in the code and is now more dynamic, allowing Node-RED to update the hot alarm and cold alarm limits through a form</p>

<p>First, the code creates a topic for Node-RED to send as a JSON payload to</p>

```cpp
iot/config/threshold/all
```
<p>An example of what it would look like would be</p>

```json
{
"heat": 30.5,
"cold": 5.0
}
```
<p>The master then parses the payload and broadcasts it as a MessageConfigurateThreshold structure using ESP-NOW:</p>

```cpp
typedef struct MessageConfigurateThreshold {
    uint8_t targetID;
    float HeatAlarm;
    float ColdAlarm;
}MessageConfigurateThreshold;
```
<p>Each slave updates its internal alarm thresholds in real-time, without reflashing the ESP32. This feature allows remote threshold tuning for any condition and can be viewed in real time through the dashboard</p>

## NodeRed interface

<img width="1360" height="1010" alt="image" src="https://github.com/user-attachments/assets/de7b1832-5eb3-4c86-ae92-29aa97eba144" />


<p>The figure above shows the Node-RED flow used to visualise and control the ESP32 sensor network, where the ESP32 master publishes all the gathered sensor readings as a JSON object to the MQTT Mosquitto server using multiple topics, and Node-RED subscribes and publishes topic data to the ESP32. Each slave node has its own ID (ID 42-45), with its own dashboard panels showing temperature and humidity over time. Additionally, Node-RED has a threshold-update, reset and alerts system where the alert system is triggered if one of the slaves goes over the temperature and sets an alarm visually on screen, while the master sends the alarm data via ESP-NOW using the slave MACs </p>

<img width="1073" height="668" alt="image" src="https://github.com/user-attachments/assets/8c54c60c-0a61-4201-aa6c-9cc974974f58" />

<p>The image above shows the key C data structures used by both the ESP32 and slave nodes. These structures are serialised into JSON and sent to Node-RED via the mosquitto MQTT, and transmitted into raw bytes over ESP-NOW for device-to-device connection </p>

### The structures include:

- <strong> ResetCommand</strong>: instructs one or all the slaves to restart the system
- <strong>MessageAlert</strong>: communicates whether a slave has triggered a high-temperature alarm and which ID it was from
- <strong> MessageConfigurateThreshold</strong>: distributes the new threshold values for hot and cold from the Node-RED form to all the slaves or a specific slave
- <strong>struct_message</strong>: the main telemetry structure that contains the Node-ID, Humidity, Temperature, and Alarm Flag.
<p>These structures ensure consistent data is formatted and sent across the whole system of Node-RED, the master ESP32, MQTT, MongoDB, and all the slave nodes</p>

## Energy optimisation

<p>We have optimised the energy consumption of slave nodes by using deep sleep, a mode in which the microcontroller shuts down almost all of its modules and consumes only a few microamperes.</p>

<p>In our code, the ESP32 remains awake only to read environmental data, send data to the master, and check for alarms. If there are no local or global alarms, it counts the activity cycles during which it is awake, and once a certain number is reached, it automatically goes into deep sleep.</p>

```cpp
esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
esp_deep_sleep_start(); // Set the ESP32 in deep sleep mode for the time setup in esp_sleep_enable_timer_wakeup()
```

<p>In addition, using ESP-NOW instead of a Wi-Fi connection also allows us to save energy. </p>

![WhatsApp Image 2025-12-05 at 20 03 23_f840bd97](https://github.com/user-attachments/assets/4e4bdf8b-1475-49e8-83bf-c015fd43de06)
<img width="1280" height="960" alt="image" src="https://github.com/user-attachments/assets/e7282130-b8fe-4b32-a368-c84a2120a4d5" />
As demonstrated above, the measured milliamps of the system, where if it were awake, it produces 38mA, and when in deep sleep, it produces 0.0002 mA and the duty cycle from the code provides 3s active/ 5 seconds of sleep giving it a current of 14 mA assuming the system utilises a 1000 mAh battery corresponding to an estimated 70 hours or 3 days under normal conditions

## Demonstration

<p></p>

## Physical measurements

<p></p>
