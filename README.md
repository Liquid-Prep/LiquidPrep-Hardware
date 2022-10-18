# Liquid Prep - Soil Sensor

Liquid Prep Soil Sensor is the device that measures the soil moisture value and is read in the Liquid Prep App to determine the watering advice for the selected crop.

There are 2 options of soil moisture sensor devices supported;
1. **[Arduino UNO](https://www.arduino.cc/)**:

    Please go through the [Arduino UNO setup documentation](./Arduino%20UNO/User-Manual.pdf) for stepwise instructions on how to setup the sensor device. 

2. **[ESP32](http://esp32.net/)**:

    Setup documentation coming soon.

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
- Select esp32ap, the Join "esp32ap" window will pop open.  
- Select "Configure New AP" from the menu options
- Your WiFi AP should be listed as one of the options
- Select the WiFi you would like join
- Provide the passphrase to join your WiFi, if you want to assign a static ip to the sensor, uncheck "Enable DHCP" then key in the proper values.

To calibrate the sensor, go to http://yoursensorip/save_config
To get moisture level reading go to http://yoursensorip/moisture.json


