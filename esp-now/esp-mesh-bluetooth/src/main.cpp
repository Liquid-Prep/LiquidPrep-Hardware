#include <WebServer.h>
#include "SPIFFS.h"
#include <common.h>
#include <esp_wifi.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

int DEVICE_ID = 5;             // set device id, need to store in SPIFFS
String DEVICE_NAME = "Z5"; // set device name

String moistureLevel = "";
int airValue = 3440;   // 3442;  // enter your max air value here
int waterValue = 2803; // 1779;  // enter your water value here
int sensorPin = 32;
int soilMoistureValue = 0;
int wifiChannel = WIFI_CHANNEL;
float soilmoisturepercent = 0;
const char *fwVersion = FIRMWARE_VERSION;
DynamicJsonDocument doc(1024);
int espInterval = 80000; // interval for reading data
uint8_t gatewayMacAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
String gatewayMac = "7821848D8840";

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCharacteristic;
BLEServer *pServer = nullptr;


String saveJson() {
  String msg = "";
  File configFile = SPIFFS.open("/config.json", "w+");
  if (configFile)
  {
    doc["deviceId"] = DEVICE_ID;
    doc["deviceName"] = DEVICE_NAME;
    doc["airValue"] = airValue;
    doc["waterValue"] = waterValue;
    doc["sensorPin"] = sensorPin;
    doc["espInterval"] = espInterval;
    doc["wifiChannel"] = wifiChannel;
    doc["receiverMac"] = receiverMac;
    doc["senderMac"] = senderMac;

    serializeJson(doc, configFile);
    msg = "Updated config successfully!";
    configFile.close();
  }
  else
  {
    msg = "Failed to open config file for writing!";
    Serial.println(msg);
  }
  return msg;
}
void setWifiChannel(int32_t channel = 11) {
  WiFi.mode(WIFI_STA);
  // TODO: get channel programmatically
  //int32_t channel = getWiFiChannel(WIFI_SSID);
  WiFi.printDiag(Serial); // Uncomment to verify channel number before
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
//WiFi.printDiag(Serial); // Uncomment to verify channel change after  
  WiFi.disconnect();        // we do not want to connect to a WiFi network
} 

void updateWifiChannel(int channel) {
  struct_message payload = struct_message();
  char msg[80];
  sprintf(msg, "%d", channel);
  setPayload(payload, DEVICE_ID, DEVICE_NAME, "", hostMac, "", UPDATE_WIFI_CHANNEL, BROADCAST, msg, espInterval, 0);

  Serial.printf("\nwifi: %d, %d, %s, %d, %d, %s, %s\n", espInterval, payload.from, payload.msg, payload.task, payload.type, payload.name, payload.senderAddress);
  payload.msgId = generateMessageHash(payload);
  esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));

  // Broadcast to newly joined or to any esp still listening on default wifi channel 11
  if(wifiChannel != WIFI_CHANNEL) {
    setWifiChannel(WIFI_CHANNEL);
    esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
  }
}

void broadcastWifiResult(int channel) {
  struct_message payload = struct_message();
  char msg[80];
  sprintf(msg, "Updated wifi: from %d to %d", wifiChannel, channel);
  setPayload(payload, DEVICE_ID, DEVICE_NAME, "", hostMac, "", UPDATE_WIFI_RESULT, BROADCAST, msg, espInterval, WEB_REQUEST_RESULT);

  Serial.printf("\n%s\n", payload.msg);
  payload.msgId = generateMessageHash(payload);
  esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
}

void processWifiUpdates(int channel) {
  Serial.printf("%d\n", channel);
  updateWifiChannel(channel);
  delay(300);
  setWifiChannel(channel);
  broadcastWifiResult(channel);
  wifiChannel = channel;
  saveJson();
}

