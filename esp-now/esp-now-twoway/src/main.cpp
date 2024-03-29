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
int espInterval=30000; //interval for reading data

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
void resetPayload(struct_message payload) {
  payload = struct_message();
  payload.id = DEVICE_ID;
  payload.name = DEVICE_NAME;
  payload.hostAddress = hostMac;
}
void resetPayloadTask(struct_message payload, int task, String msg="", String moisture="") {
  resetPayload(payload);
  payload.task = task;
  sprintf(payload.msg, "%s", msg.c_str());          
}
void updateDeviceReceiver(struct_message payload) {
  deletePeer(receiverAddress);
  receiverMac = payload.receiverAddress;
  stringToInt(receiverMac, receiverAddress);
  addPeer(receiverAddress);
  Serial.printf("registering...%s, %d", payload.name, payload.id);
  DEVICE_ID = payload.id;
  DEVICE_NAME = payload.name;
  saveJson();
  // reset payload
  resetPayloadTask(payload, CONNECT_WITH_ME);
  esp_now_send(receiverAddress, (uint8_t *) &payload, sizeof(payload));
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
  Serial.printf("%d from %s, %d, %d, %d, %s\n", len, payload.name, payload.task, payload.espInterval, payload.moisture, payload.msg);
  Serial.printf("=> %s, %s, %s\n", payload.hostAddress, hostMac, payload.senderAddress);
  Serial.println("------");
  if(payload.hostAddress == hostMac) {
    Serial.println("processing...");
    switch(payload.task) {
      case REGISTER_DEVICE:
        if(payload.receiverAddress != receiverMac) {
          deletePeer(receiverAddress);
          receiverMac = payload.receiverAddress;
          stringToInt(receiverMac, receiverAddress);
          addPeer(receiverAddress);
          Serial.printf("registering...%s, %d\n", payload.name, payload.id);
          DEVICE_ID = payload.id;
          DEVICE_NAME = payload.name;
          saveJson();
          // reset payload
          resetPayloadTask(payload, CONNECT_WITH_ME);
          esp_now_send(receiverAddress, (uint8_t *) &payload, sizeof(payload));
        }
      break;
      case UPDATE_RECEIVER_ADDR:
        if(payload.receiverAddress != receiverMac) {
          deletePeer(receiverAddress);
          receiverMac = payload.receiverAddress;
          stringToInt(receiverMac, receiverAddress);
          saveJson();
          //std::copy(std::begin(tmpAddress), std::end(tmpAddress), std::begin(receiverAddress));
          addPeer(receiverAddress);
          Serial.printf("connect with me...%s, %d", payload.name, payload.id);
          // reset payload
          //payload = struct_message();
          //payload.id = DEVICE_ID;
          //payload.name = DEVICE_NAME;
          //payload.hostAddress = hostMac;
          //payload.task = CONNECT_WITH_ME;          
          //esp_now_send(receiverAddress, (uint8_t *) &payload, sizeof(payload));
          resetPayloadTask(payload, CONNECT_WITH_ME);
          esp_now_send(receiverAddress, (uint8_t *) &payload, sizeof(payload));
        }
      break;
      case CONNECT_WITH_ME:
        Serial.printf("Connecting %s with sender %s\n", DEVICE_NAME, payload.senderAddress);
        stringToInt(senderMac, senderAddress);
        deletePeer(senderAddress);
        senderMac = payload.senderAddress;
        stringToInt(senderMac, senderAddress);
        addPeer(senderAddress);
        saveJson();
        payload = struct_message();
        payload.task = CONNECT_WITH_YOU;
        payload.hostAddress = senderMac;
        payload.receiverAddress = hostMac;
        esp_now_send(senderAddress, (uint8_t *) &payload, sizeof(payload));
      break;
      case CONNECT_WITH_YOU:
        Serial.printf("Connecting %s with receiver %s\n", DEVICE_NAME, payload.receiverAddress);
        stringToInt(receiverMac, receiverAddress);
        deletePeer(receiverAddress);
        receiverMac = payload.receiverAddress;
        stringToInt(receiverMac, receiverAddress);
        addPeer(receiverAddress);
        saveJson();
        payload = struct_message();
        payload.task = MESSAGE_ONLY;
        payload.hostAddress = receiverMac;
        sprintf(payload.msg, "We are connected!");
        //resetPayloadTask(payload, MESSAGE_ONLY, "We are connected!");
        esp_now_send(receiverAddress, (uint8_t *) &payload, sizeof(payload));
      break;
      case PING:
        payload = struct_message(); 
        payload.type = PING;
        payload.task = RELATE_MESSAGE_UPSTREAM;
        payload.senderAddress = hostMac;
        payload.hostAddress = receiverMac;
        payload.name = DEVICE_NAME;
        payload.id = DEVICE_ID;
        stringToInt(receiverMac, tmpAddress);
        esp_now_send(tmpAddress, (uint8_t *) &payload, sizeof(payload));
      break;
      case QUERY:
        payload = struct_message(); 
        payload.type = QUERY_RESULT;
        payload.task = RELATE_MESSAGE_UPSTREAM;
        payload.senderAddress = hostMac;
        payload.hostAddress = receiverMac;
        payload.name = DEVICE_NAME;
        payload.id = DEVICE_ID;
        payload.espInterval = espInterval;
        sprintf(payload.msg, "%d,%d,%d,%s,%s", airValue, waterValue, sensorPin, senderMac.c_str(), receiverMac.c_str());
        Serial.println(payload.msg);
        espNowSend(receiverMac, payload);
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
        payload.type = CALIBRATE_RESULT;
        payload.task = RELATE_MESSAGE_UPSTREAM;
        payload.senderAddress = hostMac;
        payload.hostAddress = receiverMac;
        payload.name = DEVICE_NAME;
        payload.id = DEVICE_ID;
//void setPayload(struct_message &payload, int id, String name, String host, String sender, String receiver, int task, int type, String msg) {
        // Note:  Important for upstream message, set payload.senderAddress=hostMac, payload.hostAddress=receiverMac
        //setPayload(payload, DEVICE_ID, DEVICE_NAME, receiverMac, hostMac, "", RELATE_MESSAGE_UPSTREAM, CALIBRATE_RESULT, "");
        sprintf(payload.msg, "%d,%d,%d,%s,%s", airValue, waterValue, sensorPin, senderMac.c_str(), receiverMac.c_str());
        espNowSend(receiverMac, payload);
      break;
      case RELATE_MESSAGE:
        Serial.printf("Relate %s message\n", payload.name);
        payload.hostAddress = receiverMac;
        stringToInt(receiverMac, tmpAddress);
        Serial.printf("%d from %s, %d, %d, %d, %s\n", len, payload.name, payload.task, payload.espInterval, payload.hostAddress, receiverMac);
        esp_now_send(tmpAddress, (uint8_t *) &payload, sizeof(payload));
      break;
      case MESSAGE_ONLY:
        Serial.printf("fyi: %s, %s\n", payload.msg, payload.senderAddress);
      break;
      case RELATE_MESSAGE_UPSTREAM:
        Serial.printf("send upstream to %s from %s\n", receiverMac, payload.name);
        stringToInt(receiverMac, tmpAddress);
        esp_now_send(tmpAddress, (uint8_t *) &payload, sizeof(payload));
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
    if(payload.task == RELATE_MESSAGE_UPSTREAM) {
      Serial.printf("send ustream to %s from %s\n", receiverMac, payload.name);
      stringToInt(receiverMac, tmpAddress);
      esp_now_send(tmpAddress, (uint8_t *) &payload, sizeof(payload));

    } else {
      Serial.printf("send downstream to %s from %s\n", senderMac, payload.name);
      stringToInt(senderMac, tmpAddress);
      esp_now_send(tmpAddress, (uint8_t *) &payload, sizeof(payload));
    }
  }
}

//void dataReceived(uint8_t *senderMac, uint8_t *data, uint8_t dataLength) {
//char macStr[18];
//dataPacket packet;

//snprintf(macStr, sizeof(macStr), “%02x:%02x:%02x:%02x:%02x:%02x”, senderMac[0], senderMac[1], senderMac[2], senderMac[3], senderMac[4], senderMac[5]);
//memcpy(&myData, data, sizeof(myData)); // copy redeived data into myData and printit
//Serial.println();
//Serial.print(“Received data from: “);
//Serial.println(macStr);
//Serial.print(“data: “);
//Serial.println(myData.d);

//memcpy(&packet, data, sizeof(packet));

//// Serial.print(“sensor1: “);

//}

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
  if(payload.id > 1) {
    // TODO:  need a better way to identify leader vs workers
    payload.task = RELATE_MESSAGE;
  } else {
    payload.task = NO_TASK;
  }
  payload.moisture = moistureLevel;
  esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &payload, sizeof(payload));

  delay(espInterval);
}