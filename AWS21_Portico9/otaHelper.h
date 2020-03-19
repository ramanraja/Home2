// otaHelper.h
// OTA files on S3 are of the form:
// https://my-bucket.s3.us-east-2.amazonaws.com/my-folder/my-file.jpg

#ifndef OTAHELPER_H
#define OTAHELPER_H

#include "common.h"
#include "config.h"
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>   // https://github.com/knolleary/pubsubclient 

class OtaHelper {
 public:
    OtaHelper();
    void init(Config *configptr, PubSubClient *mqttptr);
    int check_and_update();
    bool check_version();    
    int update_firmware();  
 private:
     Config *pC;
     PubSubClient *pM;
};

#endif 
