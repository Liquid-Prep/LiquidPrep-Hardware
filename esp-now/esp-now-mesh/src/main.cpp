#include <WebServer.h>
#include "SPIFFS.h"
#include <common.h>
#include <esp_wifi.h>

int DEVICE_ID = 1;                   // set device id, need to store in SPIFFS
String DEVICE_NAME = "ZONE_1";       // set device name

String moistureLevel = "";
int airValue = 3440;   // 3442;  // enter your max air value here
int waterValue = 1803; // 1779;  // enter your water value here
int sensorPin = 32;
int soilMoistureValue = 0;
float soilmoisturepercent = 0;
const char* fwVersion = FIRMWARE_VERSION;
DynamicJsonDocument doc(1024);
int espInterval=80000; //interval for reading data

void calculate() {
  int val = analogRead(sensorPin);  // connect sensor to Analog pin

  // soilmoisturepercent = map(soilMoistureValue, airValue, waterValue, 0, 100);
  int valueMinDiff = abs(val - airValue);
  int maxMinDiff = abs(airValue - waterValue);
  soilmoisturepercent = ((float)valueMinDiff / maxMinDiff) * 100;
 
  char str[8];
  if(soilmoisturepercent < 0) {
    soilmoisturepercent = 0;
  } else if(soilmoisturepercent > 100) {
    soilmoisturepercent = 100;
  }
  Serial.printf("sensor reading: %d - %f%\n", val, soilmoisturepercent);  // print the value to serial port
  dtostrf(soilmoisturepercent, 1, 2, str);
  moistureLevel = str;
}

void moistureJson() {
  calculate();
  Serial.printf("sensor reading: %s", moistureLevel);
}

String saveJson() {
  String msg = "";
  File configFile = SPIFFS.open("/config.json", "w+"); 
  if(configFile) {
    doc["deviceId"] = DEVICE_ID;
    doc["deviceName"] = DEVICE_NAME;
    doc["airValue"] = airValue;
    doc["waterValue"] = waterValue;
    doc["sensorPin"] = sensorPin;
    doc["espInterval"] = espInterval;
    doc["receiverMac"] = receiverMac;
    doc["senderMac"] = senderMac;

    serializeJson(doc, configFile);
    msg = "Updated config successfully!";
    configFile.close();
  } else {
    msg = "Failed to open config file for writing!";
    Serial.println(msg);
  }
  return msg;
}
// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Last Packet Send Status: %u, %s\t", receiverAddress, hostMac);
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  Serial.println("");
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  struct_message payload = struct_message();
  memcpy(&payload, incomingData, sizeof(payload));
  Serial.print("Bytes received: ");
  Serial.printf("%d from %s, %s, %d, %d, %s\n", len, payload.name, payload.hostAddress, payload.task, payload.type, payload.msg);
  Serial.printf("=> msgId: %d\n", payload.msgId);
  Serial.println("------");

  if (isMessageSeen(payload.msgId)) {
    Serial.printf("%d from %s, %d Message already seen, ignoring...\n", len, payload.name, payload.task);
    return; // The message is a duplicate, don't send it again
  } else {
    if(payload.hostAddress == hostMac) {
      Serial.println("processing...");
      switch(payload.task) {
        case PING:
          setPayload(payload, DEVICE_ID, DEVICE_NAME, "", hostMac, "", PING_BACK, BROADCAST, DEVICE_NAME, espInterval);
          Serial.printf("%d, %s, %s, %s, %d, %s\n", payload.id,payload.name,payload.hostAddress,payload.senderAddress,payload.task,payload.msg);
          payload.msgId = generateMessageHash(payload);
          esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
        break;
        case QUERY:
          //payload = struct_message(); 
          //payload.type = BROADCAST;
          //payload.task = QUERY_RESULT;
          //payload.senderAddress = hostMac;
          //payload.hostAddress = receiverMac;
          //payload.name = DEVICE_NAME;
          //payload.id = DEVICE_ID;
          //payload.espInterval = espInterval;
          char msg[80];
          sprintf(msg, "%d,%d,%d,%s,%s", airValue, waterValue, sensorPin, senderMac.c_str(), receiverMac.c_str());
          Serial.println(msg);
          setPayload(payload, DEVICE_ID, DEVICE_NAME, "", hostMac, "", QUERY_RESULT, BROADCAST, msg, espInterval);
          payload.msgId = generateMessageHash(payload);
          esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
        break;
        case CALIBRATE_AIR:
        case CALIBRATE_WATER:
          if(payload.task == CALIBRATE_AIR) {
            calibrateAir(airValue, sensorPin);
          } else {
            calibrateWater(waterValue, sensorPin);
          }
          saveJson();
          payload = struct_message(); 
          payload.type = BROADCAST;
          payload.task = CALIBRATE_RESULT;
          payload.senderAddress = hostMac;
          payload.hostAddress = receiverMac;
          payload.name = DEVICE_NAME;
          payload.id = DEVICE_ID;
          sprintf(payload.msg, "%d,%d,%d,%s,%s", airValue, waterValue, sensorPin, senderMac.c_str(), receiverMac.c_str());
          esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
        break;
        case UPDATE_WIFI_CHANNEL:
        break;
        case UPDATE_DEVICE_NAME:
          Serial.printf("update device name: %s\n", payload.name);
          DEVICE_NAME = payload.name;
          saveJson();
        break;
        case UPDATE_DEVICE_ID:
          Serial.printf("update device id: %d\n", payload.id);
          DEVICE_ID = payload.id;
          saveJson();
        break;
        case UPDATE_ESP_INTERVAL:
          Serial.printf("update esp_interval: %d\n", payload.espInterval);
          espInterval = payload.espInterval;
          saveJson();
        break;
        default:
          Serial.println("Nothing to do.");
        break;
      }
    } else {
      if(payload.type == BROADCAST) {
        if (isMessageSeen(payload.msgId)) {
          Serial.printf("%d from %s, %d Message already seen, ignoring...\n", len, payload.name, payload.task);
          return; // The message is a duplicate, don't send it again
        } else {
          Serial.printf("relate broadcast %s from %s\n", payload.msg, payload.name);
          esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
        }
      }
    }
  }
}

