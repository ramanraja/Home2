// keys.h
// Keep this file secret: NEVER upload it to any code repository like Github !
// OTA files on S3 are of the form:
// https://my-bucket.s3.us-east-2.amazonaws.com/my-folder/my-file.jpg

#ifndef KEYS_H
#define KEYS_H

#define  SOFT_AP_SSID         "xxxx_AP"
#define  SOFT_AP_PASSWD       "xxxx"

// OTA files on S3 are of the form:
// https://my-bucket.s3.us-east-2.amazonaws.com/my-folder/my-file.jpg
// HTTPS does not work ! to be debugged; use plain HTTP for now
#define  FW_SERVER_URL        "http://my-bucket.s3.us-east-2.amazonaws.com/ota2/port.bin"
#define  FW_VERSION_URL       "http://my-bucket.s3.us-east-2.amazonaws.com/ota2/port.txt"

 

//AWS MQTT broker address for your device
#define  AWS_END_POINT             "xxxxxx-ats.iot.us-east-2.amazonaws.com"
#define  MQTT_PORT                 8883
#define  SUB_TOPIC_PREFIX          "cmd"
#define  PUB_TOPIC_PREFIX          "status"

#endif 
