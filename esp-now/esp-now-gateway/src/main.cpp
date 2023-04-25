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
int airValue = 3440; //3442;  // enter your max air value here
int waterValue = 1803; //1779;  // enter your water value here
int sensorPin = 32;
int soilMoistureValue = 0;
float soilmoisturepercent=0;
const char* fwVersion = FIRMWARE_VERSION;
DynamicJsonDocument doc(1024);

WebSocketsClient webSocketClient;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;

// TODO: allow input certain values in webtools and writ to SPIFFS at the time of flashing
int espInterval=60000; //espInterval for reading data
String wsserver = "192.168.86.24";  //ip address of Express server
int wsport= 3000;
char path[] = "/";   //identifier of this device
boolean webSocketConnected=0;
String data= "";

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

String saveJson() {
  String msg = "";
  File configFile = SPIFFS.open("/config.json", "w+"); 
  if(configFile) {
  Serial.printf("%d, %d, %d, %d, %s, %d, %s, %s\n", airValue,waterValue,sensorPin,DEVICE_ID,DEVICE_NAME,espInterval,receiverMac,senderMac);
    doc["deviceId"] = DEVICE_ID;
    doc["deviceName"] = DEVICE_NAME;
    doc["airValue"] = airValue;
    doc["waterValue"] = waterValue;
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
  Serial.printf("Last Packet Send Status: %u, %u, %s\t", receiverAddress, mac_addr, receiverMac);
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  Serial.println("");
}

void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  struct_message payload;

  memcpy(&payload, incomingData, sizeof(payload));
  Serial.print("Bytes received: ");
  Serial.printf("%d, %u\n", len, mac);
  String response = "";
  if(payload.task == PING) {
    // TODO: check if it's already seen
    Serial.println("Ping: " + payload.name + ", " + payload.id + ", " + payload.type + ", " + payload.task);
    Serial.println();
    response = "{\"mac\": \"" + payload.senderAddress + "\", \"id\": " + String(payload.id) + ", \"name\": \"" + payload.name + "\", \"type\": " + String(payload.type) + ", \"task\": " + String(payload.task) + "}";
  } else if(payload.task == QUERY_RESULT || payload.task == CALIBRATE_RESULT) {
    // TODO: check if it's already seen
    Serial.println("Query: " + payload.name + ", " + payload.id + ", " + payload.type + ", " + payload.task);
    Serial.println();
    response = "{\"mac\": \"" + payload.senderAddress + + "\", \"interval\": " + String(payload.espInterval) + ", \"id\": " + String(payload.id) + ", \"name\": \"" + payload.name + "\", \"msg\": \"" + payload.msg + "\", \"type\": " + String(payload.type) + ", \"task\": " + String(payload.task) + "}";
  } else {
    Serial.println(payload.name + ", " + payload.id + ", " + payload.moisture + ", " + payload.task);
    Serial.println();
    response = "{\"mac\": \"" + payload.senderAddress + "\", \"id\": " + String(payload.id) + ", \"name\": \"" + payload.name + "\", \"moisture\": " + payload.moisture + "}";
  }
  sendData(response);
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
    if(targetHostAddr == hostMac) {
      if(Server.arg(0) == "air_value") {
        calibrateAir(airValue, sensorPin);
      } else {
        calibrateWater(waterValue, sensorPin);
      }
      saveJson();
      sprintf(payload.msg, "%d,%d,%d,%s,%s", airValue, waterValue, sensorPin, senderMac.c_str(), receiverMac.c_str());
      String response = "{\"mac\": \"" + hostMac + + "\", \"interval\": " + String(espInterval) + ", \"id\": " + String(DEVICE_ID) + ", \"name\": \"" + DEVICE_NAME + "\", \"msg\": \"" + payload.msg + "\", \"type\": " + String(CALIBRATE_RESULT) + "}";
      sendData(response);
    } else {
      int task = Server.arg(0) == "air_value" ? CALIBRATE_AIR : CALIBRATE_WATER;
      setPayload(payload, DEVICE_ID, DEVICE_NAME, targetHostAddr, senderMac, receiverMac, task, BROADCAST, "");
      esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
    }
  }
  httpResponse(success, "Calibrate",  "Invalid request params, correct params: value=air_value&host_addr=... OR value=water_value&host_addr=...");
}
void queryESP() {
  boolean success = false;
  if(Server.args() == 1 && Server.argName(0) == "host_addr") {
    success = true;
    String targetHostAddr = removeFromString(Server.arg(0), (char *)":");
    struct_message payload = struct_message();
    if(targetHostAddr == hostMac) {
      sprintf(payload.msg, "%d,%d,%d,%s,%s", airValue, waterValue, sensorPin, senderMac.c_str(), receiverMac.c_str());
      String response = "{\"mac\": \"" + hostMac + + "\", \"interval\": " + String(espInterval) + ", \"id\": " + String(DEVICE_ID) + ", \"name\": \"" + DEVICE_NAME + "\", \"msg\": \"" + payload.msg + "\", \"type\": " + String(QUERY_RESULT) + "}";
      sendData(response);
    } else {
      //payload.id = DEVICE_ID;
      //payload.name = DEVICE_NAME;
      //payload.hostAddress = targetHostAddr;
      //payload.senderAddress = hostMac;
      //payload.task = QUERY;
      setPayload(payload, DEVICE_ID, DEVICE_NAME, targetHostAddr, hostMac, "", QUERY, BROADCAST, "");
      //stringToInt(gatewayReceiverMac, tmpAddress);
      esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
    }  
  }
  httpResponse(success, "Query ESP",  "Invalid request params, correct params: host_addr=...");
}
void pingESP() {
  boolean success = false;  
  if(Server.args() == 1 && Server.argName(0) == "host_addr") {
    String targetHostAddr = removeFromString(Server.arg(0), (char *)":");
    success = true;
    struct_message payload = struct_message();
    //payload.id = DEVICE_ID;
    //payload.name = DEVICE_NAME;
    //payload.hostAddress = targetHostAddr;
    //payload.senderAddress = hostMac;
    //payload.task = PING;
    //sprintf(payload.msg, "ping from %s", DEVICE_NAME.c_str());                 
    //stringToInt(gatewayReceiverMac, tmpAddress);
    setPayload(payload, DEVICE_ID, DEVICE_NAME, targetHostAddr, hostMac, "", PING, BROADCAST, DEVICE_NAME);
    Serial.printf("%d, %s, %s, %s, %d, %s\n", payload.id,payload.name,payload.hostAddress,payload.senderAddress,payload.task,payload.msg);
    esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));
  }
  if(success) {
    Server.send(200, "text/plain", "Ping!");
  } else {
    Server.send(400, "text/plain", "Invalid request params, correct params: host_addr=...");
  }  
}
void configureGateway() {
  struct_message payload = struct_message();
  boolean success = false;  
  if(Server.args() == 4 && Server.argName(0) == "wsserver" && Server.argName(1) == "wsport" && Server.argName(2) == "sender_addr" && Server.argName(3) == "pin") {
    wsserver = removeFromString(Server.arg(0), (char *)":");
    wsport = atoi(removeFromString(Server.arg(1), (char *)":").c_str());
    gatewayReceiverMac = removeFromString(Server.arg(2), (char *)":");
    sensorPin = atoi(removeFromString(Server.arg(3), (char *)":").c_str());
  Serial.printf("%d, %d, %d, %d, %s, %d, %s, %s\n", airValue,waterValue,sensorPin,DEVICE_ID,DEVICE_NAME,espInterval,gatewayReceiverMac,senderMac);
    saveJson();
    success = true;
  }  
  if(success) {
    Server.send(200, "text/plain", "ESP-NOW Gateway configured!");
    delay(3000);
    restartESP();
  } else {
    Server.send(400, "text/plain", "Invalid request params, correct params: wsserver=...&wsport=...&sender_addr=...&pin=...");
  }  

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
void connectGateway() {
  boolean success = false;
  if(Server.args() == 1 && Server.argName(0) == "sender_addr") {
    String senderAddr = removeFromString(Server.arg(0), (char *)":");
    stringToInt(gatewayReceiverMac, gatewayReceiverAddress);
    deletePeer(gatewayReceiverAddress);
    gatewayReceiverMac = senderAddr;
    stringToInt(gatewayReceiverMac, gatewayReceiverAddress);
    addPeer(gatewayReceiverAddress);
    saveJson();

    struct_message payload = struct_message();
    payload.task = MESSAGE_ONLY;
    payload.hostAddress = receiverMac;
    sprintf(payload.msg, "gateways are connected");  
    Serial.printf("connecting gateway with %s\n", gatewayReceiverMac);
    esp_now_send(gatewayReceiverAddress, (uint8_t *) &payload, sizeof(payload));
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
  stringToInt(gatewayReceiverMac, gatewayReceiverAddress);
  if(!esp_now_is_peer_exist(gatewayReceiverAddress)) {
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
      esp_err_t result = esp_now_send(gatewayReceiverAddress, (uint8_t *) &payload, sizeof(payload));
      //saveJson();
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
      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &payload, sizeof(payload));      
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

String onSaveConfig(AutoConnectAux& aux, PageArgument& args) {
  airValue = doc["airValue"] = args.arg("airValue").toInt();
  waterValue = doc["waterValue"] = args.arg("waterValue").toInt();
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
      airValue = doc["airValue"];
      waterValue = doc["waterValue"];
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
  Serial.printf("%d, %d, %d, %d, %s, %d, %s, %s\n", airValue,waterValue,sensorPin,DEVICE_ID,DEVICE_NAME,espInterval,receiverMac,senderMac);

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
  Server.on("/connect_gateway", connectGateway);
  Server.on("/configure_gateway", configureGateway);
  Server.on("/reboot_gateway", restartESP);
  Server.on("/ping", pingESP);
  Server.on("/query", queryESP);
  Server.on("/calibrate", calibrate);
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
  Serial.printf("Adding peer: %s, %u\n", receiverMac, gatewayReceiverAddress);
  addPeer(broadcastAddress);  
  //addPeer(gatewayReceiverAddress);  
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