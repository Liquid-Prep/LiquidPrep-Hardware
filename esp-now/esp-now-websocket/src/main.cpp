#include <WebServer.h>
#include "SPIFFS.h"
#include <AutoConnect.h>
#include <WebSocketsClient.h>
#include <common.h>

// TODO:  need to store device configuration in SPIFFS
int DEVICE_ID = 0;                   // set device id, 0 = ESP32 Gateway that will relate all messages to edge gateway via websocket
String DEVICE_NAME = "GATEWAY";       // set device name

//uint8_t hostAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//uint8_t receiverAddress[] = {0x40,0x91,0x51,0x9F,0x30,0xAC};   // please update this with the MAC address of the receiver
//uint8_t tmpAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

//typedef struct struct_message {  
//  int currPointer;
//  int id;
//  String name;
//  String moisture;
//  unsigned long timestamp;

//} struct_message;
//struct_message myData;
//esp_now_peer_info_t peerInfo;

WebServer Server;
AutoConnect Portal(Server);
AutoConnectConfig Config;
String moistureLevel = "";
int airValue = 3440; //3442;  // enter your max air value here
int waterValue = 1803; //1779;  // enter your water value here
int SensorPin = 32;
int soilMoistureValue = 0;
float soilmoisturepercent=0;
const char* fwVersion = FIRMWARE_VERSION;
DynamicJsonDocument doc(1024);

WebSocketsClient webSocketClient;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;

// TODO: allow input certain values in webtools and writ to SPIFFS at the time of flashing
int interval=60000; //interval for reading data
String wsserver = "192.168.86.24";  //ip address of Express server
int wsport= 3000;
char path[] = "/";   //identifier of this device
boolean webSocketConnected=0;
String data= "";

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
  // TODO:  only push value when there is a device connected
  //pCharacteristic->setValue(str);  // push the value via bluetooth
}

void sendData(String data) {
  if (data.length() > 0) {
    Serial.println("Sending: " + data);
    webSocketClient.sendTXT(data);//send sensor data to websocket server
  } else {
  }    
}

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success\n" : "Delivery Fail\n");
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  struct_message payload;

  memcpy(&payload, incomingData, sizeof(payload));
  Serial.print("Bytes received: ");
  Serial.printf("%d, %u\n", len, mac);
  Serial.println(payload.name + ", " + payload.id + ", " + payload.moisture + ", " + payload.task);
  Serial.println();
  String response = "{\"mac\": \"" + payload.senderAddress + "\", \"id\": " + String(payload.id) + ", \"name\": \"" + payload.name + "\", \"moisture\": " + payload.moisture + "}";
  sendData(response);
}

String moistureJson() {
  calculate();
  String response = "{\"mac\": \"" + hostMac + "\", \"id\": " + String(DEVICE_ID) + ", \"name\": \"" + DEVICE_NAME + "\", \"moisture\": " + moistureLevel + "}";
  Server.send(200, "text/json", response);
  Serial.printf("sensor reading: %s\n", moistureLevel);
  return response;
}

