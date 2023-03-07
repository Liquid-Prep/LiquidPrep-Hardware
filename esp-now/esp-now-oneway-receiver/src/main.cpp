#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>

#define BOARD_ID 1

uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

typedef struct struct_message
{
    int currPointer;
    int id;
    char *moisture[100];
    long timestamp;
} struct_message;

struct_message myData;

esp_now_peer_info_t peerInfo;

String moistureLevel = "";
int airValue = 3440;   // 3442;  // enter your max air value here
int waterValue = 1803; // 1779;  // enter your water value here
int SensorPin = 32;
int soilMoistureValue = 0;
float soilmoisturepercent = 0;

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
}

void moistureJson()
{
    calculate();
    Serial.printf("sensor reading: %s", moistureLevel);
}

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

struct_message incoming_data;
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
    memcpy(&myData, incomingData, 12);
    Serial.print("Bytes received: ");
    Serial.println(myData.currPointer);

    Serial.println();
}

void setup()
{
    delay(1000);
    myData.currPointer = 0;
    // Init Serial Monitor
    Serial.begin(115200);
    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK)
    {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Once ESPNow is successfully Init, we will register for Send CB to
    // get the status of Trasnmitted packet
    esp_now_register_send_cb(OnDataSent);
    // Register peer
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        return;
    }
    esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
    calculate();
    // Set values to send
    dtostrf(soilmoisturepercent, 1, 2, myData.moisture[myData.currPointer]);
    myData.id = BOARD_ID;
    myData.currPointer++;
    esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

    delay(10000);
}