void calibrateSensor(int mode) {
  struct_message payload = struct_message();
  String msg = "";
  char str[80];
  if(mode == CALIBRATE_AIR) {
    sprintf(str, "%d", airValue);
    std::string s(str);
    msg += "Air: Old=" + String(s.c_str());
    calibrateAir(airValue, sensorPin);
    sprintf(str, "%d", airValue);
    std::string s2(str);
    msg = msg + ", New=" + String(s2.c_str());
  } else {
    sprintf(str, "%d", waterValue);
    std::string s(str);
    msg += "Water: Old=" + String(s.c_str());
    calibrateWater(waterValue, sensorPin);
    sprintf(str, "%d", waterValue);
    std::string s2(str);
    msg += ", New=" + String(s2.c_str());
  }
  saveJson();
  setPayload(payload, DEVICE_ID, DEVICE_NAME, "", hostMac, "", CALIBRATE_RESULT, BROADCAST, msg, espInterval, WEB_REQUEST_RESULT);

  payload.msgId = generateMessageHash(payload);
  Serial.printf("\n%s, %d\n\n", payload.msg, payload.msgId);
  esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
}
void calibrateByPercentage(int percent) {
  struct_message payload = struct_message();
  String msg = "";
  char str[80];
  sprintf(str, "%d", waterValue);
  std::string s(str);
  msg += "Water: Old=" + String(s.c_str());

  int val = analogRead(sensorPin); // connect sensor to Analog pin
  int valueMinDiff = abs(val - airValue);
  int maxMinDiff = valueMinDiff * 100 / percent;
  waterValue = abs(airValue - maxMinDiff); 
  sprintf(str, "%d", waterValue);
  std::string s2(str);
  msg += ", New=" + String(s2.c_str());

  saveJson();
  setPayload(payload, DEVICE_ID, DEVICE_NAME, "", hostMac, "", CALIBRATE_RESULT, BROADCAST, msg, espInterval, WEB_REQUEST_RESULT);
}
class BLECallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();
    DynamicJsonDocument pdoc(512);
    char payload[80];
    int j = 0;

    if (value.length() > 0)
    {
      Serial.printf("*********: %d\n", value.length());
      Serial.print("New value: ");
      for (int i = 0; i < value.length(); i++) {
        if(i%2 == 0) {
          payload[j++] = value[i];
          Serial.print(value[i]);          
        }
      }
      payload[j] = '\0';
      Serial.println();
      Serial.println("*********");

      Serial.printf("%s\n", payload);
      deserializeJson(pdoc, payload);
      Serial.printf("%s, %s\n", pdoc["type"].as<String>(), pdoc["value"].as<String>());

      if(pdoc["type"].as<String>() == "CHANNEL") {
        int channel =  atoi(pdoc["value"].as<String>().c_str());
        processWifiUpdates(channel);
      } else if(pdoc["type"].as<String>() == "CALIBRATE") {
        int mode = pdoc["value"].as<String>() == "water" ? CALIBRATE_WATER : CALIBRATE_AIR;
        calibrateSensor(mode);
      }
    }
  }
};
void calculate() {
  int val = analogRead(sensorPin); // connect sensor to Analog pin
  char str[8];
  if(val >= airValue) {
    soilmoisturepercent = 0;
  } else if(val <= waterValue) {
    soilmoisturepercent = 100.00;
  } else {
    int diff = airValue - waterValue;
    soilmoisturepercent = ((float)(airValue - val) / diff) * 100;
  }
  Serial.printf("sensor reading: %d - %f%\n", val, soilmoisturepercent); // print the value to serial port
  dtostrf(soilmoisturepercent, 1, 2, str);
  moistureLevel = str;
}
void calculate2()
{
  int val = analogRead(sensorPin); // connect sensor to Analog pin

  // soilmoisturepercent = map(soilMoistureValue, airValue, waterValue, 0, 100);
  int valueMinDiff = abs(val - airValue);
  int maxMinDiff = abs(airValue - waterValue);
  soilmoisturepercent = ((float)valueMinDiff / maxMinDiff) * 100;

  char str[8];
  if (soilmoisturepercent < 0)
  {
    soilmoisturepercent = 0;
  }
  else if (soilmoisturepercent > 100)
  {
    soilmoisturepercent = 100;
  }
  Serial.printf("sensor reading: %d - %f%\n", val, soilmoisturepercent); // print the value to serial port
  dtostrf(soilmoisturepercent, 1, 2, str);
  moistureLevel = str;
}

void moistureJson()
{
  calculate();
  Serial.printf("sensor reading: %s", moistureLevel);
}

uint16_t connId = 0;  // This should be globally declared if it needs to be accessed in other functions

void enableBluetooth() {
  char bleName[80] = "";
  sprintf(bleName, "ESP32-LiquidPrep-%s", DEVICE_NAME);
  Serial.printf("Starting BLE work!  %s\n", bleName);

  BLEDevice::init(bleName);
  pServer = BLEDevice::createServer();

  // Keep track of connection ID when a device connects
  // pServer->setCallbacks(new MyServerCallbacks()); // you may need to define the callback to get the connection ID

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new BLECallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it on your phone!");
}


