#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <AutoConnect.h>
#include <HTTPClient.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define FIRMWARE_VERSION           "0.2.2";
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCharacteristic;
WebServer Server;
AutoConnect Portal(Server);
AutoConnectConfig Config;
HTTPClient http;
WiFiClientSecure client;
String moistureLevel = "";
int airValue = 3440; //3442;  // enter your max air value here
int waterValue = 1803; //1779;  // enter your water value here
int SensorPin = 32;
int soilMoistureValue = 0;
float soilmoisturepercent=0;
const char* fwVersion = FIRMWARE_VERSION;
DynamicJsonDocument doc(1024);

unsigned long previousMillis = 0;
unsigned long currentMillis;
unsigned long interval=60000; //interval for sending data to the websocket server in ms

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
  pCharacteristic->setValue(str);  // push the value via bluetooth
//https://community.blynk.cc/t/interesting-esp32-issue-cant-use-analogread-with-wifi-and-or-esp-wifimanager-library/49130
}

void moisture() {
  calculate();
  Server.send ( 200, "text/html", moistureLevel );
}

void moistureJson() {
  calculate();
  String response = "{\"moisture\": " + moistureLevel + "}";
  Server.send( 200, "text/json", response);
}

String onHome(AutoConnectAux& aux, PageArgument& args) {
  calculate();
  Serial.println(moistureLevel);
  aux["results"].as<AutoConnectText>().value = moistureLevel;
  return String();
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
boolean saveJsonFile(String file) {
  String msg = "";
  boolean success = false;
  File configFile = SPIFFS.open(file, "w+"); 
  if(configFile) {
    serializeJson(doc, configFile);
    msg = "Updated " + file + " config successfully!";
    configFile.close();
    success = true;
  } else {
    msg = "Failed to open " + file + " config file for writing!";
  }
  Serial.println(msg);
  return success;
}

String onSaveConfig(AutoConnectAux& aux, PageArgument& args) {
  airValue = doc["airValue"] = args.arg("airValue").toInt();
  waterValue = doc["waterValue"] = args.arg("waterValue").toInt();
  SensorPin = doc["pin"] = args.arg("pin").toInt();
  String msg = saveJsonFile("/config.json") ? "Updated config successfully" : "Failed to update";
  aux["results"].as<AutoConnectText>().value = msg;
  return String();
}

String onUpdateConfig(AutoConnectAux& aux, PageArgument& args) {
  int value = doc["waterValue"];
  Serial.println(value);
  aux["waterValue"].as<AutoConnectInput>().value = value;
  value = doc["airValue"];
  Serial.println(value);
  aux["airValue"].as<AutoConnectInput>().value = value;
  value = doc["pin"];
  Serial.println(value);
  aux["pin"].as<AutoConnectInput>().value = value;
  return String();
}

void enableBluetooth() {
    Serial.println("Starting BLE work!");

  BLEDevice::init("ESP32-LiquidPrep");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_READ |
                                         BLECharacteristic::PROPERTY_WRITE
                                       );
  // pCharacteristic->setValue("92");  // use this to hard-code value sent via bluetooth (for testing)
  
  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");                                       
}

void createConfigJson() {
  char json[] = "{\"uuid\":{\"SERVICE_UUID\":\"4fafc201-1fb5-459e-8fcc-c5c9c331914b\",\"CHARACTERISTIC_UUID\": \"beb5483e-36e1-4688-b7f5-ea07361b26a8\"},\"airValue\":3440,\"waterValue\":1803,\"pin\":32}";
  deserializeJson(doc, json);
  saveJsonFile("/config.json");  
}

void createPageJson() {
  char json[] = "[{\"title\":\"Config\",\"uri\":\"/update_config\",\"menu\":true,\"element\":[{\"name\":\"header\",\"type\":\"ACText\"},{\"name\":\"caption1\",\"type\":\"ACText\",\"value\":\"Air value\"},{\"name\":\"airValue\",\"type\":\"ACInput\"},{\"name\":\"caption2\",\"type\":\"ACText\",\"value\":\"Water value\"},{\"name\":\"waterValue\",\"type\":\"ACInput\"},{\"name\":\"caption3\",\"type\":\"ACText\",\"value\":\"Pin\"},{\"name\":\"pin\",\"type\":\"ACInput\"},{\"name\":\"caption4\",\"type\":\"ACText\",\"value\":\"Service uuid\"},{\"name\":\"serviceUuid\",\"type\":\"ACInput\"},{\"name\":\"caption5\",\"type\":\"ACText\",\"value\":\"Characteristic uuid\"},{\"name\":\"characteristicUuid\",\"type\":\"ACInput\"},{\"name\":\"save\",\"type\":\"ACSubmit\",\"value\":\"SAVE\",\"uri\":\"/save_config\"}]},{\"uri\":\"/save_config\",\"title\":\"Save configuration\",\"menu\":false,\"element\":[{\"name\":\"results\",\"type\":\"ACText\",\"value\":\"\"}]},{\"uri\":\\/\",\"title\":\"Moisture reading\",\"menu\":false,\"element\":[{\"name\":\"moisture\",\"type\":\"ACText\",\"value\":\"Moisture:  \"},{\"name\":\"results\",\"type\":\"ACText\",\"value\":\"...\"},{\"name\":\"save\",\"type\":\"ACSubmit\",\"value\":\"Read again...\",\"uri\":\"/\"}]}]";
  deserializeJson(doc, json);
  if(saveJsonFile("/page.json") == true) {
    File page = SPIFFS.open("/page.json", "r");
    if(page) {
      Portal.load(page);
      page.close();
    }
  } 
}

void loadConfigJson() {
  File config = SPIFFS.open("/config.json", "r");
  if(!config || config.size() <= 0) {
    createConfigJson();
  } else {
    DeserializationError error = deserializeJson(doc, config);
    if(error) {
      Serial.println(F("Failed to read file, using default configuration"));
      Serial.println(error.c_str());
      if(error.c_str() == "EmptyInput") {
        createConfigJson();
      }
    } else {
      airValue = doc["airValue"];
      waterValue = doc["waterValue"];
      SensorPin = doc["pin"]; 
      int pin = doc["pin"];
      Serial.println(pin);
      config.close();
    }
  }
}
void loadPageJson() {
  File page = SPIFFS.open("/page.json", "r");
  if(!page || page.size() <= 0) {
    Serial.println("no page: ");
    createPageJson();
  } else {
    Portal.load(page);
    page.close();
  }
}
void setup() {
  int waitCount = 0;
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  enableBluetooth();

  while (!SPIFFS.begin(true) && waitCount++ < 3) {
    delay(1000);
  }
  loadPageJson();
  loadConfigJson();

  Config.autoReconnect = true;
  Config.hostName = "liquid-prep";
  Config.ota = AC_OTA_BUILTIN;
  Config.otaExtraCaption = fwVersion;
  Portal.config(Config);
  Portal.on("/update_config", onUpdateConfig);
  Portal.on("/save_config", onSaveConfig);
  Portal.on("/", onHome);
  Portal.begin();

  Server.enableCORS();
  Server.on("/moisture", moisture);
  Server.on("/moisture.json", moistureJson);
  Serial.println("Connecting");
  if (Portal.begin()) {
    Serial.println("WiFi connected: " + WiFi.localIP().toString());
  }
  waitCount = 0;
  while (WiFi.status() != WL_CONNECTED && waitCount++ < 3) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.print("Failed to connect to WiFi");
  } else {
  }
}

void loop() {
  Portal.handleClient();
  currentMillis=millis(); 
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    calculate();
  }
}