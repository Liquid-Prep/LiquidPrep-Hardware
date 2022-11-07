#include <WebServer.h>
#include <WiFi.h>
#include <AutoConnect.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebSocketsServer.h>

#define FIRMWARE_VERSION "0.2.2";
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

const uint8_t wsPort = 81;
WebSocketsServer webSocket(wsPort);
WebServer Server;
AutoConnect Portal(Server);
AutoConnectConfig Config;
String moistureLevel = "";
int airValue = 3440;   // 3442;  // enter your max air value here
int waterValue = 1803; // 1779;  // enter your water value here
int SensorPin = 32;
int soilMoistureValue = 0;
float soilmoisturepercent = 0;
const char *fwVersion = FIRMWARE_VERSION;
StaticJsonDocument<512> doc;

void calculate()
{
  int val = analogRead(SensorPin); // connect sensor to Analog pin

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
  // Serial.printf("sensor reading: %d - %f%\n", val, soilmoisturepercent); // print the value to serial port
  dtostrf(soilmoisturepercent, 1, 2, str);
  moistureLevel = str;
}

void moisture()
{
  calculate();
  Server.send(200, "text/html", moistureLevel);
}

void moistureJson()
{
  calculate();
  String response = "{\"moisture\": " + moistureLevel + "}";
  Server.send(200, "text/json", response);
  Serial.printf("sensor reading: %s", moistureLevel);
}

String onHome(AutoConnectAux &aux, PageArgument &args)
{
  calculate();
  Serial.println(moistureLevel);
  aux["results"].as<AutoConnectText>().value = moistureLevel;
  return String();
}
String saveJson()
{
  String msg = "";
  File configFile = SPIFFS.open("/config.json", "w+");
  if (configFile)
  {
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

String onSaveConfig(AutoConnectAux &aux, PageArgument &args)
{
  airValue = doc["airValue"] = args.arg("airValue").toInt();
  waterValue = doc["waterValue"] = args.arg("waterValue").toInt();
  String msg = saveJson();
  aux["results"].as<AutoConnectText>().value = msg;
  return String();
}

String onUpdateConfig(AutoConnectAux &aux, PageArgument &args)
{
  int value = doc["waterValue"];
  Serial.println(value);
  aux["waterValue"].as<AutoConnectInput>().value = value;
  value = doc["airValue"];
  Serial.println(value);
  aux["airValue"].as<AutoConnectInput>().value = value;
  return String();
}

void setup()
{
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  SPIFFS.begin();

  File page = SPIFFS.open("/page.json", "r");
  if (page)
  {
    Portal.load(page);
    page.close();
  }
  File config = SPIFFS.open("/config.json", "r");
  if (config)
  {
    DeserializationError error = deserializeJson(doc, config);
    if (error)
    {
      Serial.println(F("Failed to read file, using default configuration"));
      Serial.println(error.c_str());
    }
    else
    {
      airValue = doc["airValue"];
      waterValue = doc["waterValue"];
      SensorPin = doc["pin"];
      int pin = doc["pin"];
      Serial.println(pin);
      config.close();
    }
  }

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

  Serial.println("WiFi connected: " + WiFi.localIP().toString());

  if (Portal.begin())
  {
    Serial.println("WebSocket begins");
    webSocket.begin(); // <--- After AutoConnect::begin
    webSocket.onEvent([](uint8_t num, WStype_t type, uint8_t *payload, size_t length)
                      {
      try {
        switch(type) {
        case WStype_DISCONNECTED:
          Serial.printf("[%u] Disconnected!\n", num);
          break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Connected from %s\n", num, ip.toString().c_str());
            webSocket.sendTXT(num, "Connected");
          }
          break;
        case WStype_TEXT:
          Serial.printf("[%u] get Text: %s\n", num, payload);
          calculate();
          String response = "{\"moisture\": " + moistureLevel + "}";
          webSocket.sendTXT(num, response);
          // send some response to the client
          // webSocket.sendTXT(num, "message here");
          break;
        }
      } catch(...) {
        Serial.println("Catch web socket errors");
      } });
  }

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
}

void loop()
{
  webSocket.loop();
  Portal.handleClient();
  calculate();
  delay(1000);
}