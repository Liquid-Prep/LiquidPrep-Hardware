#include <WiFi.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include "FS.h"

#define FIRMWARE_VERSION  "0.2.3"
#define MY_ROLE           ESP_NOW_ROLE_CONTROLLER         // set the role of this device: CONTROLLER, SLAVE, COMBO
#define RECEIVER_ROLE     ESP_NOW_ROLE_SLAVE              // set the role of the receiver
#define WIFI_CHANNEL      1

#define VAL(str) #str
#define TOSTRING(str) VAL(str)

enum Task {
  NO_TASK,
  UPDATE_RECEIVER_ADDR,
  UPDATE_WIFI_CHANNEL,
  UPDATE_DEVICE_NAME,
  UPDATE_DEVICE_ID,
  UPDATE_ESP_INTERVAL,
  UPDATE_SENDER_ADDR,
  REGISTER_DEVICE,
  RELATE_MESSAGE,
  RELATE_MESSAGE_UPSTREAM,
  CONNECT_WITH_ME,
  MESSAGE_ONLY,
  PING,
  PING_BACK,
  QUERY,
  QUERY_RESULT,
  CONNECT_WITH_YOU,
  CALIBRATE_AIR,
  CALIBRATE_WATER,
  CALIBRATE_RESULT,
  BROADCAST
};
Task str2enum(const std::string& str) {
  if(str == "UPDATE_RECEIVER_ADDR") return UPDATE_RECEIVER_ADDR;
  else if(str == "UPDATE_SENDER_ADDR") return UPDATE_SENDER_ADDR;
  else if(str == "UPDATE_WIFI_CHANNEL") return UPDATE_WIFI_CHANNEL;
  else if(str == "UPDATE_DEVICE_NAME") return UPDATE_DEVICE_NAME;
  else if(str == "UPDATE_DEVICE_ID") return UPDATE_DEVICE_ID;
  else if(str == "UPDATE_ESP_INTERVAL") return UPDATE_ESP_INTERVAL;
  else if(str == "REGISTER_DEVICE") return REGISTER_DEVICE;
  else if(str == "RELATE_MESSAGE") return RELATE_MESSAGE;
  else if(str == "RELATE_MESSAGE_UPSTREAM") return RELATE_MESSAGE_UPSTREAM;
  else if(str == "CONNECT_WITH_ME") return CONNECT_WITH_ME;
  else if(str == "MESSAGE_ONLY") return MESSAGE_ONLY;
  else if(str == "PING") return PING;
  else return NO_TASK;
}

uint8_t tmpAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t hostAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t gatewayReceiverAddress[] = {0x40,0x91,0x51,0x9F,0x30,0xAC};   // please update this with the MAC address of the receiver
uint8_t receiverAddress[] = {0x78, 0x21, 0x84, 0x8C, 0x89, 0xFC};   // please update this with the MAC address of the receiver
uint8_t senderAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

String gatewayMac = "7821848C89FC";
String gatewayReceiverMac = "40:91:51:9F:30:AC";                // mac address of gateway receiver  
String hostMac = "";                           // mac address of this device
String receiverMac = "7821848C89FC";           // mac address of receiver
String senderMac = "";                         // mac address of sender
String broadcastMac = "FFFFFFFFFFFF";           // broadcast address

esp_now_peer_info_t peerInfo;

  // TODO: will try to use uint8_t over String
  //uint8_t receiverAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  //uint8_t hostAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
typedef struct struct_message {  
  int id;
  String name;
  String moisture = "";
  int espInterval = 6000;
  String receiverAddress = "";
  String senderAddress = "";
  String hostAddress = "";
  char msg[80];
  int task;
  int type;
  uint32_t  msgId;
} struct_message;

// Common utility functions
int32_t getWiFiChannel(const char *ssid) {
  if (int32_t n = WiFi.scanNetworks()) {
    for (uint8_t i=0; i<n; i++) {
      if (!strcmp(ssid, WiFi.SSID(i).c_str())) {
        return WiFi.channel(i);
      }
    }
  }
  return 0;
}

