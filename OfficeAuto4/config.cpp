//config.cpp
#include "config.h"

/* Example topics:
 publish:   intof/portico/status/G1/2CF432173BC0
 subscribe: intof/portico/cmd/G1/2CF432173BC0
 subscribe: intof/bath/cmd/G1/0
 */

Config::Config(){
}

// returns false if TLS certificates are missing; true in all other scenarios 
bool Config::init() {
    byte mac[6];
    WiFi.macAddress(mac);
    snprintf (mac_address, MAC_ADDRESS_LENGTH-1, "%2X%2X%2X%2X%2X%2X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    mac_address[MAC_ADDRESS_LENGTH-1]='\0';   // TODO: create a utility wrapper safe_snprintf()  
        
    // the string variables are to be copied; (other basic types are already initialized in config.h) 
    safe_strncpy (app_id, APP_ID, MAX_TINY_STRING_LENGTH);
    safe_strncpy (org_id, ORG_ID, MAX_TINY_STRING_LENGTH);
    safe_strncpy (group_id, GROUP_ID, MAX_TINY_STRING_LENGTH);    
    // the firmware URLs will later be concatenated with the intervening slash; so remove them if present
    safe_strncpy_remove_slash (firmware_primary_prefix,  FW_PRIMARY_PREFIX, MAX_LONG_STRING_LENGTH);
    safe_strncpy_remove_slash (firmware_secondary_prefix, FW_BACKUP_PREFIX, MAX_LONG_STRING_LENGTH);    
    // the Flash file names already have a mandatory slash at the beginning; so remove them from the prefix
    safe_strncpy_remove_slash (certificate_primary_prefix,  CERTIFICATE_PRIMARY_PREFIX, MAX_LONG_STRING_LENGTH);
    safe_strncpy_remove_slash (certificate_secondary_prefix, CERTIFICATE_BACKUP_PREFIX, MAX_LONG_STRING_LENGTH);  
        
    // now that the main parameters are in place, set up the derived parameters:
    make_derived_params();
    SERIAL_PRINTLN(F("Raw configuration:"));
    dump();
    int result = load_config();  // read overriding params from Flash file    
    SERIAL_PRINT(F("SPIFF configuration load result: "));
    SERIAL_PRINTLN(get_error_message(result));
    if (result==SPIFF_FAILED || result==TLS_CERTIFICATE_FAILED)  
        return false;
    return true;
}

// parameters that depend on other parameters initialized earlier
void Config::make_derived_params() {
    ON  = active_low ? 0 : 1;
    OFF = active_low ? 1 : 0;
    snprintf(night_hours_str, MAX_TINY_STRING_LENGTH-1, "%d:%d - %d:%d", night_start_hour, night_start_minute,
             night_end_hour,night_end_minute);
             
    snprintf (mqtt_pub_topic, MAX_SHORT_STRING_LENGTH-1, "%s/%s/%s/%s/%s", org_id, app_id, PUB_TOPIC_PREFIX, group_id, mac_address); 
    snprintf (mqtt_sub_topic, MAX_SHORT_STRING_LENGTH-1, "%s/%s/%s/%s/%s", org_id, app_id, SUB_TOPIC_PREFIX, group_id, mac_address);
    snprintf (mqtt_broadcast_topic, MAX_SHORT_STRING_LENGTH-1, "%s/%s/%s/%s/%s", org_id, app_id, SUB_TOPIC_PREFIX, group_id,
              UNIVERSAL_DEVICE_ID);    
}

int Config::load_config() {
    SERIAL_PRINTLN(F("Loading config from Flash..."));
    if (!SPIFFS.begin()) {
        SERIAL_PRINTLN(F("--- Failed to mount file system. ---"));
        ///SPIFFS.end();
        return SPIFF_FAILED;
    }
    // quick check if security certificates are at least present
    SERIAL_PRINTLN(F("Checking TLS certificate files..."));
    for (int i=1; i<NUM_CERTIFICATE_FILES; i++) {  // the first file is config.txt; it is ok if that file is missing
        SERIAL_PRINTLN (file_names[i]);
        if (!SPIFFS.exists(file_names[i])) {
            SERIAL_PRINTLN(F("*** TLS certificate file missing ! ***"));
            SPIFFS.end();
            return TLS_CERTIFICATE_FAILED;
        }
    }
    SERIAL_PRINTLN(F("TLS certificate files present."));
    SERIAL_PRINTLN(F("Checking Config file..."));
    File configFile = SPIFFS.open(CONFIG_FILE_NAME, "r");
    if (!configFile) {
        SERIAL_PRINTLN(F("--- Failed to open config file. ---"));
        SPIFFS.end();
        return FILE_OPEN_ERROR;
    }
    size_t size = configFile.size();
    SERIAL_PRINT(F("Config file present. Size: "));
    SERIAL_PRINTLN(size);    
    if (size > CONFIG_FILE_SIZE) {
        SERIAL_PRINTLN(F("--- Config file size is too large. ---"));
        SPIFFS.end();
        return FILE_TOO_LARGE;
    }
    // Allocate a buffer to store file contents ***
    // ArduinoJson library requires the input buffer to be mutable     
    std::unique_ptr<char[]> buf(new char[size]);
    configFile.readBytes(buf.get(), size);
    
    // Consider this number carefully; json needs extra space for metadata:
    StaticJsonDocument<JSON_CONFIG_FILE_SIZE> doc;  // including extra space for Json
    auto error = deserializeJson(doc, buf.get());
    SERIAL_PRINT(F("Deserialization result: "));
    SERIAL_PRINTLN (error.c_str());
    if (error) {
        SERIAL_PRINTLN(F("--- Failed to parse config file. ---"));
        SPIFFS.end();
        return JSON_PARSE_ERROR;
    }
    // string variables
    char *tmp;
    tmp = (char *)(doc["OTA1"] | FW_PRIMARY_PREFIX); 
    safe_strncpy_remove_slash (firmware_primary_prefix, tmp, MAX_LONG_STRING_LENGTH);
    tmp = (char *)(doc["OTA2"] | FW_BACKUP_PREFIX); 
    safe_strncpy_remove_slash (firmware_secondary_prefix, tmp, MAX_LONG_STRING_LENGTH);   
    tmp = (char *)(doc["CERT1"] | CERTIFICATE_PRIMARY_PREFIX); 
    safe_strncpy_remove_slash (certificate_primary_prefix, tmp, MAX_LONG_STRING_LENGTH);
    tmp = (char *)(doc["CERT2"] | CERTIFICATE_BACKUP_PREFIX); 
    safe_strncpy_remove_slash (certificate_secondary_prefix, tmp, MAX_LONG_STRING_LENGTH); 
    
    tmp = (char *)(doc["GRP"] | GROUP_ID); 
    safe_strncpy (group_id, tmp, MAX_TINY_STRING_LENGTH);
    tmp = (char *)(doc["ORG"] | ORG_ID); 
    safe_strncpy (org_id, tmp, MAX_TINY_STRING_LENGTH);
    tmp = (char *)(doc["APP"] | APP_ID); 
    safe_strncpy (app_id, tmp, MAX_TINY_STRING_LENGTH); 
    
    // Non-string variables       
    current_certificate_version = doc["CERT_VER"] | CERTIFICATE_VERSION;
    active_low = doc["ACTIVE_LOW"] | ACTIVE_LOW_RELAY; 
    primary_relay = doc["PRIMARY_REL"] | PRIMARY_RELAY; 
    radar_triggers = doc["RADAR_TRIG"] | RADAR_TRIGGERS; 
    status_report_frequency = doc["STAT_FREQ_MIN"] | STATUS_FREQUENCEY; 
    auto_off_minutes = doc["AUTO_OFF_MIN"] | AUTO_OFF_TIME_MIN;   
    night_start_hour = doc["NIGHT_HRS"][0] | NIGHT_START_HOUR; 
    night_start_minute = doc["NIGHT_HRS"][1] | NIGHT_START_MINUTE; 
    night_end_hour = doc["NIGHT_HRS"][2] | NIGHT_END_HOUR; 
    night_end_minute = doc["NIGHT_HRS"][3] | NIGHT_END_MINUTE; 
    day_light_threshold = doc["DAY_LIGHT"] | DAY_LIGHT_THRESHOLD;
    night_light_threshold = doc["NIGHT_LIGHT"] | NIGHT_LIGHT_THRESHOLD;   
     
    // now that the main parameters are in place, set up the derived parameters:
    make_derived_params();
    
    SPIFFS.end();  // unmount file system
    return CODE_OK;
}

void Config::dump() {
#ifdef ENABLE_DEBUG
    SERIAL_PRINTLN(F("\n-----------------------------------------"));
    SERIAL_PRINTLN(F("               configuration             "));
    SERIAL_PRINTLN(F("-----------------------------------------"));    
    print_heap();
    SERIAL_PRINT(F("app name: "));
    SERIAL_PRINTLN (app_name);    
    SERIAL_PRINT(F("Firmware version: "));
    SERIAL_PRINTLN (current_firmware_version);       
    SERIAL_PRINT(F("TLS certificate version: "));
    SERIAL_PRINTLN (current_certificate_version);       
    SERIAL_PRINTLN();        
    SERIAL_PRINT(F("primary OTA server: "));
    SERIAL_PRINTLN (get_primary_OTA_url());   
    SERIAL_PRINT(F("primary version: "));
    SERIAL_PRINTLN (get_primary_version_url());   
    SERIAL_PRINT(F("secondary OTA server: "));
    SERIAL_PRINTLN (get_secondary_OTA_url());   
    SERIAL_PRINT(F("secondary version: "));
    SERIAL_PRINTLN (get_secondary_version_url());       
    
    SERIAL_PRINTLN(F("primary certificate version check file: "));
    SERIAL_PRINTLN (get_primary_certificate_version_url());  
    SERIAL_PRINTLN(F("secondary certificate version check  file: "));
    SERIAL_PRINTLN (get_secondary_certificate_version_url());  
    SERIAL_PRINTLN(F("primary certificate files: "));   
    for (int i=0; i<NUM_CERTIFICATE_FILES; i++) 
        SERIAL_PRINTLN (get_primary_certificate_url(i));   
    SERIAL_PRINTLN(F("secondary certificate files: "));
    for (int i=0; i<NUM_CERTIFICATE_FILES; i++)     
        SERIAL_PRINTLN (get_secondary_certificate_url(i));  
            
    SERIAL_PRINT(F("MQTT server: "));
    SERIAL_PRINTLN(AWS_END_POINT); 
    SERIAL_PRINT(F("MQTT Port: "));
    SERIAL_PRINTLN(MQTT_PORT); 
    SERIAL_PRINTLN();
    SERIAL_PRINT(F("org id: "));
    SERIAL_PRINTLN (org_id);   
    SERIAL_PRINT(F("app id: "));
    SERIAL_PRINTLN (app_id);      
    SERIAL_PRINT(F("group id: "));
    SERIAL_PRINTLN (group_id);   
    SERIAL_PRINT(F("MAC address: "));    
    SERIAL_PRINTLN (mac_address);    
//    SERIAL_PRINT(F("Soft AP SSID: "));
//    SERIAL_PRINTLN (get_ap_ssid());  
    SERIAL_PRINT(F("Publish topic: "));
    SERIAL_PRINTLN (mqtt_pub_topic); 
    SERIAL_PRINT(F("Subscribe topic: "));
    SERIAL_PRINTLN (mqtt_sub_topic); 
    SERIAL_PRINT(F("Broadcast topic (sub): "));
    SERIAL_PRINTLN (mqtt_broadcast_topic);  
    SERIAL_PRINTLN();
    // the autonomous relay, which is triggered by movement, time of the day etc.
    SERIAL_PRINT(F("primary (autonomous) realy number: "));
    SERIAL_PRINTLN (primary_relay);   
    // some relay modules need active LOW signals, ie, logic 0 operates the relay and 1 releases it
    SERIAL_PRINT(F("active LOW relays ?: "));
    SERIAL_PRINTLN (active_low? "Yes":"No");          
    // to transition from unoccupied to occupied status, should radar also fire ? (0=only PIR; 1=both radar & PIR need to fire)
    SERIAL_PRINT(F("radar triggers occupancy?: "));
    SERIAL_PRINTLN (radar_triggers ? "Yes":"No");   
    // time based automatic lights; hour and minute in 24 hour format 
    SERIAL_PRINT(F("Night hours: "));
    SERIAL_PRINTLN (night_hours_str); 
    SERIAL_PRINT(F("Day/Night Light threshold: "));
    SERIAL_PRINT (day_light_threshold);
    SERIAL_PRINT(F(" / "));
    SERIAL_PRINTLN(night_light_threshold); 
    SERIAL_PRINT(F("status report frequency (minutes): "));
    SERIAL_PRINTLN (status_report_frequency);        
    SERIAL_PRINT(F("auto off minutes: "));
    SERIAL_PRINTLN (auto_off_minutes);      
    SERIAL_PRINT(F("auto off ticks: "));
    SERIAL_PRINTLN (get_auto_off_ticks());             
    yield();
    delay(1000);  // stabilize heap ?     
    print_heap();           
    SERIAL_PRINTLN(F("-----------------------------------------\n"));
#endif    
}

short Config::get_num_files() {
    return NUM_CERTIFICATE_FILES;
}

// converts auto_off_minutes into number of 10-second ticks
int Config::get_auto_off_ticks() {
    return (auto_off_minutes * 60 / (check_interval/1000));
}

// NOTE: reusable_string is a scratch pad string; it holds one URL at a time, to save memory!
// So don't call any of these functions while the previous string is still in use 
// add a random parameter to the URL to bypass the server side cache:
// https://stackoverflow.com/questions/50699554/my-esp8266-using-cached-how-to-fix

// NOTE: the following two functions introduce a slash between the prefix and the file name; so this should not
// be added in the prefix string (it would have been removed, if present)
const char* Config::get_primary_certificate_version_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s?X=%d", certificate_primary_prefix, CERT_VERSION_FILE, random(0,1000));  
    return ((const char*) reusable_string);
}