void disableBluetooth() {
  if (pServer) {
    uint16_t connId = 0;  // Replace with actual connection ID

    pServer->getAdvertising()->stop();
    
    pServer->disconnect(connId); // Now passing a connection ID

    delete pServer;
    pServer = nullptr; // Reset pServer to nullptr

    Serial.println("Bluetooth disabled");
  } else {
    Serial.println("Bluetooth is not enabled");
  }
}



// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  Serial.printf("Last Packet Send Status: %u, %s\t", receiverAddress, hostMac);
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  Serial.println("");
}


void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  struct_message payload = struct_message();
  memcpy(&payload, incomingData, sizeof(payload));
  Serial.print("Bytes received: ------\n");
  Serial.printf("%d, moisture: %s from %s, %s, %d, %d, %d, %s\n", len, payload.moisture, payload.name, payload.hostAddress, payload.task, payload.type, payload.from, payload.msg);
  Serial.printf("=> msgId: %d\n", payload.msgId);
  Serial.println("------\n");

  if (isMessageSeen(payload.msgId)) {
    Serial.printf("%d from %s, %d, Message %d already seen, ignoring...\n", len, payload.name, payload.task, payload.msgId);
    return; // The message is a duplicate, don't send it again
  } else {
    if(payload.hostAddress == hostMac) {
      Serial.println("processing...\n");
      int from = payload.from == WEB_REQUEST ? WEB_REQUEST_RESULT : NO_TASK;
      char msg[80];
      switch(payload.task) {
        case PING:
          setPayload(payload, DEVICE_ID, DEVICE_NAME, "", hostMac, "", PING_BACK, BROADCAST, DEVICE_NAME, espInterval, from);
          Serial.printf("%d, %s, %s, %s, %d, %s\n", payload.id,payload.name,payload.hostAddress,payload.senderAddress,payload.task,payload.msg);
          payload.msgId = generateMessageHash(payload);
          esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
        break;
        case QUERY:
          sprintf(msg, "%d,%d,%d,%d,%s,%s", airValue, waterValue, sensorPin, wifiChannel, hostMac.c_str(), "");
          Serial.printf("msg: %s -> %d", msg, from);
          setPayload(payload, DEVICE_ID, DEVICE_NAME, "", hostMac, "", QUERY_RESULT, BROADCAST, msg, espInterval, from);
          payload.msgId = generateMessageHash(payload);
          esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
        break;
        case CALIBRATE_AIR:
        case CALIBRATE_WATER:
          calibrateSensor(payload.task);
        break;
        case GET_MOISTURE:
          calculate();
          sprintf(msg, "%d,%d,%d,%d,%s,%s,%s", airValue, waterValue, sensorPin, wifiChannel, hostMac.c_str(), "", moistureLevel);
          setPayload(payload, DEVICE_ID, DEVICE_NAME, "", hostMac, "", MOISTURE_RESULT, BROADCAST, msg, espInterval, from);
          payload.msgId = generateMessageHash(payload);
          esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
        break;
        case UPDATE_DEVICE_NAME:
          Serial.printf("update device name: %s\n\n", payload.name);
          DEVICE_NAME = payload.name;
          saveJson();
        break;
        case UPDATE_DEVICE_ID:
          Serial.printf("update device id: %d\n\n", payload.id);
          DEVICE_ID = payload.id;
          saveJson();
        break;
        case UPDATE_ESP_INTERVAL:
          Serial.printf("update esp_interval: %d\n\n", payload.espInterval);
          espInterval = payload.espInterval;
          saveJson();
        break;
        case ENABLE_BLUETOOTH:
          Serial.println("enable bluetooth");
          enableBluetooth();
        break;
        case DISABLE_BLUETOOTH:
          Serial.println("disable bluetooth");
          disableBluetooth();
        break;
        default:
          Serial.println("Nothing to do.\n");
        break;
      }
    } else {
      if(payload.type == BROADCAST) {
        if(payload.task == UPDATE_WIFI_CHANNEL) {
          int32_t channel = atoi(payload.msg);
          Serial.printf("Wifi channel:  %d", channel);
          if(channel != wifiChannel) {
            processWifiUpdates(channel);
          } else {
            Serial.println("Same wifi channel, nothing to do.");
          }
        } else {
          Serial.printf("relate broadcast %d from %s, %s, %d, %d, %d, %s\n\n", payload.msgId, payload.name, payload.senderAddress, payload.task, payload.type, payload.from, payload.msg);
          esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
        } 
      } else {
          Serial.println("Else nothing to do.\n");
      }
    }
  }
}

