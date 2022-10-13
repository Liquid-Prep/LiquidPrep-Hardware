
# Liquid Prep - Hardware

Liquid Prep hardware is the device that measures the soil moisture value and is read in the Liquid Prep App to determine the watering advice for the selected crop.

## Table of Contents
1. [Soil Moisture Device Support](#soil-moisture-device-support)
2. [Troubleshooting](#troubleshooting)

## Soil Moisture Device Support
There are 2 options of soil moisture sensor devices supported;

1. **[Arduino UNO](https://www.arduino.cc/)**:
   Please go through the [Arduino UNO setup documentation](./Arduino%20UNO/User-Manual.pdf) for stepwise instructions on how to setup the Arduino based sensor device.
2. **[ESP32](http://esp32.net/)**:
  Please see the [ESP32 setup video](https://youtu.be/EU28Z_lu67w) for stepwise instructions on how to setup the ESP32 based sensor device.

## Troubleshooting
- **The USB com port isn’t recognized during flashing**
  - Unplug the microcontroller and plug it back in. This will restart it and it should then be recognized.
- **Android bluetooth sometimes does not connect**
  - Unplug the microcontroller and plug it back in. This will restart it and it should then connect.
- **Program size larger than the default**
  - In the IDE, choose a different built-in partition scheme from the Tools menu. 
	Click on _Tools > Partition scheme: No OTA (Large APP)_. This will allocate enough memory for it to write.
![large-app](large-app)
- **Arduino out of the box is missing libraries needed for the card.**
  - Libraries to include:
   -- [ArduinoJson](https://www.arduino.cc/reference/en/libraries/arduino_json/)
   -- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)
   -- [AutoConnect](https://www.arduino.cc/reference/en/libraries/autoconnect/)
   -- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
   -- [PageBuilder](https://www.arduino.cc/reference/en/libraries/pagebuilder/)

   - To install a new library into your Arduino IDE* you can use the Library Manager. Open the IDE and click to the "Sketch" menu and then _Include Library > Manage Libraries_.
  
	  \* It is recommended to use VSCode with the [PlatformIO IDE extension](https://docs.platformio.org/en/latest/integration/ide/vscode.html) instead of Arduino IDE. It is more user-friendly.