const char* Config::get_secondary_certificate_version_url(){
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s?X=%d", certificate_secondary_prefix, CERT_VERSION_FILE, random(0,1000));  
    return ((const char*) reusable_string);
}

// NOTE: the following two functions do not add a slash between the prefix and the file name, since the SPIFF file name
// already should have a mandatory leading slash.
const char* Config::get_primary_certificate_url (short file_number){
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s%s?X=%d", certificate_primary_prefix, file_names[file_number], random(0,1000));  
    return ((const char*) reusable_string);
}

const char* Config::get_secondary_certificate_url (short file_number){
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s%s?X=%d", certificate_secondary_prefix, file_names[file_number], random(0,1000));  
    return ((const char*) reusable_string);
}

// NOTE: these functions introduce the slash again;so  the prefix should not have it.
const char*  Config::get_primary_OTA_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s.bin?X=%d", firmware_primary_prefix, app_id, 
    random(0,1000));  
    return ((const char*) reusable_string);
}

const char*  Config::get_primary_version_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s.txt?X=%d", firmware_primary_prefix, app_id, 
    random(0,1000));
    return ((const char*) reusable_string);    
}

const char*  Config::get_secondary_OTA_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s.bin?X=%d", firmware_secondary_prefix, app_id, 
    random(0,1000));
    return ((const char*) reusable_string);    
}

