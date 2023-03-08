#include <WebServer.h>
#include "SPIFFS.h"
#include <common.h>
#include <esp_wifi.h>

int DEVICE_ID = 1;                   // set device id, need to store in SPIFFS
String DEVICE_NAME = "ZONE_1";       // set device name

//uint8_t output[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//uint8_t hostAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//uint8_t senderAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//uint8_t receiverAddress[] = {0x78, 0x21, 0x84, 0x8C, 0x89, 0xFC};   // please update this with the MAC address of the receiver

//struct_message myData;

struct_message payload;

String hostMac = "";
String receiverMac = "";
String moistureLevel = "";
int airValue = 3440;   // 3442;  // enter your max air value here
int waterValue = 1803; // 1779;  // enter your water value here
int SensorPin = 32;
int soilMoistureValue = 0;
float soilmoisturepercent = 0;
const char* fwVersion = FIRMWARE_VERSION;
DynamicJsonDocument doc(1024);
int espInterval=6000; //interval for reading data

//boolean addPeer(uint8_t *macAddress) {
//  // Register peer
//  memcpy(peerInfo.peer_addr, macAddress, 6);
//  peerInfo.channel = 0;
//  peerInfo.encrypt = false;
//  // Add peer
//  esp_err_t ret = esp_now_add_peer(&peerInfo);
//  if(ret != ESP_OK) {
//    Serial.println("Failed to add peer, error: " + ret);
//    return false;
//  } else {
//    return true;
//  }
//}
//boolean deletePeer(uint8_t *macAddress) {
//  esp_err_t ret = esp_now_del_peer(macAddress);
//  if(ret != ESP_OK) {
//    Serial.println("Failed to delete peer, error: " + ret);
//    return false;
//  } else {
//    return true;
//  }
//}

void calculate() {
  int val = analogRead(SensorPin);  // connect sensor to Analog pin

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

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("\r\nLast Packet Send Status: %u\t", receiverAddress);
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  memcpy(&payload, incomingData, sizeof(payload));
  Serial.print("Incoming: ");
  Serial.println(len);
  Serial.printf("%u\n", mac);
  Serial.println(payload.task);
  Serial.println(payload.espInterval);
  Serial.printf("%s, %s, %u\n", payload.hostAddress, hostMac, hostAddress);
  Serial.println("------");
  //mac2int(payload.hostAddress, tmpAddress);
  if(payload.hostAddress == hostMac) {
    Serial.println("updating...");
    switch(payload.task) {
      case UPDATE_RECEIVER_ADDR:
        if(payload.receiverAddress != receiverMac) {
          deletePeer(receiverAddress);
          receiverMac = payload.receiverAddress;
          stringToInt(receiverMac, receiverAddress);
          //std::copy(std::begin(tmpAddress), std::end(tmpAddress), std::begin(receiverAddress));
          addPeer(receiverAddress);
        }
      break;
      case RELATE_MESSAGE:
        esp_now_send(receiverAddress, (uint8_t *) &payload, sizeof(payload));
      break;
      case UPDATE_WIFI_CHANNEL:
      break;
      case UPDATE_DEVICE_NAME:
      break;
      case UPDATE_DEVICE_ID:
      break;
      case UPDATE_ESP_INTERVAL:
        Serial.println("update esp_interval");
        espInterval = payload.espInterval;
      break;
      default:
        Serial.println("Nothing to do.");
      break;
    }
  } else {
    esp_now_send(tmpAddress, (uint8_t *) &payload, sizeof(payload));
  }
}

String saveJson() {
  String msg = "";
  File configFile = SPIFFS.open("/config.json", "w+"); 
  if(configFile) {
    serializeJson(doc, configFile);
    msg = "Updated config successfully!";
    configFile.close();
  } else {
    msg = "Failed to open config file for writing!";
    Serial.println(msg);
  }
  return msg;
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
  while (!SPIFFS.begin(true) && waitCount++ < 3) {
    delay(1000);
  }
  // Init Serial Monitor
  Serial.begin(115200);
  // Set device as a Wi-Fi Station
  Serial.println("Initializing...");
  Serial.printf("Device Name: %s\nDevice Id: %d\n", myData.name, myData.id);
  Serial.println("My MAC address is: " + WiFi.macAddress());
  hostMac = removeFromString(WiFi.macAddress(), (char *)":");
  stringToInt(hostMac, hostAddress);
  Serial.printf("My MAC address is: %s, %u\n", hostMac, hostAddress);
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

}

void loop() {
  calculate();
  // Set values to send
  myData.id = DEVICE_ID;
  myData.name = DEVICE_NAME;
  Serial.printf("info: %d, %s, %d, %s\n", espInterval, moistureLevel, myData.id, myData.name);
  if(myData.id > 1) {
    // TODO:  need a better way to identify leader vs workers
    myData.task = RELATE_MESSAGE;
  } else {
    myData.task = NO_TASK;
  }
  myData.moisture = moistureLevel;
  esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &myData, sizeof(myData));

  delay(espInterval);
}