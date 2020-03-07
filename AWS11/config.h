//config.h
#ifndef CONFIG_H
#define CONFIG_H
 
#include "common.h"
#include "keys.h"

// increment the firmwver version number for every version
#define  FIRMWARE_VERSION       1

// update these unique for every new device
#define  GROUP_ID               1  
#define  DEVICE_ID              1

#define  BAUD_RATE              115200 

class Config {
public :
int  current_firmware_version =  FIRMWARE_VERSION;  
//bool sleep_deep = true;

char firmware_server_url [MAX_LONG_STRING_LENGTH];
char version_check_url [MAX_LONG_STRING_LENGTH];
bool verison_check_enabled = true;

/* The following constants should be updated in  "keys.h" file  */
const char *wifi_ssid1        = WIFI_SSID1;
const char *wifi_password1    = WIFI_PASSWORD1;
const char *wifi_ssid2        = WIFI_SSID2;
const char *wifi_password2    = WIFI_PASSWORD2;

//MQTT 
bool  generate_random_client_id = true;   // this must always be true in order to 
// avoid repeated 8266 restart issue !
const char*  mqtt_client_prefix = "myprefix_";
char  mqtt_sub_topic[MAX_SHORT_STRING_LENGTH];
char  mqtt_pub_topic[MAX_SHORT_STRING_LENGTH];

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
 
