//config.cpp
#include "config.h"

Config::Config(){
}

/* Example topics:
 publish:   intof/portico/status/G1/2CF432173BC0
 subscribe: intof/portico/cmd/G1/2CF432173BC0
 subscribe: intof/bath/cmd/G1/0
 */

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
    safe_strncpy (firmware_primary_prefix,  FW_PRIMARY_PREFIX, MAX_LONG_STRING_LENGTH);
    safe_strncpy (firmware_secondary_prefix, FW_BACKUP_PREFIX, MAX_LONG_STRING_LENGTH);    
    safe_strncpy (certificate_primary_prefix,  CERTIFICATE_PRIMARY_PREFIX, MAX_LONG_STRING_LENGTH);
    safe_strncpy (certificate_secondary_prefix, CERTIFICATE_BACKUP_PREFIX, MAX_LONG_STRING_LENGTH);  
        
    // now that the main parameters are in place, set up the derived parameters:
    make_derived_params();
    //dump();
    int result = load_config();  // read overriding params from Flash file    
    SERIAL_PRINT("SPIFF configuration load result: ");
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
    SERIAL_PRINTLN("Loading config from Flash...");
    if (!SPIFFS.begin()) {
        SERIAL_PRINTLN("--- Failed to mount file system. ---");
        ///SPIFFS.end();
        return SPIFF_FAILED;
    }
    // quick check if security certificates are at least present
    SERIAL_PRINTLN ("Checking TLS certificate files...");
    for (int i=1; i<NUM_CERTIFICATE_FILES; i++) {  // the first file is config.txt; it is ok if that file is missing
        SERIAL_PRINTLN (file_names[i]);
        if (!SPIFFS.exists(file_names[i])) {
            SERIAL_PRINTLN ("*** TLS certificate file missing ! ***");
            SPIFFS.end();
            return TLS_CERTIFICATE_FAILED;
        }
    }
    SERIAL_PRINTLN ("TLS certificate files present.");
    SERIAL_PRINTLN ("Checking Config file...");
    File configFile = SPIFFS.open(CONFIG_FILE_NAME, "r");
    if (!configFile) {
        SERIAL_PRINTLN("--- Failed to open config file. ---");
        SPIFFS.end();
        return FILE_OPEN_ERROR;
    }
    size_t size = configFile.size();
    SERIAL_PRINT ("Config file present. Size: ");
    SERIAL_PRINTLN(size);    
    if (size > CONFIG_FILE_SIZE) {
        SERIAL_PRINTLN("--- Config file size is too large. ---");
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
    SERIAL_PRINT ("Deserialization result: ");
    SERIAL_PRINTLN (error.c_str());
    if (error) {
        SERIAL_PRINTLN("--- Failed to parse config file. ---");
        SPIFFS.end();
        return JSON_PARSE_ERROR;
    }
    // string variables
    char *tmp;
    tmp = (char *)(doc["OTA1"] | FW_PRIMARY_PREFIX); 
    safe_strncpy (firmware_primary_prefix, tmp, MAX_LONG_STRING_LENGTH);
    tmp = (char *)(doc["OTA2"] | FW_BACKUP_PREFIX); 
    safe_strncpy (firmware_secondary_prefix, tmp, MAX_LONG_STRING_LENGTH);   
    tmp = (char *)(doc["CERT1"] | CERTIFICATE_PRIMARY_PREFIX); 
    safe_strncpy (certificate_primary_prefix, tmp, MAX_LONG_STRING_LENGTH);
    tmp = (char *)(doc["CERT2"] | CERTIFICATE_BACKUP_PREFIX); 
    safe_strncpy (certificate_secondary_prefix, tmp, MAX_LONG_STRING_LENGTH); 
    
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
    SERIAL_PRINTLN("\n-----------------------------------------");
    SERIAL_PRINTLN("               configuration             ");
    SERIAL_PRINTLN("-----------------------------------------");    
    print_heap();
    SERIAL_PRINT ("app name: ");
    SERIAL_PRINTLN (app_name);    
    SERIAL_PRINT ("Firmware version: ");
    SERIAL_PRINTLN (current_firmware_version);       
    SERIAL_PRINT ("TLS certificate version: ");
    SERIAL_PRINTLN (current_certificate_version);       
    SERIAL_PRINTLN();        
    SERIAL_PRINT ("primary OTA server: ");
    SERIAL_PRINTLN (get_primary_OTA_url());   
    SERIAL_PRINT ("primary version: ");
    SERIAL_PRINTLN (get_primary_version_url());   
    SERIAL_PRINT ("secondary OTA server: ");
    SERIAL_PRINTLN (get_secondary_OTA_url());   
    SERIAL_PRINT ("secondary version: ");
    SERIAL_PRINTLN (get_secondary_version_url());       
    
    SERIAL_PRINTLN ("primary certificate version check file: ");
    SERIAL_PRINTLN (get_primary_certificate_version_url());  
    SERIAL_PRINTLN ("secondary certificate version check  file: ");
    SERIAL_PRINTLN (get_secondary_certificate_version_url());  
    SERIAL_PRINTLN ("primary certificate files: ");   
    for (int i=0; i<NUM_CERTIFICATE_FILES; i++) 
        SERIAL_PRINTLN (get_primary_certificate_url(i));   
    SERIAL_PRINTLN ("secondary certificate files: ");
    for (int i=0; i<NUM_CERTIFICATE_FILES; i++)     
        SERIAL_PRINTLN (get_secondary_certificate_url(i));  
            
    SERIAL_PRINT("MQTT server: ");
    SERIAL_PRINTLN(AWS_END_POINT); 
    SERIAL_PRINT("MQTT Port: ");
    SERIAL_PRINTLN(MQTT_PORT); 
    SERIAL_PRINTLN();
    SERIAL_PRINT ("org id: ");
    SERIAL_PRINTLN (org_id);   
    SERIAL_PRINT ("app id: ");
    SERIAL_PRINTLN (app_id);      
    SERIAL_PRINT ("group id: ");
    SERIAL_PRINTLN (group_id);   
    SERIAL_PRINT ("MAC address: ");
    SERIAL_PRINTLN (mac_address);       
    SERIAL_PRINT ("Publish topic: ");
    SERIAL_PRINTLN (mqtt_pub_topic); 
    SERIAL_PRINT ("Subscribe topic: ");
    SERIAL_PRINTLN (mqtt_sub_topic); 
    SERIAL_PRINT ("Broadcast topic (sub): ");
    SERIAL_PRINTLN (mqtt_broadcast_topic);  
    SERIAL_PRINTLN();
    // the autonomous relay, which is triggered by movement, time of the day etc.
    SERIAL_PRINT ("primary (autonomous) realy number: ");
    SERIAL_PRINTLN (primary_relay);   
    // some relay modules need active LOW signals, ie, logic 0 operates the relay and 1 releases it
    SERIAL_PRINT ("active LOW relays ?: ");
    SERIAL_PRINTLN (active_low? "Yes":"No");          
    // to transition from unoccupied to occupied status, should radar also fire ? (0=only PIR; 1=both radar & PIR need to fire)
    SERIAL_PRINT ("radar triggers occupancy?: ");
    SERIAL_PRINTLN (radar_triggers ? "Yes":"No");   
    // time based automatic lights; hour and minute in 24 hour format 
    SERIAL_PRINT ("Night hours: ");
    SERIAL_PRINTLN (night_hours_str); 
    SERIAL_PRINT ("Day/Night Light threshold: ");
    SERIAL_PRINT (day_light_threshold);
    SERIAL_PRINT (" / ");
    SERIAL_PRINTLN(night_light_threshold); 
    SERIAL_PRINT ("status report frequency (minutes): ");
    SERIAL_PRINTLN (status_report_frequency);        
    SERIAL_PRINT ("auto off minutes: ");
    SERIAL_PRINTLN (auto_off_minutes);      
    SERIAL_PRINT ("auto off ticks: ");
    SERIAL_PRINTLN (get_auto_off_ticks());             
    yield();
    delay(1000);  // stabilize heap ?     
    print_heap();           
    SERIAL_PRINTLN("-----------------------------------------\n");       
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

// NOTE: the following two functions introduce a slash between the prefix and the file name; so this is not
// to be added in the prefix string
const char* Config::get_primary_certificate_version_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s?dummy=%d", certificate_primary_prefix, CERT_VERSION_FILE, random(0,1000));  
    return ((const char*) reusable_string);
}