void setup() {
  int waitCount = 0;
  delay(1000);
  // Init Serial Monitor
  Serial.begin(115200);

  while (!SPIFFS.begin(true) && waitCount++ < 3) {
    delay(1000);
  }
  File config = SPIFFS.open("/config.json", "r");
  if(config) {
    DeserializationError error = deserializeJson(doc, config);
    if(error) {
      saveJson();
      Serial.println(F("Failed to read file, using default configuration"));
      Serial.println(error.c_str());
      saveJson();
    } else {
      JsonObject obj = doc.as<JsonObject>();
      airValue = doc["airValue"];
      waterValue = doc["waterValue"];
      sensorPin = doc["sensorPin"];
      DEVICE_ID = obj["deviceId"];
      DEVICE_NAME = doc["deviceName"].as<String>();
      espInterval = doc["espInterval"];
      receiverMac = doc["receiverMac"].as<String>();
      senderMac = doc["senderMac"].as<String>();
      stringToInt(receiverMac, receiverAddress);
      stringToInt(senderMac, senderAddress);
    }
    config.close();
  } else {
    saveJson();
  }
Serial.printf("%d, %d, %d, %d, %s, %d, %s, %s\n", airValue,waterValue,sensorPin,DEVICE_ID,DEVICE_NAME,espInterval,receiverMac,senderMac);
  // Set device as a Wi-Fi Station
  Serial.println("Initializing...");
  Serial.println("My MAC address is: " + WiFi.macAddress());
  hostMac = removeFromString(WiFi.macAddress(), (char *)":");
  stringToInt(hostMac, hostAddress);
  stringToInt(receiverMac, receiverAddress);
  Serial.printf("My MAC address is: %s, %u, %u\n", hostMac, hostAddress, receiverAddress);
  Serial.printf("sender: %s, %u, receiver: %s, %u\n", senderMac, senderAddress, receiverMac, receiverAddress);

  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());


  WiFi.mode(WIFI_STA);
//int32_t channel = 1;
//WiFi.printDiag(Serial); // Uncomment to verify channel number before
//esp_wifi_set_promiscuous(true);
//esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
//esp_wifi_set_promiscuous(false);
//WiFi.printDiag(Serial); // Uncomment to verify channel change after  
  WiFi.disconnect();        // we do not want to connect to a WiFi network

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
      Serial.println("Error initializing ESP-NOW");
      return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  // esp_now_set_self_role(MY_ROLE);   
  esp_now_register_recv_cb(OnDataRecv);   // this function will get called once all data is sent

  esp_now_register_send_cb(OnDataSent);

  addPeer(receiverAddress);
  addPeer(broadcastAddress);
  if(senderMac.length() == 12) {
    stringToInt(senderMac, senderAddress);
    addPeer(senderAddress);
  }
}

void loop() {
  calculate();
  // Set values to send
  struct_message payload = struct_message();
  payload.id = DEVICE_ID;
  payload.name = DEVICE_NAME;
  payload.hostAddress = receiverMac;
  payload.senderAddress = hostMac;
  payload.espInterval = espInterval;
  Serial.printf("info: %d, %s, %d, %s, %s, %s\n", espInterval, moistureLevel, payload.id, payload.name, payload.senderAddress, payload.receiverAddress);
  payload.type = BROADCAST;
  payload.moisture = moistureLevel;
  payload.msgId = generateMessageHash(payload);
  esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));

  delay(espInterval);
}
