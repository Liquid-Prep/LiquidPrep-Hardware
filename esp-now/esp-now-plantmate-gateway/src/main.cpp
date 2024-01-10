#include <WebServer.h>
#include "SPIFFS.h"
#include <AutoConnect.h>
#include <WebSocketsClient.h>
#include <common.h>

int DEVICE_ID = 0;                    // set device id, 0 = ESP32 Gateway that will relate all messages to edge gateway via websocket
String DEVICE_NAME = "GATEWAY";       // set device name

WebServer Server;
AutoConnect Portal(Server);
AutoConnectConfig Config;
String moistureLevel = "";
int soilMoistureValue = 0;
float soilmoisturepercent=0;
const char* fwVersion = FIRMWARE_VERSION;
DynamicJsonDocument doc(1024);

WebSocketsClient webSocketClient;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;

int sensorPin = A0; // Assuming A0 is where your sensor is connected
int Value_dry; // This will hold the maximum value obtained during dry calibration
int Value_wet; // This will hold the minimum value obtained during wet calibration


// TODO: allow input certain values in webtools and writ to SPIFFS at the time of flashing
int espInterval=90000; //espInterval for reading data
int capacitance;
String wsserver = "192.168.86.24";  //ip address of Express server
int wsport= 3000;
char path[] = "/";   //identifier of this device
boolean webSocketConnected=0;
String data= "";
uint8_t leaderMacAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
String leaderMac = "7821848D8840";

void calculate()
{
  int val = analogRead(sensorPin); // connect sensor to Analog pin


  soilmoisturepercent = map(val, Value_dry, Value_wet, 0, 100);
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

void sendData(String data) {
  if (data.length() > 0) {
    Serial.println("Sending: " + data);
    webSocketClient.sendTXT(data);//send sensor data to websocket server
  } else {
  }    
}

String saveJson() {
  String msg = "";
  File configFile = SPIFFS.open("/config.json", "w+"); 
  if(configFile) {
  Serial.printf("%d, %d, %d, %d, %s, %d, %s, %s\n", Value_dry,Value_wet,sensorPin,DEVICE_ID,DEVICE_NAME,espInterval,receiverMac,senderMac);
    doc["deviceId"] = DEVICE_ID;
    doc["deviceName"] = DEVICE_NAME;
    doc["airValue"] = Value_dry;
    doc["waterValue"] = Value_wet;
    doc["sensorPin"] = sensorPin;
    doc["espInterval"] = espInterval;
    doc["receiverMac"] = gatewayReceiverMac;
    doc["senderMac"] = senderMac;
    doc["wsserver"] = wsserver;
    doc["wsport"] = wsport;

    serializeJson(doc, configFile);
    msg = "Updated config successfully!";
    configFile.close();
  } else {
    msg = "Failed to open config file for writing!";
    Serial.println(msg);
  }
  Serial.println(msg);
  return msg;
}
// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.printf("Last Packet Send Status: %u, %u, %s\t", leaderMacAddress, mac_addr, receiverMac);
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  Serial.println("");
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  struct_message payload = struct_message();
  memcpy(&payload, incomingData, sizeof(payload));
  Serial.printf("Bytes received at %s: ------\n", DEVICE_NAME);
  Serial.printf("%d, moisture: %s, %d from %s, %d, %d, %d, %s\n", len, payload.moisture, payload.capacitance, payload.name, payload.task, payload.type, payload.from, payload.msg);
  Serial.printf("=> msgId: %d\n", payload.msgId);
  Serial.println("------\n");
  String response = "";
  if(isMessageSeen(payload.msgId)) {
    Serial.printf("%d from %s, %d Message already seen, ignoring...\n", len, payload.name, payload.task);
  } else {
    String from = "";
    if(payload.from == WEB_REQUEST_RESULT) {
      from = ", \"from\": " + String(WEB_REQUEST_RESULT);
    }
    if(payload.task == PING_BACK) {
      Serial.printf("Ping: %d, %s, %d, %d\n", payload.msgId, payload.name, payload.type, payload.task);
      Serial.println();
      response = "{\"mac\": \"" + payload.senderAddress + "\", \"id\": " + String(payload.id) + from + ", \"name\": \"" + payload.name + "\", \"type\": " + String(payload.type) + ", \"task\": " + String(payload.task) + "}";
    } else if(payload.task == UPDATE_WIFI_RESULT) {
      Serial.printf("Wifi updated: %d, %s, %d, %d\n", payload.msgId, payload.name, payload.type, payload.task);
      Serial.println();
      response = "{\"mac\": \"" + payload.senderAddress + "\", \"id\": " + String(payload.id) + from + ", \"name\": \"" + payload.name + "\", \"type\": " + String(payload.type) + ", \"task\": " + String(payload.task) + "}";
    }
    else if(payload.task == QUERY_RESULT || payload.task == CALIBRATE_RESULT || payload.task == MOISTURE_RESULT) {
      Serial.printf("Query: %d, %s, %d, %d, %s, %s\n", payload.msgId, payload.name, payload.type, payload.task, from, payload.msg);
      Serial.println();
      response = "{\"mac\": \"" + payload.senderAddress + "\", \"interval\": " + String(payload.espInterval) + ", \"id\": " + String(payload.id) + from + ", \"name\": \"" + payload.name + "\", \"msg\": \"" + payload.msg + "\", \"type\": " + String(payload.type) + ", \"task\": " + String(payload.task) + "}";
    } else {
      Serial.println(payload.name + ", " + payload.id + ", " + payload.moisture + ", " + payload.task);
      Serial.println();
      response = "{\"mac\": \"" + payload.senderAddress + "\", \"id\": " + String(payload.id) + from + ", \"name\": \"" + payload.name + "\", \"moisture\": \"" + payload.moisture + "\"}";
    }
    sendData(response);
  }  
}