void registerESP32() {
  struct_message payload = struct_message();
  boolean success = false;  
  if(Server.args() == 4 && Server.argName(0) == "host_addr" && Server.argName(1) == "recv_addr" && Server.argName(2) == "device_id" && Server.argName(3) == "device_name") {
    String hostAddr = removeFromString(Server.arg(0), (char *)":");
    String taskValue = removeFromString(Server.arg(1), (char *)":");
    String deviceId = removeFromString(Server.arg(2), (char *)":");
    String deviceName = removeFromString(Server.arg(3), (char *)":");
    if(hostAddr != hostMac) {
      success = true;
      payload.id = atoi(deviceId.c_str());
      payload.name = deviceName;
      payload.task = REGISTER_DEVICE;
      payload.hostAddress = hostAddr;
      payload.receiverAddress = taskValue;
      stringToInt(hostAddr, tmpAddress);
      if(esp_now_is_peer_exist(tmpAddress)) {
        Serial.println("found peer");
      } else {
        Serial.println("peer not found");
        addPeer(tmpAddress);  
      }
      esp_now_send(tmpAddress, (uint8_t *) &payload, sizeof(payload));
    } else {
      Server.send(200, "text/plain", "This is esp gateway, can't register itself!");
    }
  }
  if(success) {
    Server.send(200, "text/plain", "Device registered!");
  } else {
    Server.send(400, "text/plain", "Invalid request params, correct params: host_addr=...&recv_addr=...&device_id=...&device_name=...");
  }  
}
void connectGateways() {
  boolean success = false;
  if(Server.args() == 1 && Server.argName(0) == "sender_addr") {
    String senderAddr = removeFromString(Server.arg(0), (char *)":");
    stringToInt(receiverMac, receiverAddress);
    deletePeer(receiverAddress);
    receiverMac = senderAddr;
    stringToInt(receiverMac, receiverAddress);
    addPeer(receiverAddress);
    // TODO: savejson

    struct_message payload = struct_message();
    payload.task = MESSAGE_ONLY;
    payload.hostAddress = receiverMac;
    payload.msg = "gateways are connected!";
    esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &payload, sizeof(payload));
    success = true;
  }
  if(success) {
    Server.send(200, "text/plain", "Gateways connected!");
  } else {
    Server.send(400, "text/plain", "Invalid request params, provide mac address, ex: sender_addr=40:91:51:9F:30:AC");
  }  
}
void connectTwoESP32() {
  boolean success = false;
  stringToInt(receiverMac, receiverAddress);
  if(!esp_now_is_peer_exist(receiverAddress)) {
    Server.send(200, "text/plain", "Gateways are not connected!  Please connect gateways first.  Ex: connect_gateways?sender_addr=40:91:51:9F:30:AC");
  } else {
    if(Server.args() == 2 && Server.argName(0) == "sender_addr" && Server.argName(1) == "recv_addr") {
      String senderAddr = removeFromString(Server.arg(0), (char *)":");
      String receiverAddr = removeFromString(Server.arg(1), (char *)":");
      Serial.printf("Broacast to: %s, %u\n", receiverMac, receiverAddress);
      struct_message payload = struct_message();
      payload.task = CONNECT_WITH_ME;
      payload.hostAddress = receiverAddr;
      payload.senderAddress = senderAddr;
      esp_err_t result = esp_now_send(receiverAddress, (uint8_t *) &payload, sizeof(payload));
      // TODO: savejson
      success = true;
    }
    if(success) {
      Server.send(200, "text/plain", "They are connected!");
    } else {
      Server.send(400, "text/plain", "Invalid request params, provide mac address, ex: sender_addr=40:91:51:9F:30:AC&recv_addr=40:91:51:9F:30:AC");
    }  
  } 
}
void updateESP32() {
  boolean success = false;
  if(Server.args() == 2) {
    String hostAddr = removeFromString(Server.arg(0), (char *)":");
    String taskValue = removeFromString(Server.arg(1), (char *)":");

    stringToInt(hostAddr, tmpAddress);
    Serial.printf("%s, %u\n", hostAddr, tmpAddress);
    if(Server.argName(0) == "host_addr" && tmpAddress == hostAddress) {
      stringToInt(taskValue, tmpAddress);
      if(Server.argName(1) == "recv_addr" && tmpAddress != gatewayReceiverAddress) {
        deletePeer(gatewayReceiverAddress);
        std::copy(std::begin(tmpAddress), std::end(tmpAddress), std::begin(gatewayReceiverAddress));
        addPeer(gatewayReceiverAddress);
        success = true;

        Serial.print("Param name: ");
        Serial.println(Server.argName(0));
        Serial.print("Param value: ");
        Serial.println(Server.arg(0));
        Serial.println("------");
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
      }
      //stringToInt(hostAddr, tmpAddress);
      //if(esp_now_is_peer_exist(tmpAddress)) {
      //  Serial.println("found peer");
      //} else {
      //  Serial.println("peer not found");
      //  addPeer(tmpAddress);  
      //}
      Serial.printf("Broacast to: %s, %u\n", payload.hostAddress, gatewayReceiverAddress);
      esp_err_t result = esp_now_send(gatewayReceiverAddress, (uint8_t *) &payload, sizeof(payload));      
    }
  }
  if(success) {
    Server.send(200, "text/plain", "Good to go!");
  } else {
    Server.send(400, "text/plain", "Invalid request params");
  }  
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
    webSocketClient.begin(wsserver, wsport, path);
    // WebSocket event handler
    webSocketClient.onEvent(webSocketEvent);
    // if connection failed retry every 5s
    webSocketClient.setReconnectInterval(5000);
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

String onSaveConfig(AutoConnectAux& aux, PageArgument& args) {
  airValue = doc["airValue"] = args.arg("airValue").toInt();
  waterValue = doc["waterValue"] = args.arg("waterValue").toInt();
  SensorPin = doc["pin"] = args.arg("pin").toInt();
  doc["wsserver"] = args.arg("wsserver");
  JsonObject obj = doc.as<JsonObject>();
  wsserver = obj["wsserver"].as<String>();
  wsport = doc["wsport"] = args.arg("wsport").toInt();

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
  value = doc["pin"];
  Serial.println(value);
  aux["pin"].as<AutoConnectInput>().value = value;
  String strValue = doc["wsserver"];
  Serial.println(strValue);
  aux["wsserver"].as<AutoConnectInput>().value = strValue;
  value = doc["wsport"];
  Serial.println(value);
  aux["wsport"].as<AutoConnectInput>().value = value;
  return String();
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
    } else {
      JsonObject obj = doc.as<JsonObject>();
      airValue = doc["airValue"];
      waterValue = doc["waterValue"];
      SensorPin = doc["pin"];
      wsserver = obj["wsserver"].as<String>();
      wsport = doc["wsport"];
      int pin = doc["pin"];
      Serial.println(pin);
      config.close();
    }
  }
