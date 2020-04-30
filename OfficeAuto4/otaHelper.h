// otaHelper.h
// OTA files on S3 are of the form:
// https://my-bucket.s3.us-east-2.amazonaws.com/my-folder/my-file.jpg

#ifndef OTAHELPER_H
#define OTAHELPER_H

// enable the following line to receive MQTT status during FW update; comment out to disable messages
#define MQTT_ENABLED

#include "common.h"
#include "config.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#ifdef MQTT_ENABLED
  #include <PubSubClient.h>   // https://github.com/knolleary/pubsubclient 
#endif

class OtaHelper {
 public:
    OtaHelper();
    void init(Config *configptr);    
#ifdef MQTT_ENABLED
    void init(Config *configptr, PubSubClient *mqttptr);
#endif
    int check_and_update();
    int check_version();    
    int update_firmware();  
 private:
     Config *pC;
     bool use_backup_urls = false; // this is a global flag used throughout this class
#ifdef MQTT_ENABLED     
     PubSubClient *pM;
#endif     
};

#endif 
