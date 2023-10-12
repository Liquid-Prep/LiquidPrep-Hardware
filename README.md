# Liquid Prep - Hardware

Liquid Prep hardware is the device that measures the soil moisture value and is read in the Liquid Prep App to determine the watering advice for the selected crop.

## Table of Contents

1. [Soil Moisture Device Support](#soil-moisture-device-support)
2. [Troubleshooting](#troubleshooting)

## Soil Moisture Device Support

There are 2 options of soil moisture sensor devices supported;

1. **[Arduino UNO](https://www.arduino.cc/)**:
   Please go through the [Arduino UNO setup documentation](./Arduino%20UNO/User-Manual.pdf) for stepwise instructions on how to setup the Arduino based sensor device.
2. **[ESP32](http://esp32.net/)**: [Stepwise instructions for ESP32 IoT setup](https://github.com/Liquid-Prep/LiquidPrep-Hardware#Stepwise-instructions-for-ESP32-IoT-setup)

# For End Users
## Flash firmware directly from [Liquid Prep Web Tools](https://playground.github.io/liquid-prep-web-tools/)

# For Developers

## Develop/build firmware with PlatformIO
    Intall PlatformIO plugin in VSCode
    Open "Soil Moisture Sensor" project

    Build & Upload firmware to ESP32

### Notes
    Upload page.json and config.json to update UI
    esptool.py --chip esp32 -p /dev/cu.usbserial-0001 erase_flash
    http://192.168.1.xxx/_ac/update
    http://192.168.1.xxx/_ac/config
    ESP32 default ip 192.168.4.1, 192.168.1.184 

Sets IP address for Soft AP in captive portal. When AutoConnect fails the initial WiFi.begin, it starts the captive portal with the IP address specified this.

The default IP value is 172.217.28.1
password: 12345678


# Set up your soil moisture sensor to work with Liquid Prep
After plugging usb cable and the ESP32 is powered on, you will see an esp32ap as one of your WiFi AP(access point) options.  
- Select esp32ap, the Join "esp32ap" window will pop open. ![Alt text](image1.jpg?raw=true "Title") 
- Select "Configure New AP" from the menu options
- Your WiFi AP should be listed as one of the options
- Select the WiFi you would like join
- Provide the passphrase to join your WiFi, if you want to assign a static ip to the sensor, uncheck "Enable DHCP" then key in the proper values. ![Alt text](image2.jpg?raw=true "Title")
- To calibrate the sensor, go to http://yoursensorip/save_config
- To get moisture level reading go to http://yoursensorip/moisture.json

# Stepwise instructions for ESP32 IoT setup

- Requirements for setup:
  - Computer 
  - ESP32
  - Three way adapter 
  - Thin rail mounting brackets
  - Weather sealed enclosure (moderately sealed) - [A waterproof junction box like this would work](https://www.desertcart.in/products/53076593) 
  - Micro USB Cable
  - Hockey Tape
  - Soil moisture sensor
  - Tape Measure
  - Connector wires
  - Velcro
  - Drilling machine
  - Zipties
  - thin rail 
  - hot glue
- Connect the ESP32 to your computer using the microUSB cable.
- Extend length of the connector wires to make sure they are long enough to connect from the sensor to the GPIO pins on the ESP32.
- Connect wires to GND (Ground), V5 (5 Volts), P4 (Port #4).
- Download and install [Ardunio IDE](arduino.cc/en/Guide) for your OS, to flash firmware to ESP32.
- Obtain a copy of the necessary firmware by copying the code from [ESP32_BLE_server.ino](https://github.com/nnethaji/LiquidPrep-Hardware/blob/main/ESP32/ESP32_BLE_server.ino) file.
- Paste the copied code into the Ardunio editor.
- From the Ardunio editor's menu bar select Tools -> Board: "Node32s" -> ESP32 Arduino -> Node32s.
- Set Upload speed to 115200.
- From the same Ardunio editor menu select Tools -> Serial Monitor. This can also be enabled with the shortcut Shift + Command + M. This is used for sensor calibration.
- Flash the firmware by clicking on the Upload button. If the compiliation is successful, the software will begin writing to the ESP32.
- Note: Some versions of the ESP32 may require the user to hold down the reset button on the ESP32 when it attempts to connect. This will put the ESP32 in Flash mode.
- Once this is done, the serial monitor will display measurements from the sensor on a new window.
- From the measurements titled sensor reading, find the min and max value.
- Set the mix and max variables in the code you copied to the IDE to these values. Note that you need to assign the min value from the reading to the max variable in the code and the max value from the reading to the min variable in the code. 
- Click on the right arrow button (next to the tick button) in the IDE window again, this will flash the firmware for the second time.
- Open the liquid prep webapp and connect ESP32 via the adapter, select the image with the crop and click on the blue measurement icon.
- This will prompt you to connect to the sensor. In the step 3, select connect with bluetooth.
- Select ESP32 liquid prep, click on pair button. This will connect it to the sensor.
- This will return a value indicating the current moisture level of whatever medium the ESP32 is submerged in or inserted into.
- Now proceed to assemble the moisture sensor
- Remove labels and stick to the sides of the ESP32 port
- Stick velcro to the base of the ESP32
- Now stick two other pieces of velcro to both sides of an 1 inch stryofoam piece of the same length as the sensor
- Stick the base of the ESP32 on top of the velcro that is on one side of the stryofoam. Cut the stryofoam to make sure that the stryofoam can essentially fit within the body of the ESP32. That is the pins of the ESP32 should stick out and not poke into the stryofoam.
- Take another piece of velcro and stick it onto the weather sealed enclosure.
- Stick the stryofoam's other side onto this (the side without the ESP32).
- Wrap cardboard around the thin rail and secure it with hockey tape.
- Use screwdriver to pierce holes into the enclosure so that the connecting wire can pass through it.
- Mark the centre points on both sides (the points would be between adjacent screwholes) and drill holes through the lid of the enclosure.
- Mount mounting bracket onto the thin rail, screw the lid onto the rail with the bracket.
- Place the rest of the enclosure (which has the ESP32 inside) and close it.
- Using hot glue, stick the moisture sensor to one edge of the thin rail
- This edge should be inserted into the soil to check the moisture level.
  
# References
   Please see the [ESP32 setup video](https://youtu.be/EU28Z_lu67w) for further detail on the ESP32 setup process.

## Troubleshooting

- **The USB com port isn’t recognized during flashing**
  - Unplug the microcontroller and plug it back in. This will restart it and it should then be recognized.
- **Android bluetooth sometimes does not connect**
  - Unplug the microcontroller and plug it back in. This will restart it and it should then connect.
- **Program size larger than the default**
  - In the IDE, choose a different built-in partition scheme from the Tools menu.
    Click on _Tools > Partition scheme: No OTA (Large APP)_. This will allocate enough memory for it to write.
    ![Partition Scheme Example](/assets/large-app.png)
- **Arduino out of the box is missing libraries needed for the card.**

  - Libraries to include:
    -- [ArduinoJson](https://www.arduino.cc/reference/en/libraries/arduino_json/)
    -- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)
    -- [AutoConnect](https://www.arduino.cc/reference/en/libraries/autoconnect/)
    -- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
    -- [PageBuilder](https://www.arduino.cc/reference/en/libraries/pagebuilder/)

  - To install a new library into your Arduino IDE\* you can use the Library Manager. Open the IDE and click to the "Sketch" menu and then _Include Library > Manage Libraries_.

  \* It is recommended to use VSCode with the [PlatformIO IDE extension](https://docs.platformio.org/en/latest/integration/ide/vscode.html) instead of Arduino IDE. It is more user-friendly.
