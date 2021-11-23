ESP32 With Thingsboard  

# ESP-IDF Training  
Repository showing "how-to-build" fully functional IoT product with ESP-IDF framework.  
  
# Wi-Fi Provisioning Manager Example  
Add Wi-Fi Provisioning Manager,  
Refer to : https://github.com/espressif/esp-idf/tree/master/examples/provisioning/wifi_prov_mgr  
  
# Thingsboard Cloud  
Refer to : https://thingsboard.io/docs/samples/esp32/ota  
  
# How to use:  
$ git clone git@github.com:yungwenpeng/esp32-iot-example.git  
$ cd esp32-iot-example/example  
$ idf.py menuconfig  
$ idf.py build  
  
$ idf.py erase-flash  
$ idf.py -p /dev/ttyUSB0 flash monitor  
  
Demo:  
![iot-example-demo](esp32_iot_example.gif)  
  
  
# Example adding conditional configuration 
Refer to : https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/build-system.html#adding-conditional-configuration  
