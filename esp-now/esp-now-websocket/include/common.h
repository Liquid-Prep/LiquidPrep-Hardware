#include <WiFi.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include "FS.h"

enum Task {
  NO_TASK,
  UPDATE_RECEIVER_ADDR,
  UPDATE_WIFI_CHANNEL,
  UPDATE_DEVICE_NAME,
  UPDATE_DEVICE_ID,
  UPDATE_ESP_INTERVAL
};
Task str2enum(const std::string& str) {
  if(str == "UPDATE_RECEIVER_ADDR") return UPDATE_RECEIVER_ADDR;
  else if(str == "UPDATE_WIFI_CHANNEL") return UPDATE_WIFI_CHANNEL;
  else if(str == "UPDATE_DEVICE_NAME") return UPDATE_DEVICE_NAME;
  else if(str == "UPDATE_DEVICE_ID") return UPDATE_DEVICE_ID;
  else if(str == "UPDATE_ESP_INTERVAL") return UPDATE_ESP_INTERVAL;
  else return NO_TASK;
}

uint8_t tmpAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t hostAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
uint8_t gatewayReceiverAddress[] = {0x40,0x91,0x51,0x9F,0x30,0xAC};   // please update this with the MAC address of the receiver
uint8_t receiverAddress[] = {0x78, 0x21, 0x84, 0x8C, 0x89, 0xFC};   // please update this with the MAC address of the receiver

esp_now_peer_info_t peerInfo;

typedef struct struct_message {  
  int currPointer;
  int id;
  String name;
  String moisture;
  unsigned long timestamp;
  int espInterval;
  uint8_t receiverAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  uint8_t hostAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  int task;
} struct_message;
struct_message myData;

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

uint8_t* mac2int(String mac, uint8_t *output) {
  std::string blank = "";
  std::string colon = ":";
  std::string s = mac.c_str();
  size_t pos;

  while ((pos = s.find(colon)) != std::string::npos) {
    s = s.replace(pos, 1, blank);
  }
  char dummy[3] = {0};
  int j = 0;
  uint8_t num = 0;
  uint8_t *pout;
  char *ptr;
  for(int k=0;k<6;k++) {
    for(int i=0;i<2;i++) {
      dummy[i] = s[i+j];
    }
    num = strtol(dummy, &ptr, 16);
    j=j+2;
    //printf("0x%2x\n", num);
    output[k] = num;
  }
  Serial.printf("%u\n", output);
  return output;
}
