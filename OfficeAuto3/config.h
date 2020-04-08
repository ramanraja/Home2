//config.h
#ifndef CONFIG_H
#define CONFIG_H
 
// Takes default values from keys.h (for sensitive data) and settings.h (for others)
// Then reads config.h file from the Flash and overwrites the defaults
#include "common.h"
#include "settings.h"
#include "Downloader.h"
#include "utilities.h"
#include "keys.h"
#include "FS.h"
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>     // https://github.com/bblanchon/ArduinoJson
 
class Config {
public :
// constants from settings.h
short  current_firmware_version =  FIRMWARE_VERSION;  
short  current_certificate_version = CERTIFICATE_VERSION;

const char * app_name = APP_NAME;  // this is only for display on startup message
char  app_id [MAX_TINY_STRING_LENGTH];
char  org_id [MAX_TINY_STRING_LENGTH];     
char  group_id [MAX_TINY_STRING_LENGTH];   

int tick_interval = 100;  // millisec; less than ~50 will starve the network stack of CPU cycles
int sensor_interval = 60000;   // read sensors every 1 minute

//---------------- application specific -------------------------------
int check_interval = 10000;  // 10 sec; for occupancy based light controller only
int status_report_frequency = STATUS_FREQUENCEY;  // in minutes; usually 5 minutes
float auto_off_minutes = AUTO_OFF_TIME_MIN;  // the autonomous relay switches off after this time (can be fractional)
//---------------- end application specific ---------------------------

bool version_check_enabled = true;
// holds primary and secondary .bin and .txt files etc, one at a time. it is REUSED ! Consume as soon as you generate it.
char reusable_string [MAX_LONG_STRING_LENGTH];   

// The following should NOT have an ending slash '/' as it will be dynamicaly added
char firmware_primary_prefix [MAX_LONG_STRING_LENGTH];       
char firmware_secondary_prefix [MAX_LONG_STRING_LENGTH];     
char certificate_primary_prefix [MAX_LONG_STRING_LENGTH];     
char certificate_secondary_prefix [MAX_LONG_STRING_LENGTH];   

// MQTT 
// bool  generate_random_client_id = true;   // client id MUST be unique to avoid 'repeated 8266 restart' issue !
// .. but this is now solved by appending the MAC ID as part of the client id

const char*  mqtt_client_prefix = org_id;  // this MUST  be <= 10 characters; the MAC address will be appended to this
char  mqtt_broadcast_topic[MAX_SHORT_STRING_LENGTH];  // fully qualified topic paths
char  mqtt_sub_topic[MAX_SHORT_STRING_LENGTH];
char  mqtt_pub_topic[MAX_SHORT_STRING_LENGTH];
char  mac_address[MAC_ADDRESS_LENGTH];  // 12+1 bytes needed to hold 6 bytes in HEX 
char  night_hours_str[MAX_TINY_STRING_LENGTH];  // for display 

bool active_low = ACTIVE_LOW_RELAY;
bool ON  = !active_low;  
bool OFF = active_low;

short  primary_relay = PRIMARY_RELAY;   // the autonomous relay, which is triggered by movement, time of the day etc.
bool   radar_triggers = RADAR_TRIGGERS;  // to transition from unoccupied to occupied status, should radar also fire ? (0=only PIR; 1=both radar & PIR need to fire)
short  night_start_hour = NIGHT_START_HOUR;   // time based automatic lights; hour and minute in 24 hour format          
short  night_end_hour = NIGHT_END_HOUR;        
short  night_start_minute = NIGHT_START_MINUTE;          
short  night_end_minute = NIGHT_END_MINUTE;       
int    day_light_threshold = DAY_LIGHT_THRESHOLD;
int    night_light_threshold = NIGHT_LIGHT_THRESHOLD;

// The leading slash in the following file names is necessary: 
// they are not included in the PREFIX strings in order to simplify string manipulations
// NOTE: config.txt must be listed first; it is referred to by the index 0 in Downloader.cpp
const char* file_names[NUM_CERTIFICATE_FILES] = {
        "/config.txt",  
        "/ca.der",
        "/cert.der",
        "/private.der"
    };
    
Config();
bool  init();
int   load_config();
void  dump();
const char* get_error_message (int error_code);

void make_derived_params();
int get_auto_off_ticks(); // number of 10-sec blocks after which the autonomous light will go off
//const char* get_ap_ssid();  // soft AP name for wifi manager
const char* get_primary_OTA_url();
const char* get_primary_version_url();
const char* get_secondary_OTA_url();
const char* get_secondary_version_url();
const char* get_primary_certificate_version_url();
const char* get_secondary_certificate_version_url();
const char* get_primary_certificate_url (short file_number);
const char* get_secondary_certificate_url (short file_number);
short get_num_files(); // number of certificate files, usually 4
int download_certificates();  // this is called from command handler through MQTT
};  
#endif 
 