String moistureJson() {
  calculate();
  String response = "{\"mac\": \"" + hostMac + "\", \"id\": " + String(DEVICE_ID) + ", \"name\": \"" + DEVICE_NAME + "\", \"moisture\": " + moistureLevel + "}";
  Server.send(200, "text/json", response);
  Serial.printf("sensor reading: %s\n", moistureLevel);
  return response;
}
void httpResponse(boolean success, String msg1, String msg2) {
  if(success) {
    Server.send(200, "text/plain", msg1);
  } else {
    Server.send(400, "text/plain", msg2);
  }  
}
void restartESP() {
  ESP.restart();
}
void calibrate() {
  boolean success = false;
  if(Server.args() == 2 && Server.argName(0) == "value" && Server.argName(1) == "host_addr" && (Server.arg(0) == "air_value" || Server.arg(0) == "water_value")) {
    success = true;
    String targetHostAddr = removeFromString(Server.arg(1), (char *)":");
    struct_message payload = struct_message();
    int from = Server.arg(2) == "true" && Server.argName(2) == "web_request" ? WEB_REQUEST : NO_TASK;
    if(targetHostAddr == hostMac) {
      if(Server.arg(0) == "air_value") {
        calibrateAir(Value_dry, sensorPin);
      } else {
        calibrateWater(Value_wet, sensorPin);
      }
      saveJson();
      sprintf(payload.msg, "%d,%d,%d,%s,%s", Value_dry, Value_wet, sensorPin, senderMac.c_str(), receiverMac.c_str());
      String response = "{\"mac\": \"" + hostMac + "\", \"interval\": " + String(espInterval) + ", \"id\": " + String(DEVICE_ID) + ", \"from\": " + String(WEB_REQUEST_RESULT) + ", \"name\": \"" + DEVICE_NAME + "\", \"msg\": \"" + payload.msg + "\", \"task\": " + String(CALIBRATE_RESULT) + "}";
      sendData(response);
    } else {
      int task = Server.arg(0) == "air_value" ? CALIBRATE_AIR : CALIBRATE_WATER;
      setPayload(payload, DEVICE_ID, DEVICE_NAME, targetHostAddr, senderMac, receiverMac, task, BROADCAST, "", espInterval, from, capacitance);
      payload.msgId = generateMessageHash(payload);
      esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
      esp_now_send(leaderMacAddress, (uint8_t *) &payload, sizeof(payload));
    }
  }
  httpResponse(success, "Calibrate",  "Invalid request params, correct params: value=air_value&host_addr=... OR value=water_value&host_addr=...");
}
void getMoisture() {
  boolean success = false;
  if(Server.args() == 1 && Server.argName(0) == "host_addr" || Server.args() == 2 && Server.argName(0) == "host_addr" && Server.argName(1) == "web_request") {
    success = true;
    String targetHostAddr = removeFromString(Server.arg(0), (char *)":");
    struct_message payload = struct_message();
    int from = Server.arg(1) == "true" && Server.argName(1) == "web_request" ? WEB_REQUEST : NO_TASK;
    Serial.printf("from: %d, %s, %s\n", payload.from, Server.argName(1), Server.arg(1));
    if(targetHostAddr == hostMac) {
      String fromStr = "";
      if(from == WEB_REQUEST) {
        fromStr = ", \"from\": " + String(WEB_REQUEST_RESULT);
      }
      Serial.printf("why why why, %s\n", fromStr);
      calculate();
      sprintf(payload.msg, "%d,%d,%d,%d,%s,%s,%s", Value_dry, Value_wet, sensorPin, WiFi.channel(), hostMac.c_str(), "", moistureLevel);
      String response = "{\"mac\": \"" + hostMac + "\", \"interval\": " + String(espInterval) + ", \"id\": " + String(DEVICE_ID) + fromStr + ", \"name\": \"" + DEVICE_NAME + "\", \"msg\": \"" + payload.msg + "\", \"task\": " + String(MOISTURE_RESULT) + "}";
      sendData(response);
    } else {
      Serial.printf("from: %d\n", from);
      setPayload(payload, DEVICE_ID, DEVICE_NAME, targetHostAddr, hostMac, "", GET_MOISTURE, BROADCAST, "", espInterval, from, capacitance);
      payload.msgId = generateMessageHash(payload);
      Serial.printf("why why why, %d, %d, %d, %d\n", payload.from, payload.task, payload.type, payload.msgId);
      esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
    }
  }    
  httpResponse(success, "Query ESP",  "Invalid request params, correct params: host_addr=...");
}
void queryESP() {
  boolean success = false;
  if(Server.args() == 1 && Server.argName(0) == "host_addr" || Server.args() == 2 && Server.argName(0) == "host_addr" && Server.argName(1) == "web_request") {
    success = true;
    String targetHostAddr = removeFromString(Server.arg(0), (char *)":");
    struct_message payload = struct_message();
    int from = Server.arg(1) == "true" && Server.argName(1) == "web_request" ? WEB_REQUEST : NO_TASK;
    Serial.printf("from: %d, %s, %s\n", payload.from, Server.argName(1), Server.arg(1));
    if(targetHostAddr == hostMac) {
      String fromStr = "";
      if(from == WEB_REQUEST) {
        fromStr = ", \"from\": " + String(WEB_REQUEST_RESULT);
      }
      Serial.printf("why why why, %s\n", fromStr);
      sprintf(payload.msg, "%d,%d,%d,%d,%s,%s", Value_dry, Value_wet, sensorPin, WiFi.channel(), senderMac.c_str(), receiverMac.c_str());
      String response = "{\"mac\": \"" + hostMac + "\", \"interval\": " + String(espInterval) + ", \"id\": " + String(DEVICE_ID) + fromStr + ", \"name\": \"" + DEVICE_NAME + "\", \"msg\": \"" + payload.msg + "\", \"task\": " + String(QUERY_RESULT) + "}";
      sendData(response);
    } else {
      Serial.printf("from: %d\n", from);
      setPayload(payload, DEVICE_ID, DEVICE_NAME, targetHostAddr, hostMac, "", QUERY, BROADCAST, "", espInterval, from, capacitance);
      payload.msgId = generateMessageHash(payload);
      Serial.printf("why why why, %d, %d, %d, %d\n", payload.from, payload.task, payload.type, payload.msgId);
      esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
    }  
  }
  httpResponse(success, "Query ESP",  "Invalid request params, correct params: host_addr=...");
}
void pingESP() {
  boolean success = false;  
  if(Server.args() == 1 && Server.argName(0) == "host_addr" || Server.args() == 2 && Server.argName(0) == "host_addr" && Server.argName(1) == "web_request") {
    String targetHostAddr = removeFromString(Server.arg(0), (char *)":");
    success = true;
    struct_message payload = struct_message();
    int from = Server.arg(1) == "true" && Server.argName(1) == "web_request" ? WEB_REQUEST : NO_TASK;
    if(targetHostAddr == hostMac) {
      String fromStr = "";
      if(from == WEB_REQUEST) {
        fromStr = ", \"from\": " + String(WEB_REQUEST_RESULT);
      }
      sprintf(payload.msg, "%d,%d,%d,%s,%s", Value_dry, Value_wet, sensorPin, senderMac.c_str(), receiverMac.c_str());
      String response = "{\"mac\": \"" + hostMac + "\", \"id\": " + String(DEVICE_ID) + fromStr + ", \"name\": \"" + DEVICE_NAME + "\", \"msg\": \"" + payload.msg + "\", \"task\": " + String(PING_BACK) + "}";
      setPayload(payload, DEVICE_ID, DEVICE_NAME, "", hostMac, "", PING_BACK, BROADCAST, DEVICE_NAME, espInterval, WEB_REQUEST_RESULT, capacitance);
      sendData(response);
    } else {
      Serial.printf("from: %d\n", from);
      setPayload(payload, DEVICE_ID, DEVICE_NAME, targetHostAddr, hostMac, "", PING, BROADCAST, DEVICE_NAME, espInterval, from, capacitance);
      payload.msgId = generateMessageHash(payload);
      Serial.printf("%d, %s, %s, %s, %d, %s\n", payload.id,payload.name,payload.hostAddress,payload.senderAddress,payload.task,payload.msg);
      esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
    }
  }
  if(success) {
    Server.send(200, "text/plain", "Ping!");
  } else {
    Server.send(400, "text/plain", "Invalid request params, correct params: host_addr=...");
  }  
}
void updateESP32() {
  boolean success = false;
  if(Server.args() == 2) {
    String hostAddr = removeFromString(Server.arg(0), (char *)":");
    String taskValue = removeFromString(Server.arg(1), (char *)":");

    Serial.printf("%s, %s, %u, %u\n", hostAddr, hostMac);
    if(Server.argName(0) == "host_addr" && hostAddr == hostMac) {
      String task = Server.argName(1);
      if(task == "esp_interval") {
        int ms = atoi(taskValue.c_str());
        Serial.printf("update esp_interval: %d\n", ms);
        espInterval = ms;
        saveJson();
      } else if(task == "device_name") {
        Serial.printf("update device name: %s\n", taskValue);
        DEVICE_NAME = taskValue;
        saveJson();
      } else if(task == "device_id") {
        int id = atoi(taskValue.c_str());
        Serial.printf("update device id: %d\n", id);
        DEVICE_ID = id;
        saveJson();
      }
    } else {
      success = true;
      String task = Server.argName(1);
      struct_message payload = struct_message();
      payload.hostAddress = hostAddr;
      if(task == "recv_addr") {
        payload.task = UPDATE_RECEIVER_ADDR;
        payload.receiverAddress = taskValue;
      } else if(task == "esp_interval") {
        Serial.println(hostAddr);
        Serial.printf("%s\n", payload.hostAddress);
        payload.task = UPDATE_ESP_INTERVAL;
        payload.espInterval = atoi(taskValue.c_str());
      } else if(task == "device_name") {
        payload.task = UPDATE_DEVICE_NAME;
        payload.name = taskValue;
      } else if(task == "device_id") {
        payload.task = UPDATE_DEVICE_ID;
        payload.id = atoi(taskValue.c_str());
      } else if(task == "enable_bluetooth") {
        payload.task = ENABLE_BLUETOOTH;
      } else if(task == "disable_bluetooth") {
        payload.task = DISABLE_BLUETOOTH;
      } else if(task == "update_pin") {
        payload.task = UPDATE_PIN;
        payload.espInterval = atoi(taskValue.c_str());
      }
      payload.msgId = generateMessageHash(payload);
      Serial.printf("Broacast to: %s, %u\n", payload.hostAddress, gatewayReceiverAddress);
      esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));      
      esp_now_send(leaderMacAddress, (uint8_t *) &payload, sizeof(payload));
    }
  }
  if(success) {
    Server.send(200, "text/plain", "Good to go!");
  } else {
    Server.send(400, "text/plain", "Invalid request params");
  }  
}

