This is the seed program to bootstrap a new ESP chip/device:
The sole purpose of this program is to: 
 - get Wifi credentials, 
 - update software through OTA and 
 - download TLS security certificates, 
all without much human input (except the wifi credentials).

Ideally, the new chip has to be initialized through serial port with the TLS ceritificates and the config.txt file through 8266 data upload feature in Aruduino IDE.

Where this is not feasible: 
 - download this boot program binary from AWS.
 - upload it to the NodeMCU using PyFlasher tool. 
 - It will do the rest of the setup, and download the latest firmware of your application program.

Config:
Opens a config file from SPIFF and displays/stores the values. If the file is not found, it has hard coded default values for all.


Important note:
A typical config.txt file will be as follows:
{ 
"OTA1":"http://my-bucket.s3.us-east-2.amazonaws.com/ota",
"OTA2":"http://192.168.0.101:8000/ota",
"CERT1":"http://my-bucket.s3.us-east-2.amazonaws.com/cert",
"CERT2":"http://192.168.0.101:8000/ota/cert" 
}
Note that there is no slash at the end of the URLs. This will be added programmatically.
This is done in order to avoid String operations with dynamic memory allocation.

Have a file certversion.txt along with the certificate files; increase its version number for
every new firmware release. The certificates and config.txt files will be downloaded only if the
certificate version is greater than the current FW version in the application.

Wifi:
Tries to connect to WiFi with already saved credentials (using ESP auto connect).
If not possible, starts the Wifi Manager portal.
If credentials are not input within a time out (3 minutes), we restart the ESP.

Certificates:
It checks for TLS security files. If not present, it downloads the files from AWS cloud. -> this address is configurable through config.txt. (It just stores the files, and does not try to load it to any TLS client).

OTA:
The primary and backup OTA address is configurable through config.txt.
Then it checks for OTA updates from AWS through HTTP. (no need for TLS certificates for this). If a new version is available, installs it, restarts ESP.
If the version check file on AWS is not available, tries to download it from a known local IP. If a new verion is available, install it, restarts ESP.

This is a boot process stub that does just this much.No business logic is implemented.In particular, it does not try to connect to AWS/MQTT server.
-----------------------------------------------------------------------
   TLS certificates:

   Create a new device in AWS IoT,  
   give it AWS IoT permissions and 
   download its certificates (in pem format).
   Certificate files can be put on AWS S3 or on a local python web server. They are of the form:
   https://my-bucket.s3.us-east-2.amazonaws.com/my-folder/my-file.jpg
  Use OpenSSL (install it for Windows) to convert the certificates to DER format:
  > openssl x509 -in portico_thing.cert.pem    -out  cert.der    -outform  DER
  > openssl rsa -in  portico_thing.private.key -out  private.der -outform  DER
  > openssl x509 -in root-CA.crt  -out  ca.der  -outform  DER
  copy the der files to a data' folder under your project and upload it to SPIFFS using tools/"ESP8266 Sketch Data Upload"
  To upload to SPIFF, use arduino-esp8266fs-plugin:
  https://github.com/esp8266/arduino-esp8266fs-plugin/releases/tag/0.5.0
  and put the jar file in
  C:\Users\you\Documents\Arduino\tools\ESP8266FS\tool\esp8266fs.jar
  http://arduino.esp8266.com/Arduino/versions/2.0.0/doc/filesystem.html
  
  Upload the 3 certificates in /data folder to SPIFF using tools/"ESP8266 Sketch Data Upload" menu
  To test if your device's certificates are working and you can connect to AWS through SSL:
> openssl s_client -connect xxxxx-ats.iot.us-east-2.amazonaws.com:8443   -CAfile root-CA.crt 
  -cert portico_thing.cert.pem   -key portico_thing.private.key

Put the config.txt file also along with the certificates in the same folder. They will be downloaded
together.
In addition, have a version file certversion.txt in the same folder with the latest version number
of the certificate files. (This is different from the OTA firmware version file, which resides in a
different folder along with the firmware).