void setup()
{
  int waitCount = 0;
  delay(1000);
  // Init Serial Monitor
  Serial.begin(115200);

  while (!SPIFFS.begin(true) && waitCount++ < 3)
  {
    delay(1000);
  }
  File config = SPIFFS.open("/config.json", "r");
  if (config)
  {
    DeserializationError error = deserializeJson(doc, config);
    if (error)
    {
      Serial.println(F("Failed to read file, using default configuration"));
      Serial.println(error.c_str());
      saveJson();
    } else {
      JsonObject obj = doc.as<JsonObject>();
      if(!doc["deviceName"] || doc["espInterval"] <= 0) {
        saveJson();  //data corrupted, use default values
      } else {
        airValue = doc["airValue"];
        waterValue = doc["waterValue"];
        sensorPin = doc["sensorPin"];
        DEVICE_ID = obj["deviceId"];
        DEVICE_NAME = doc["deviceName"].as<String>();
        espInterval = doc["espInterval"];
        wifiChannel = doc["wifiChannel"];
        receiverMac = doc["receiverMac"].as<String>();
        senderMac = doc["senderMac"].as<String>();
        stringToInt(receiverMac, receiverAddress);
        stringToInt(senderMac, senderAddress);
      }
    }
    config.close();
  }
  else
  {
    saveJson();
  }
  Serial.printf("%d, %d, %d, %d, %s, %d, %d, %s, %s\n", airValue,waterValue,sensorPin,DEVICE_ID,DEVICE_NAME,espInterval,wifiChannel,receiverMac,senderMac);
  // Set device as a Wi-Fi Station
  enableBluetooth();
  setWifiChannel(wifiChannel);

  Serial.println("Initializing...");
  Serial.println("My MAC address is: " + WiFi.macAddress());
  hostMac = removeFromString(WiFi.macAddress(), (char *)":");
  stringToInt(hostMac, hostAddress);
  stringToInt(receiverMac, receiverAddress);
  Serial.printf("My MAC address is: %s, %u, %u\n", hostMac, hostAddress, receiverAddress);
  Serial.printf("sender: %s, %u, receiver: %s, %u\n", senderMac, senderAddress, receiverMac, receiverAddress);

  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

  //WiFi.mode(WIFI_STA);
  // int32_t channel = 1;
  // WiFi.printDiag(Serial); // Uncomment to verify channel number before
  // esp_wifi_set_promiscuous(true);
  // esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
  // esp_wifi_set_promiscuous(false);
  // WiFi.printDiag(Serial); // Uncomment to verify channel change after
  //WiFi.disconnect(); // we do not want to connect to a WiFi network

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK)
  {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to
  // get the status of Trasnmitted packet
  // esp_now_set_self_role(MY_ROLE);
  esp_now_register_recv_cb(OnDataRecv); // this function will get called once all data is sent

  esp_now_register_send_cb(OnDataSent);

  peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  //peerInfo.ifidx = ESP_IF_WIFI_STA;
  peerInfo.encrypt = false;
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  } else {
    Serial.printf("Adding peer: %u\n", peerInfo.peer_addr);
  }
  //stringToInt(gatewayMac, gatewayMacAddress);
  //addPeer(gatewayMacAddress);
  //Serial.printf("Adding gateway: %u\n", gatewayMacAddress);

}

void loop()
{
  calculate();
  // Set values to send
  struct_message payload = struct_message();
  payload.id = DEVICE_ID;
  payload.name = DEVICE_NAME;
  payload.hostAddress = hostMac;
  payload.senderAddress = hostMac;
  payload.espInterval = espInterval;
  Serial.printf("info: %d, %d, %s, %d, %s, %s, %s\n", wifiChannel, espInterval, moistureLevel, payload.id, payload.name, payload.senderAddress, payload.receiverAddress);
  payload.type = BROADCAST;
  payload.moisture = moistureLevel;
  payload.msgId = generateMessageHash(payload);
  esp_now_send(broadcastAddress, (uint8_t *)&payload, sizeof(payload));
  //esp_now_send(gatewayMacAddress, (uint8_t *)&payload, sizeof(payload));

  pCharacteristic->setValue(moistureLevel.c_str());
  Delay(espInterval);
}