const char*  Config::get_secondary_version_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s.txt?X=%d", firmware_secondary_prefix, app_id, 
    random(0,1000));
    return ((const char*) reusable_string);    
}

// this is called from command handler through MQTT
// If the download succeeds, the command handler will restart ESP
int Config::download_certificates() {
    SERIAL_PRINTLN(F("[Config] Downloading TLS certificates.."));  
    print_heap();
    Downloader D;
    D.init(this);
    D.list_files();
    int result = D.download_files();
    SERIAL_PRINT(F("[Config] download result: "));
    SERIAL_PRINTLN(get_error_message(result));
    print_heap();
    return (result);
}

const char* Config::get_error_message (int error_code) {
  switch (error_code) {
    case CODE_OK:
        return ("CODE_OK"); break;
    case PROCEED_TO_UPDATE:
        return ("PROCEED_TO_UPDATE"); break;
    case UPDATE_OK:
        return ("UPDATE_OK"); break;
    case BAD_URL:
        return ("BAD_URL"); break;
    case NO_WIFI:
        return ("NO_WIFI"); break;
    case HTTP_FAILED:
        return ("HTTP_FAILED"); break;
    case NOT_FOUND:
        return ("NOT_FOUND"); break;
    case NO_ACCESS:
        return ("NO_ACCESS"); break;
    case VERSION_CHECK_FAILED:
        return ("VERSION_CHECK_FAILED"); break;
    case UPDATE_FAILED:
        return ("UPDATE_FAILED"); break;
    case NO_UPDATES:
        return ("NO_UPDATES"); break;
    case SPIFF_FAILED:
        return ("SPIFF_FAILED"); break;
    case FILE_OPEN_ERROR:
        return ("FILE_OPEN_ERROR"); break;
    case FILE_WRITE_ERROR:
        return ("FILE_WRITE_ERROR"); break;
    case AWS_CONNECT_SUCCESS:
        return ("AWS_CONNECT_SUCCESS"); break;        
    case AWS_CONNECT_FAILED:
        return ("AWS_CONNECT_FAILED"); break;    
    case TLS_CERTIFICATE_FAILED:
        return ("TLS_CERTIFICATE_FAILED"); break;    
    case TIME_SERVER_FAILED:
        return ("TIME_SERVER_FAILED"); break;     
    case FILE_TOO_LARGE:
        return ("FILE_TOO_LARGE"); break;      
    case JSON_PARSE_ERROR:
        return ("JSON_PARSE_ERROR"); break;                               
    default:
        return("UNCONFIGURED ERROR !"); break;
  }
}

