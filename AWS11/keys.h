// keys.h
// Keep this file secret: NEVER upload it to any code repository like Github !
// OTA files on S3 are of the form:
// https://my-bucket.s3.us-east-2.amazonaws.com/my-folder/my-file.jpg

#ifndef KEYS_H
#define KEYS_H

#define  WIFI_SSID1           "xxxx"
#define  WIFI_PASSWORD1       "xxxx"
#define  WIFI_SSID2           "xxxx"
#define  WIFI_PASSWORD2       "xxxx"


// HTTPS does not work ! to be debugged; use plain HTTP for now
#define  FW_SERVER_URL        "http://xxx-bucket.s3.us-east-2.amazonaws.com/portico.bin"
#define  FW_VERSION_URL       "http://xxx-bucket.s3.us-east-2.amazonaws.com/portico.txt"


//AWS MQTT broker address for your device
#define AWS_ENDPOINT          "xxxxxxx-ats.iot.us-east-2.amazonaws.com"
#define MQTT_PORT             8883

#define  MQTT_SUB_TOPIC_PREFIX     "my/topic/command"
#define  MQTT_PUB_TOPIC_PREFIX     "my/topic/status"

#endif 
