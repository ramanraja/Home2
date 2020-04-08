// keys.h
// Keep this file secret: NEVER upload it to any code repository like Github !

#ifndef KEYS_H
#define KEYS_H

// When the backup AP is also not accessible, start ESP8266 in AP mode and start a config web server:
// This is facilitated by the Wifi Manager
#define  SOFT_AP_SSID_PREFIX       "your-ssid"
#define  SOFT_AP_PASSWD            "your-password"

// OTA binary and version files on S3 are of the form:
// https://my-bucket.s3.us-east-2.amazonaws.com/my-folder/my-file.bin
// https://my-bucket.s3.us-east-2.amazonaws.com/my-folder/my-file.txt
// HTTPS does not work ! add finger print; use plain HTTP for now
#define  FW_PRIMARY_PREFIX          "http://xxxx-bucket.s3.us-east-2.amazonaws.com/ota2/"
#define  FW_BACKUP_PREFIX          "http://192.168.0.101:8000/ota/"
#define  CERTIFICATE_PRIMARY_PREFIX   "http://xxxxx-bucket.s3.us-east-2.amazonaws.com/cert1"
#define  CERTIFICATE_BACKUP_PREFIX    "http://192.168.0.101:8000/ota/cert" 

//AWS MQTT broker address for your device
#define  AWS_END_POINT             "xxxx-ats.iot.us-east-2.amazonaws.com"
#define  MQTT_PORT                 8883
#define  SUB_TOPIC_PREFIX          "cmd"
#define  PUB_TOPIC_PREFIX          "status"

#endif 
