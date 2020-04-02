//config.h
#ifndef CONFIG_H
#define CONFIG_H
 
// Takes default values from keys.h (for sensitive data) and settings.h (for others)
// Then reads config.h file from the Flash and overwrites the defaults
#include "common.h"
#include "settings.h"
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

char  app_id [MAX_TINY_STRING_LENGTH];
bool version_check_enabled = true;

char reusable_string [MAX_LONG_STRING_LENGTH];   // holds primary and secondary .bin and .txt files etc, one at a time. REUSED !
char firmware_primary_prefix [MAX_LONG_STRING_LENGTH];       // must end in '/'
char firmware_secondary_prefix [MAX_LONG_STRING_LENGTH];     // must end in '/'
char certificate_primary_prefix [MAX_LONG_STRING_LENGTH];    // must end in '/'
char certificate_secondary_prefix [MAX_LONG_STRING_LENGTH];  // must end in '/'

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
void init();
bool load_config();
void dump();
/////void make_derived_params();
const char* get_error_message (short error_code);
short get_num_files();  // number of certificate & config files to download from cloud
const char* get_primary_certificate_version_url();
const char* get_secondary_certificate_version_url();
const char* get_primary_certificate_url (short file_number);
const char* get_secondary_certificate_url (short file_number);
const char* get_primary_OTA_url();
const char* get_primary_version_url();
const char* get_secondary_OTA_url();
const char* get_secondary_version_url();
};  
#endif 
 