void updatePin() {

}

String onHome(AutoConnectAux& aux, PageArgument& args) {
  calculate();
  Serial.println(moistureLevel);
  aux["results"].as<AutoConnectText>().value = moistureLevel;
  return String();
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  try {
    switch(type) {
      case WStype_DISCONNECTED:
        Serial.printf("Websocket Disconnected!\n");
        break;
      case WStype_CONNECTED:
          Serial.printf("Websocket Connected\n");
        break;
      case WStype_TEXT:
        Serial.printf("get Text: %s\n", payload);
        break;
      }
  } catch(...) {
    Serial.println("Catch web socket errors");
  }
  webSocketConnected = 1;
}

void wsconnect() {
  if(!webSocketConnected) {
    // Connect to the websocket server
    Serial.printf("%s, %d, %s\n", wsserver, wsport, path);
    //webSocketClient.begin(wsserver, wsport, path);
    webSocketClient.beginSSL(wsserver.c_str(), wsport, path);
    // WebSocket event handler
    webSocketClient.onEvent(webSocketEvent);
    // if connection failed retry every 5s
    webSocketClient.setReconnectInterval(5000);
  }
}

String onSaveConfig(AutoConnectAux& aux, PageArgument& args) {
  Value_dry = doc["airValue"] = args.arg("airValue").toInt();
  Value_wet = doc["waterValue"] = args.arg("waterValue").toInt();
  sensorPin = doc["sensorPin"] = args.arg("sensorPin").toInt();
  espInterval = doc["espInterval"] = args.arg("espInterval").toInt();
  doc["wsserver"] = args.arg("wsserver");
  JsonObject obj = doc.as<JsonObject>();
  wsserver = obj["wsserver"].as<String>();
  wsport = doc["wsport"] = args.arg("wsport").toInt();
  gatewayReceiverMac = args.arg("receiverAddress");
  senderMac = args.arg("senderAddress");

  String msg = saveJson();
  aux["results"].as<AutoConnectText>().value = msg;
  ESP.restart();
  return String();
}