// Get commands are in the form {"G":"param"}
const char* Config::get_param (const char* param) {
    SERIAL_PRINT(F("Get: "));
    SERIAL_PRINTLN(param);
    // the following calls return the full URL, not just the prefix 
    if (strcmp(param, "OTAP") == 0)
        return (get_primary_OTA_url());
    if (strcmp(param, "OTAS") == 0)
        return (get_secondary_OTA_url());
    if (strcmp(param, "OTAPV") == 0)
        return (get_primary_version_url());
    if (strcmp(param, "OTASV") == 0)
        return (get_secondary_version_url());        
    if (strcmp(param, "CERTP") == 0)
        return (get_primary_certificate_url(0));
    if (strcmp(param, "CERTS") == 0)
        return (get_secondary_certificate_url(0));  
    if (strcmp(param, "CERTPV") == 0)
        return (get_primary_certificate_version_url());
    if (strcmp(param, "CERTSV") == 0)
        return (get_secondary_certificate_version_url());  
    if (strcmp(param, "MAC") == 0)  // this is just for completeness;
        return ((const char*)mac_address);  // this is availabe through {"C":"MAC"} also
    if (strcmp(param, "ORG") == 0)
        return ((const char*)org_id);
    if (strcmp(param, "GRP") == 0)
        return ((const char*)group_id);  
    if (strcmp(param, "APP") == 0)
        return ((const char*)app_id); 
    if (strcmp(param, "ACTL") == 0)
        return (active_low? "AL:1" : "AL:0");    
    if (strcmp(param, "RTRIG") == 0)      
        return (radar_triggers? "RT:1" : "RT:0");            
    if (strcmp(param, "PRIREL") == 0) {
        itoa (primary_relay, reusable_string, 10);
        return ((const char*)reusable_string);
    } 
    if (strcmp(param, "STATF") == 0) {
        itoa (status_report_frequency, reusable_string, 10);
        return ((const char*)reusable_string);
    }
    if (strcmp(param, "AOFF") == 0) {
        itoa (auto_off_minutes, reusable_string, 10);
        return ((const char*)reusable_string);
    }
    if (strcmp(param, "NHRS") == 0) 
        return ((const char*)night_hours_str);
    if (strcmp(param, "LTH") == 0) {
        snprintf(reusable_string, MAX_TINY_STRING_LENGTH, "Day:%d , Night:%d", day_light_threshold,night_light_threshold);
        return ((const char*)reusable_string);         
    } 
    SERIAL_PRINTLN (F("--- ERROR: Unknown parameter ---"));   
    snprintf(reusable_string, MAX_TINY_STRING_LENGTH, "ERROR");
    return ((const char*)reusable_string);  
}

