This folder will have all the information regarding the cloud cert

Refer to : https://thingsboard.io/docs/samples/esp32/ota

If you are going to use some another server for the firmware images delivering, then content of ca_cert.pem file should be replaced.
To get public SSL certificate of a server, execute the next command (preliminary replace raw.githubusercontent.com by the desired server address)

$ openssl s_client -showcerts -connect raw.githubusercontent.com:443  
  
  
# ThingsBoard:  
## [HTTP Transport over SSL](https://thingsboard.io/docs/user-guide/ssl/http-transport-over-ssl/) 
Run openssl s_client command & paste to ca_cert.pem  
  
## [MQTT over SSL(one-way/two-way SSL connection)](https://thingsboard.io/docs/user-guide/mqtt-over-ssl/#ssl-configuration-using-pem-certificates-file):  
Replace your server mqtt over ssl file to tb_mqtt_server_cert.pem  

