
# ESP-NOW
The following two topologies uses ESP-NOW communication protocol to relate sensor data to the edge gateway by bridging between multiple esp32.  According to documentation, ESP-NOW protocol has maximum range of 480 meters between esp32's.  

## Mesh
![Mesh](https://github.com/Liquid-Prep/LiquidPrep-Hardware/blob/develop/esp-now/mesh.jpg?raw=true) 

## To setup Mesh
- Flash esp-now-gateway firmware onto the esp32 that is designated as the esp-now gateway from https://playground.github.io/liquid-prep-web-tools/ and follow the instructions 
  - To access the esp-now gateway web UI, join the esp32ap Access Point from your computer
  - In your Chrome browser, enter http://172.217.28.1/_ac/update
  - Update the config with config.json and page.json
  - Join the WiFi Access Point of your home network
  - Restart the esp32(esp-now-gateway) , then goto http://<esp32-ip>/update_config (This is needed to work with Edge Gateway to establish two way communication between esp-now-gateway and Edge Gateway) 
    - Update WebSocket Server to the ip address of the Edge Gateway
    - Update WebSocket Port to the port number that the WebSocket Server is listening on
    - Start Liquid Prep App by either loading from http://localhost:<port> of Edge Gateway or from https://open-horizon-services.github.io/service-liquid-prep, then assign proper values to the following fields
      - Edge Gateway
      - WebSocket
      - ESP-NOW Gateway

    ![Gateway Configuration](https://github.com/Liquid-Prep/LiquidPrep-Hardware/blob/develop/esp-now/gateway-info.png?raw=true) 

- Flash esp-mesh-bluetooth firmware from https://playground.github.io/liquid-prep-web-tools/ onto the esp32's, these esp32 sensors are workers that will broadcast the moisture readings to the esp-now gateway

- Once the flashing is complete and the configuration is set for the sensors, go to https://open-horizon-services.github.io/service-liquid-prep/ in your Chrome browser to start the Liquid Prep App.
  - By default the worker sensors(esp-mesh-bluetooth) are set to communicate on channel 11, esp-mesh-bluetooth need to be on the same channel as the esp-now-gateway in order to establish communication.
  - Let's go to Liquid Prep App to query the esp-now-gateway as shown below to obtain the channel # it's using  
  ![Query Sensor](https://github.com/Liquid-Prep/LiquidPrep-Hardware/blob/develop/esp-now/query-sensor.png?raw=true) 
  - Now got to update the esp-mesh-bluetooth sensors via bluetooth in the Liquid Prep App open-horizon-services.github.io/service-liquid-prep/my-crops
    - Note: This step is bit hidden and is temporary but will be improved in our next release with the new UI design 
    - Get to the following screen to update the WiFi Channel for the esp-mesh-bluetooth sensors
    Note:  There is a time limit for entering the channel number, so do it fast :-)
    ![Update WiFi Channel](https://github.com/Liquid-Prep/LiquidPrep-Hardware/blob/develop/esp-now/update-wifi-channel.png?raw=true) 
    - Once you have provide the channel number and click ok, you will be prompted with a modal with a list of available sensors.  Select the one you would like to update.  
    - The esp-now-gateway should be receiving moisture readings from these esp-now-bluetooth sensors
    - The esp-now-gateway will also be relating the data broadcasted by all the esp-now-bluetooth sensors to the Edge Gateway via WebSocket.  The Liquid Prep App that is being served by this Edge Gateway will be able to view all the sensors last reported moisture reading

# Setup Edge Gateway
  - Option 1: Use Open Horizon to manage software lifecycle and placement of application workload/updates
    - Install Open Horizon Agent on local device (any computing device that supports Docker runtime)
    - Register Agent with liquid-prepe-express service


  - Option 2: Run as a standalone application and manage everything yourself
    - Docker pull playbox21/liquid-prep-express_amd64:1.0.9
    - docker run -d -it --rm -p 3003:3000 playbox21/liquid-prep-express_arm64:1.0.9


  - Option 3: For running the code locally and for development
    - git clone https://github.com/open-horizon-services/service-liquid-prep.git
    - checkout develop branch
    - cd into liquid-prep/liquid-prep-app, run ```npm install``` then run ```npm run deploy```
    - cd into liquid-prep/liquid-prep-express, run ```npm install``` then run ```PORT=3003 npm run watch:deploy```

Note:  When accessing the liquid prep app via https://localhost:3003 or https://edge-gateway-ip:3003 you will get a Not Secure warning from the browser.  For now just proceed with the warning, we are looking into obtaining a SSL certificate.  When accessing https://edge-gateway-ip:3003 some features that require SSL may not function.


