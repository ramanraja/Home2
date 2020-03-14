//config.h
#ifndef CONFIG_H
#define CONFIG_H
 
#include "common.h"
#include "keys.h"
#include <ESP8266WiFi.h>

// increment the firmware version for every revision
#define  FIRMWARE_VERSION       9

#define  UNIVERSAL_GROUP_ID     "0"
#define  UNIVERSAL_DEVICE_ID    "0"
#define  BAUD_RATE              115200 

class Config {
public :
int  current_firmware_version =  FIRMWARE_VERSION;  
const char * app_name = "Port Controller";
const char * app_id =  "port";
const char * org_id =  "inorg";
const char * group_id = "Group1"; 

//bool sleep_deep = true;

char firmware_server_url [MAX_LONG_STRING_LENGTH];
char version_check_url [MAX_LONG_STRING_LENGTH];
bool verison_check_enabled = true;


//MQTT 
// bool  generate_random_client_id = true;   // client id MUST be unique to avoid repeated 8266 restart issue !
// .. but this is now solved by appending the MAC ID as part of the client id
const char*  mqtt_client_prefix = org_id;  // this MUST  be <= 10 characters; the MAC address will be appended to this
char  mqtt_broadcast_topic[MAX_SHORT_STRING_LENGTH];
char  mqtt_sub_topic[MAX_SHORT_STRING_LENGTH];
char  mqtt_pub_topic[MAX_SHORT_STRING_LENGTH];
char  mac_address[MAC_ADDRESS_LENGTH];

// 6 PM to 6.30 AM is considered night (for winter)
int  NIGHT_START_HOUR = 18;   
int  NIGHT_END_HOUR = 6;
int  NIGHT_START_MINUTE = 0;      
int  NIGHT_END_MINUTE = 30;

Config();
void init();
void dump();
};  
#endif 
 
