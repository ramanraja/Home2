// keys.h
// Keep this file secret: NEVER upload it to any code repository like Github !

#ifndef KEYS_H
#define KEYS_H

// When the backup AP is also not accessible, start ESP8266 in AP mode and start a config web server:
// This is facilitated by the Wifi Manager
#define  SOFT_AP_SSID              "your_AP"
#define  SOFT_AP_PASSWD            "your_password"

// OTA binary and version files on S3 are of the form:
// https://my-bucket.s3.us-east-2.amazonaws.com/my-folder/my-file.bin
// https://my-bucket.s3.us-east-2.amazonaws.com/my-folder/my-file.txt
// HTTPS does not work ! add finger print; use plain HTTP for now

// NOTE: The following prefixes do not end in '/' to simplify string manipulation     
#define  FW_PRIMARY_PREFIX            "http://my-bucket.s3.us-east-2.amazonaws.com/ota"    
#define  FW_BACKUP_PREFIX             "http://192.168.0.101:8000/ota"                        
#define  CERTIFICATE_PRIMARY_PREFIX   "http://my-bucket.s3.us-east-2.amazonaws.com/cert"
#define  CERTIFICATE_BACKUP_PREFIX    "http://192.168.0.101:8000/ota/cert" 
#endif 