String onUpdateConfig(AutoConnectAux &aux, PageArgument &args) {
  int value = doc["waterValue"];
  Serial.println(value);
  aux["waterValue"].as<AutoConnectInput>().value = value;
  value = doc["airValue"];
  Serial.println(value);
  aux["airValue"].as<AutoConnectInput>().value = value;
  value = doc["sensorPin"];
  Serial.println(value);
  aux["sensorPin"].as<AutoConnectInput>().value = value;
  String strValue = doc["wsserver"];
  Serial.println(strValue);
  aux["wsserver"].as<AutoConnectInput>().value = strValue;
  value = doc["wsport"];
  Serial.println(value);
  aux["wsport"].as<AutoConnectInput>().value = value;

  String strValue2 = doc["receiverMac"];
  Serial.println(strValue2);
  aux["receiverMac"].as<AutoConnectInput>().value = strValue2;
  String strValue3 = doc["senderMac"];
  Serial.println(strValue3);
  aux["senderMac"].as<AutoConnectInput>().value = strValue3;
  
  value = doc["espInterval"];
  Serial.println(value);
  aux["espInterval"].as<AutoConnectInput>().value = value;
  return String();
}

void updateWifiChannel() {
  struct_message payload = struct_message();
  payload.id = DEVICE_ID;
  payload.name = DEVICE_NAME;
  //payload.hostAddress = receiverMac;
  payload.hostAddress = "";
  payload.senderAddress = hostMac;
  payload.espInterval = espInterval;
  payload.type = BROADCAST;
  payload.from = NO_TASK;
  payload.task = UPDATE_WIFI_CHANNEL;
  sprintf(payload.msg, "%d", WiFi.channel());
  Serial.printf("%d: %s", Server.args(), Server.argName(0));
  if(Server.args() == 1 && Server.argName(0) == "web_request") {
    payload.from = WEB_REQUEST;  
  }
  Serial.printf("\nwifi: %d, %d, %s, %d, %d, %s, %s\n", espInterval, payload.from, payload.msg, payload.task, payload.type, payload.name, payload.senderAddress);
  payload.msgId = generateMessageHash(payload);
  esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
  esp_now_send(leaderMacAddress, (uint8_t *) &payload, sizeof(payload));
  Server.send(200, "text/plain", "Update wifi channel!");
}