// SET commands are in the form {"S":{"P":"param", "V":"value"}}
// To see the effect of a SET, issue a GET command subsequently
// * returns true if there was an error, false otherwise *
bool Config::set_param (const char* param, const char* value) {
    SERIAL_PRINT(F("Set: "));
    SERIAL_PRINTLN(param);
    SERIAL_PRINT(F("Value: ")); 
    SERIAL_PRINTLN(value);   
    bool truncated = false;
    // MAC address spoofing - use it only for testing purposes ! MAC is used in MQTT client ID
    // Warning: any duplicate MQTT client IDs will result in malfunctioning, rebooting etc
    if (strcmp(param, "MAC") == 0) {
        truncated = safe_strncpy (mac_address, value, MAC_ADDRESS_LENGTH-1);
        if (!truncated)
            make_derived_params();
        return truncated;
    }    
    // the following calls only set the prefix, not the full URL
    if (strcmp(param, "OTAP") == 0) {
        truncated = safe_strncpy_remove_slash (firmware_primary_prefix, value, MAX_LONG_STRING_LENGTH);
        return truncated;
    }
    if (strcmp(param, "OTAS") == 0) {
        truncated = safe_strncpy_remove_slash (firmware_secondary_prefix, value, MAX_LONG_STRING_LENGTH);
        return truncated;
    }
    if (strcmp(param, "CERTP") == 0) {
        truncated = safe_strncpy_remove_slash (certificate_primary_prefix, value, MAX_LONG_STRING_LENGTH);
        return truncated;
    }
    if (strcmp(param, "CERTS") == 0) {
        safe_strncpy_remove_slash (certificate_secondary_prefix, value, MAX_LONG_STRING_LENGTH);
        return truncated;
    }
    if (strcmp(param, "GRP") == 0) {     
        safe_strncpy (group_id, value, MAX_TINY_STRING_LENGTH);
        if (!truncated) {
            make_derived_params(); 
            // TODO: resubscribe to the new topic
        }
        return truncated;
    } 
    /*----------------    
    if (strcmp(param, "ORG") == 0) {         
        safe_strncpy (org_id, value, MAX_TINY_STRING_LENGTH);
        if (!truncated)
            make_derived_params(); // TODO: resubscribe to the new topic
        return truncated;
    } 
    if (strcmp(param, "APP") == 0) {         
        safe_strncpy (app_id, value, MAX_TINY_STRING_LENGTH); 
        if (!truncated)
            make_derived_params(); // TODO: resubscribe to the new topic
        return truncated;
    }    
    ----------------*/
    if (strcmp(param, "PRIREL") == 0) {         
        int rel = atoi(value); // returns zero if the integer string is invalid
        if (rel > 0 && rel < NUM_RELAYS) { 
            primary_relay = rel; 
            return false;  // false: all is OK
        }
        SERIAL_PRINTLN("\n*** Invalid relay number ****");
        return true; // true: ERROR
    }      
    if (strcmp(param, "ACTL") == 0) {
        if (value[0]=='1')      
            active_low = true;
        else 
            if (value[0]=='0')      
                active_low = false;  
            else 
                return true;  // ERROR 
        make_derived_params();
        return false;  // OK
    }     
    if (strcmp(param, "RTRIG") == 0) {         
        if (value[0]=='1')      
            radar_triggers = true;
        else 
            if (value[0]=='0')      
                radar_triggers = false;  
            else 
                return true;  // ERROR 
        return false;  // OK
    }          
    SERIAL_PRINTLN (F("--- ERROR: Unknown parameter ---"));     
    return true; // ERROR
}
