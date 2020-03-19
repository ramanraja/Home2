//config.h
#ifndef CONFIG_H
#define CONFIG_H
 
#include "common.h"
#include "settings.h"
#include "keys.h"
#include <ESP8266WiFi.h>
 
#define  UNIVERSAL_GROUP_ID     "0"   // TODO: use this to listen for pan-group messages
#define  UNIVERSAL_DEVICE_ID    "0"
#define  BAUD_RATE              115200 

class Config {
public :
// constants from settings.h
int  current_firmware_version =  FIRMWARE_VERSION;  
const char * app_name = APP_NAME;
const char * app_id =  APP_ID;
const char * org_id =  ORG_ID;     
const char * group_id = GROUP_ID;   
int status_report_frequency = STATUS_REPORT_FREQUENCEY;  // in minutes; usually 5 minutes

int tick_interval = 100;  // millisec; less than ~50 will starve the network stack of CPU cycles
int sensor_interval = 60000;   // read sensors every 1 minute

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

short  night_start_hour = NIGHT_START_HOUR;          
short  night_end_hour = NIGHT_END_HOUR;        
short  night_start_minute = NIGHT_START_MINUTE;          
short  night_end_minute = NIGHT_END_MINUTE;       

Config();
void init();
void dump();
};  
#endif 
 