const char* Config::get_secondary_certificate_version_url(){
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s?dummy=%d", certificate_secondary_prefix, CERT_VERSION_FILE, random(0,1000));  
    return ((const char*) reusable_string);
}

// NOTE: the following two functions do not add a slash between the prefix and the file name, since the SPIFF file name
// should mandatorily have a leading slash.
const char* Config::get_primary_certificate_url (short file_number){
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s%s?dummy=%d", certificate_primary_prefix, file_names[file_number], random(0,1000));  
    return ((const char*) reusable_string);
}

const char* Config::get_secondary_certificate_url (short file_number){
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s%s?dummy=%d", certificate_secondary_prefix, file_names[file_number], random(0,1000));  
    return ((const char*) reusable_string);
}

// NOTE: these functions introduce the slash again
const char*  Config::get_primary_OTA_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s.bin?dummy=%d", firmware_primary_prefix, app_id, 
    random(0,1000));  
    return ((const char*) reusable_string);
}

const char*  Config::get_primary_version_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s.txt?dummy=%d", firmware_primary_prefix, app_id, 
    random(0,1000));
    return ((const char*) reusable_string);    
}

const char*  Config::get_secondary_OTA_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s.bin?dummy=%d", firmware_secondary_prefix, app_id, 
    random(0,1000));
    return ((const char*) reusable_string);    
}

const char*  Config::get_secondary_version_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s.txt?dummy=%d", firmware_secondary_prefix, app_id, 
    random(0,1000));
    return ((const char*) reusable_string);    
}

// this is called from command handler through MQTT
// If the download succeeds, restarts ESP
int Config::download_certificates() {
    SERIAL_PRINTLN("[Config] Downloading TLS certificates..");  
    print_heap();
    Downloader D;
    D.init(this);
    D.list_files();
    int result = D.download_files();
    SERIAL_PRINT("[Config] download result: ");
    SERIAL_PRINTLN(get_error_message(result));
    print_heap();
    if (result==CODE_OK) {
        SERIAL_PRINTLN("[Config] Restarting ESP...");    
        delay(1000);
        ESP.restart();
    }
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