Serial.printf("%d, %d, %d, %d, %s, %d, %s, %s", airValue,waterValue,SensorPin,DEVICE_ID,DEVICE_NAME,espInterval,receiverMac,senderMac);

  Config.autoReconnect = true;
  Config.hostName = "liquid-prep";
  Config.ota = AC_OTA_BUILTIN;
  Config.otaExtraCaption = fwVersion;
  Portal.config(Config);
  Portal.on("/update_config", onUpdateConfig);
  Portal.on("/save_config", onSaveConfig);
  Portal.on("/", onHome);

  Server.enableCORS();
  Server.on("/moisture", moistureJson);
  Server.on("/moisture.json", moistureJson);
  Server.on("/update", updateESP32);
  Server.on("/register", registerESP32);
  Server.on("/connect_esp32", connectTwoESP32);
  Server.on("/connect_gateways", connectGateways);
  Serial.println("Connecting");
  if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
    Serial.println("My MAC address is: " + WiFi.macAddress());
    Serial.print("Wi-Fi Channel: ");
    Serial.println(WiFi.channel());
    Serial.print("Wi-Fi SSID: ");
    Serial.println(WiFi.SSID());
    hostMac = removeFromString(WiFi.macAddress(), (char *)":");
    Serial.println(hostMac);
    stringToInt(hostMac, hostAddress);
    Serial.printf("host: %u, receiver: %u\n", hostAddress, gatewayReceiverAddress);
    stringToInt(gatewayMac, hostAddress);
    Serial.printf("host: %u, receiver: %u\n", hostAddress, gatewayReceiverAddress);
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
  addPeer(gatewayReceiverAddress);  
}

void loop() {
  Portal.handleClient();
  webSocketClient.loop();
  if (WiFi.status() == WL_CONNECTED) {
    currentMillis=millis(); 
    if (currentMillis - previousMillis >= interval) {
      previousMillis = currentMillis;
      sendData(moistureJson());
    }
  }
}