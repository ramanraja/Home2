Basic AWS IoT client: this is a bare bones version
The code is experimental, and needs extensive refactoring. But it works.

#define  FIRMWARE_VERSION   1

The relays are connected to D1 and D2 (GPIO 5 and GPIO 4)
Alexa integration done (Porch lamp and Portico lamp - custom skills with my own lambda)

Before using a new NodeMCU chip, upload the CA certificates using Tools/ESP8266 Sketch Data Upload plugin.

OTA is from AWS S3. (portico.bin and portico.txt)
*** NOTE: After uploading every time, select the files and make them public ***

// use HTTP for now, HTTPS does not work yet (finger print?):
#define  FW_SERVER_URL        "http://my-bucket.s3.us-east-2.amazonaws.com/portico.bin"
#define  FW_VERSION_URL       "http://my-bucket.s3.us-east-2.amazonaws.com/portico.txt"

Group ID and device ID are now part of the MQTT topics:
#define  MQTT_SUB_TOPIC     "my/topic/command/1/1";
#define  MQTT_PUB_TOPIC     "my/topic/status/1/1";

--------------------------------------------------------------------------

   ESP8266 AWS IoT example by Evandro Luis Copercini
   Code based on:
   https://github.com/copercini/esp8266-aws_iot/blob/master/examples/mqtt_x509_DER/mqtt_x509_DER.ino

   Dependencies:
   https://github.com/esp8266/arduino-esp8266fs-plugin
   https://github.com/arduino-libraries/NTPClient
   https://github.com/knolleary/pubsubclient 

   Create a new device in AWS IoT,  
   give it AWS IoT permissions and 
   download its certificates (in pem format).
   OTA files can be accessed from S3 now. They are of the form:
   https://your-bucket.s3.us-east-2.amazonaws.com/my-folder/my-file.jpg

  Use OpenSSL (install it for Windows) to convert the certificates to DER format:
  > openssl x509 -in portico_thing.cert.pem    -out  cert.der    -outform  DER
  > openssl rsa -in  portico_thing.private.key -out  private.der -outform  DER
  > openssl x509 -in root-CA.crt  -out  ca.der  -outform  DER
  
  copy the der files to a data' folder under your project and upload it to SPIFFS using tools/"ESP8266 Sketch Data Upload"

  To upload to SPIFF, use arduino-esp8266fs-plugin:
  https://github.com/esp8266/arduino-esp8266fs-plugin/releases/tag/0.5.0
  and put the jar file in
  C:\Users\yourname\Documents\Arduino\tools\ESP8266FS\tool\esp8266fs.jar
  Upload the 3 certificates in /data folder to SPIFF using tools/"ESP8266 Sketch Data Upload" menu

  To test if your device's certificates are working and you can connect to AWS through SSL:
> openssl s_client -connect xxxxxx-ats.iot.us-east-2.amazonaws.com:8443   -CAfile root-CA.crt 
  -cert portico_thing.cert.pem   -key portico_thing.private.key

  *** NOTE: 8266 subscriber is unable to receive the long MQTT messages from AWS. Use short custom messages ***


