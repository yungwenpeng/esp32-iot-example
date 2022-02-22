# ESP32 With Thingsboard  
   Driver: DHT11 Temperature and Humidity Sensor  
   Network Configuration: [Wi-Fi Provisioning Manager](https://github.com/espressif/esp-idf/tree/master/examples/provisioning/wifi_prov_mgr)  
   Cloud platforms: [Thingsboard](https://thingsboard.io/)  
   OTA Firmware Upgrades: using [Thingsboard](https://thingsboard.io/docs/samples/esp32/ota)  
  
## ESP-IDF Training  
   Repository showing "how-to-build" fully functional IoT product with ESP-IDF framework.  
    
## How to use:  
   $ git clone --recurse-submodules -j4 git@github.com:yungwenpeng/esp32-iot-example.git  
   $ cd esp32-iot-example/esp-idf  
   $ ./install.sh  
   $ . ./export.sh  
  
   $ cd ../example  
   $ idf.py menuconfig  
      Serial flasher config --> Flash size & 'idf.py monitor' baud rate  
      Partition Table --> Partition Table  
      Example Provisioning Configuration --> Provisioning Transport(Soft AP)  
      Example MQTT & HTTPS Server Configuration  
   $ idf.py build  
  
   $ idf.py erase-flash  
   $ idf.py -p /dev/ttyUSB0 flash monitor  
  
## Demo:  
![iot-example-demo](esp32_iot_example.gif)  
  
  
## Others  
   Example adding conditional configuration, refer to [here](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#adding-conditional-configuration)  