boolean addPeer(uint8_t *macAddress, int32_t channel=0) {
  // Register peer
  memcpy(peerInfo.peer_addr, macAddress, 6);
  peerInfo.channel = channel;
  peerInfo.encrypt = false;
  // Add peer
  esp_err_t ret = esp_now_add_peer(&peerInfo);
  if(ret != ESP_OK) {
    String error = "";
    if(ret == ESP_ERR_ESPNOW_NOT_INIT) error = "Not initialized";
    else if(ret == ESP_ERR_ESPNOW_ARG) error = "Invalid argument";
    else if(ret == ESP_ERR_ESPNOW_NOT_FOUND) error = "Peer is not found";
    Serial.println("Failed to add peer, error: " + error);
    return false;
  } else {
    return true;
  }
}
boolean deletePeer(uint8_t *macAddress) {
  esp_err_t ret = esp_now_del_peer(macAddress);
  if(ret != ESP_OK) {
    Serial.println("Failed to delete peer, error: " + ret);
    return false;
  } else {
    return true;
  }
}
String removeFromString(String mac, char *charsToRemove) {
  std::string str = mac.c_str();
  for(unsigned int i = 0; i < strlen(charsToRemove); ++i ) {
    str.erase( remove(str.begin(), str.end(), charsToRemove[i]), str.end() );
  }
  return str.c_str();  
}
void stringToInt(String mac, uint8_t *output) {
  int str_len = mac.length() + 1;
  char char_array[str_len];
  mac.toCharArray(char_array, str_len);
  char *ptr = char_array;
  uint64_t addr = strtoull(ptr, &ptr, 16);
  for ( uint8_t i = 0; i < 6; i++ ) {
    output[i] = ( addr >> ( ( 5 - i ) * 8 ) ) & 0xFF;
  }  
}
void calibrateAir(int &air, int pin) {
  Serial.println("Leave Moisture sensor out of water for calibration");
  int maxValue = 0;
  for (int i = 0; i < 32; i++) {
    int val = analogRead(pin);
    Serial.println(val);
    if (val > maxValue) {
      maxValue = val;
    }
    delay(500);
  }
  Serial.println(maxValue);
  air = maxValue;
}
void calibrateWater(int &water, int pin) {
  Serial.println("Put Moisture sensor in water for calibration");
  int minValue = 4096;
  for (int i = 0; i < 32; i++){
    int val = analogRead(pin);
    Serial.println(val);
    if (val < minValue){
      minValue = val;
    }
    delay(500);
  }
  Serial.println(minValue);
  water = minValue;
}
void setPayload(struct_message &payload, int id, String name, String host, String sender, String receiver, int task, int type, String msg, int interval) {
  // Note:  Important for upstream message, set payload.senderAddress=hostMac, payload.hostAddress=receiverMac
  payload = struct_message();
  payload.id = id;
  payload.name = name;
  payload.hostAddress = host;
  payload.senderAddress = sender;
  payload.receiverAddress = receiver;
  payload.task = task;
  payload.type = type;
  payload.espInterval = interval;
  sprintf(payload.msg, "%s", msg.c_str());
}
void espNowSend(String receiver, struct_message payload) {
  stringToInt(receiverMac, tmpAddress);
  esp_now_send(tmpAddress, (uint8_t *) &payload, sizeof(payload));
}
uint32_t generateMessageHash(const struct_message &msg) {
  String combined = String(msg.senderAddress) + String(msg.type) + String(millis());
  uint32_t hash = 0;
  for (unsigned int i = 0; i < combined.length(); i++) {
    hash = hash * 31 + combined[i];
  }
  return hash;
}
#define MESSAGE_ID_BUFFER_SIZE 100

uint32_t messageIDBuffer[MESSAGE_ID_BUFFER_SIZE];
uint16_t messageIDBufferIndex = 0;

bool isMessageSeen(uint32_t messageID) {
  // Check if the message ID is already in the buffer
  for (uint16_t i = 0; i < MESSAGE_ID_BUFFER_SIZE; i++) {
    if (messageIDBuffer[i] == messageID) {
      return true; // The message has been seen before
    }
  }
  // The message is new, add it to the buffer
  messageIDBuffer[messageIDBufferIndex] = messageID;
  messageIDBufferIndex = (messageIDBufferIndex + 1) % MESSAGE_ID_BUFFER_SIZE;
  return false; // The message has not been seen before
}