# ESP-NOW
The following two topologies uses ESP-NOW communication protocol to relate sensor data to the edge gateway by bridging between multiple esp32.  According to documentation, ESP-NOW protocol has maximum range of 480 meters between esp32's.  

## Many-To-One
![](https://github.com/Liquid-Prep/LiquidPrep-Hardware/raw/esp-now-websocket/esp-now/many-to-one.jpg "Many To One") 

## One-To-One
![](https://github.com/Liquid-Prep/LiquidPrep-Hardware/raw/esp-now-websocket/esp-now/one-to-one.jpg "One To One") 


## Configure esp gateway
- Configure gateway in your browser with the following url
  - http://{esp-now-gateway-ip}/configure_gateway?wsserver={express-server-ip}&wsport={websocket-port}&sender_addr={esp-now-gateway-mac-address}&pin=32
    - esp-now-gateway-ip:  the static ip address that was assigned to esp-now gateway
    - express-server-ip:  the ip address to the express webserver running on edge device
    - websocket-port:  the web socket port express server is listening on
    - esp-now-gateway-mac-address:  the mac address of esp-now gateway ex: (78:21:84:8C:89:FC)

# Many-To-One Topology
## Register esp worker
- http://{esp-now-gateway-ip}/register?host_addr={worker-mac-address}&recv_addr={esp-now-gateway-mac-address}&device_id={id}&device_name={zone-name}
  - esp-now-gateway-ip:  the static ip address that was assigned to esp-now gateway
  - worker-mac-address:  the mac address of this worker
  - esp-now-gateway-mac-address: the mac address of the esp-now gateway
  - id: assign an id for this work start with 1 for the first worker, 2 for the next & etc.
  - name: give it a name

# One-To-One Topology
## Register esp first worker(leader)
- http://{esp-now-gateway-ip}/register?host_addr={worker-mac-address}&recv_addr={esp-now-gateway-mac-address}&device_id={id}&device_name={zone-name}
  - Same as Many-To-One
## Connect first esp worker(leader) to esp gateway
- register esp worker  
- http://{esp-now-gateway-ip}/connect_gateway?sender_addr={leader-mac-address}
## Connect second and the rest of esp's in series
- register esp worker
- Connect second worker to leader  
  - http://{esp-now-gateway-ip}/connect_esp32?sender_addr={second-worker-mac-address}&recv_addr={leader-mac-address}
- Connect third worker to second worker  
  - http://{esp-now-gateway-ip}/connect_esp32?sender_addr={third-worker-mac-address}&recv_addr={second-worker-mac-address}
- Repeat for the rest of esp's

# Available commands to interact with esp32's
- http://{esp-now-gateway-ip}/register?host_addr={worker-mac-address}&recv_addr={esp-now-gateway-mac-address}&device_id={id}&device_name={zone-name}
- http://{esp-now-gateway-ip}/connect_esp32?sender_addr={second-worker-mac-address}&recv_addr={leader-mac-address}
- http://{esp-now-gateway-ip}/update?host_addr={esp-mac-address}&esp_interval=30000
- http://{esp-now-gateway-ip}/ping?host_addr=78:21:84:8C:58:00
- http://{esp-now-gateway-ip}/query?host_addr=78:21:84:8C:58:00
- http://{esp-now-gateway-ip}/calibrate?value=air_value&host_addr=78:21:84:8C:9D:34
- http://{esp-now-gateway-ip}/calibrate?value=water_value&host_addr=78:21:84:8C:9D:34

