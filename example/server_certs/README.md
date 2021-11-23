This folder will have all the information regarding the cloud cert

Refer to : https://thingsboard.io/docs/samples/esp32/ota

If you are going to use some another server for the firmware images delivering, then content of ca_cert.pem file should be replaced.
To get public SSL certificate of a server, execute the next command (preliminary replace raw.githubusercontent.com by the desired server address)

$ openssl s_client -showcerts -connect raw.githubusercontent.com:443