void registerAndAdd() {
  // Register peer
  peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  } else {
    Serial.printf("Adding peer: %u\n", peerInfo.peer_addr);
  }
}
void setup() {
  int waitCount = 0;
  delay(1000);
  Serial.begin(115200);
  Serial.println();

  while (!SPIFFS.begin(true) && waitCount++ < 3) {
    delay(1000);
  }

  File page = SPIFFS.open("/page.json", "r");
  if(page) {
    Portal.load(page);
    page.close();
  }
  File config = SPIFFS.open("/config.json", "r");
  if(config) {
    DeserializationError error = deserializeJson(doc, config);
    if(error) {
      Serial.println(F("Failed to read file, using default configuration"));
      Serial.println(error.c_str());
      saveJson();
    } else {
      JsonObject obj = doc.as<JsonObject>();
      Value_dry = doc["airValue"];
      Value_wet = doc["waterValue"];
      wsserver = obj["wsserver"].as<String>();
      wsport = doc["wsport"];
      sensorPin = doc["sensorPin"];
      DEVICE_ID = obj["deviceId"];
      DEVICE_NAME = doc["deviceName"] ? doc["deviceName"].as<String>() : DEVICE_NAME;
      espInterval = doc["espInterval"] ? doc["espInterval"] : espInterval;
      gatewayReceiverMac = doc["receiverMac"] ? doc["receiverMac"].as<String>() : gatewayReceiverMac;
      senderMac = doc["senderMac"] ? doc["senderMac"].as<String>() : senderMac;
      stringToInt(gatewayReceiverMac, gatewayReceiverAddress);
      stringToInt(senderMac, senderAddress);
    }
    config.close();
  } else {
    // save default config
    saveJson();
  }
  Serial.printf("%d, %d, %d, %d, %s, %d, %s, %s\n", Value_dry,Value_wet,sensorPin,DEVICE_ID,DEVICE_NAME,espInterval,receiverMac,senderMac);

  Config.autoReconnect = true;
  Config.hostName = "liquid-prep";
  Config.ota = AC_OTA_BUILTIN;
  Config.otaExtraCaption = fwVersion;
  Portal.config(Config);
  Portal.on("/update_config", onUpdateConfig);
  Portal.on("/save_config", onSaveConfig);
  Portal.on("/", onHome);

  Server.enableCORS();
  Server.on("/moisture", getMoisture);
  Server.on("/moisture.json", moistureJson);
  Server.on("/update", updateESP32);
  Server.on("/reboot_gateway", restartESP);
  Server.on("/ping", pingESP);
  Server.on("/query", queryESP);
  Server.on("/calibrate", calibrate);
  Server.on("/update_wifi", updateWifiChannel);
  Serial.println("Connecting");
  if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    Serial.println("My MAC address is: " + WiFi.macAddress());
    Serial.print("Wi-Fi Channel: ");
    Serial.println(WiFi.channel());
    Serial.print("Wi-Fi SSID: ");
    Serial.println(WiFi.SSID());
    hostMac = removeFromString(WiFi.macAddress(), (char *)":");
    Serial.printf("host: %s, receiver: %s, gateway receiver: %s\n", hostMac, receiverMac, gatewayReceiverMac);
    stringToInt(hostMac, hostAddress);
    stringToInt(gatewayReceiverMac, gatewayReceiverAddress);
    Serial.printf("host: %u, gatway receiver: %u\n", hostAddress, gatewayReceiverAddress);
  }
  waitCount = 0;
  while (WiFi.status() != WL_CONNECTED && waitCount++ < 3) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Failed to connect to WiFi");
  } else {
    wsconnect();
  }
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
      Serial.println("Error initializing ESP-NOW");
      return;
  }
  esp_now_register_recv_cb(OnDataRecv);
  esp_now_register_send_cb(OnDataSent);

  //registerAndAdd();
  //updateWifiChannel();

  // Register peer
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  //peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  // Add peer        
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  } else {
    Serial.printf("Adding peer: %u\n", peerInfo.peer_addr);
  }
  //stringToInt(leaderMac, leaderMacAddress);
  //addPeer(leaderMacAddress);  
  //Serial.printf("Adding leader: %u\n", leaderMacAddress);
  //addPeer(broadcastAddress);  
}

void loop() {
  Portal.handleClient();
  webSocketClient.loop();
  if (WiFi.status() == WL_CONNECTED) {
    currentMillis=millis(); 
    if (currentMillis - previousMillis >= espInterval) {
      previousMillis = currentMillis;
      sendData(moistureJson());
    }
  }
}