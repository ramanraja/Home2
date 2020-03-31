//config.cpp
#include "config.h"

Config::Config(){
}

void Config::init() {
 safe_strncpy (app_id, APP_ID, MAX_TINY_STRING_LENGTH);
    safe_strncpy (firmware_primary_prefix,  FW_PRIMARY_PREFIX, MAX_LONG_STRING_LENGTH);
    safe_strncpy (firmware_secondary_prefix, FW_BACKUP_PREFIX, MAX_LONG_STRING_LENGTH);    
    safe_strncpy (certificate_primary_prefix,  CERTIFICATE_PRIMARY_PREFIX, MAX_LONG_STRING_LENGTH);
    safe_strncpy (certificate_secondary_prefix, CERTIFICATE_BACKUP_PREFIX, MAX_LONG_STRING_LENGTH);      
    dump();
    load_config();  // read overriding params from Flash file
}

bool Config::load_config() {
    SERIAL_PRINTLN("Loading config from Flash...");
    if (!SPIFFS.begin()) {
        Serial.println("--- Failed to mount file system. ---");
        return false;
    }
    File configFile = SPIFFS.open(CONFIG_FILE_NAME, "r");
    if (!configFile) {
        Serial.println("--- Failed to open config file. ---");
        SPIFFS.end();
        return false;
    }
    Serial.println ("Config file opened.");
    size_t size = configFile.size();
    if (size > CONFIG_FILE_SIZE) {
        Serial.println("--- Config file size is too large. ---");
        SPIFFS.end();
        return false;
    }
    // Allocate a buffer to store file contents ***
    // ArduinoJson library requires the input buffer to be mutable     
    std::unique_ptr<char[]> buf(new char[size]);
    configFile.readBytes(buf.get(), size);
    
    // Consider this number carefully; json needs extra space for metadata:
    StaticJsonDocument<JSON_CONFIG_FILE_SIZE> doc;  // including extra space for Json
    auto error = deserializeJson(doc, buf.get());
    Serial.print ("Deserialization result: ");
    Serial.println (error.c_str());
    if (error) {
        Serial.println("--- Failed to parse config file. ---");
        return false;
        SPIFFS.end();
    }
    char *tmp;
    tmp = (char *)(doc["APP"] | APP_ID); 
    safe_strncpy (app_id, tmp, MAX_TINY_STRING_LENGTH); 
    tmp = (char *)(doc["OTA1"] | FW_PRIMARY_PREFIX); 
    safe_strncpy (firmware_primary_prefix, tmp, MAX_LONG_STRING_LENGTH);
    tmp = (char *)(doc["OTA2"] | FW_BACKUP_PREFIX); 
    safe_strncpy (firmware_secondary_prefix, tmp, MAX_LONG_STRING_LENGTH);   
    tmp = (char *)(doc["CERT1"] | CERTIFICATE_PRIMARY_PREFIX); 
    safe_strncpy (certificate_primary_prefix, tmp, MAX_LONG_STRING_LENGTH);
    tmp = (char *)(doc["CERT2"] | CERTIFICATE_BACKUP_PREFIX); 
    safe_strncpy (certificate_secondary_prefix, tmp, MAX_LONG_STRING_LENGTH);  
    SPIFFS.end();  // unmount file system
    return true;
}

void Config::dump() {
    SERIAL_PRINTLN("\n-----------------------------------------");
    SERIAL_PRINTLN("               configuration             ");
    SERIAL_PRINTLN("-----------------------------------------");    
    print_heap();

    SERIAL_PRINT ("Firmware version: ");
    SERIAL_PRINTLN (current_firmware_version);       
    SERIAL_PRINT ("TLS certificate version: ");
    SERIAL_PRINTLN (current_certificate_version);          
    SERIAL_PRINT ("app id: ");
    SERIAL_PRINTLN (app_id);  
    SERIAL_PRINTLN();
        
    SERIAL_PRINTLN ("primary OTA server: ");
    SERIAL_PRINTLN (get_primary_OTA_url());   
    SERIAL_PRINTLN ("primary OTA version check file: ");
    SERIAL_PRINTLN (get_primary_version_url());  
     
    SERIAL_PRINTLN ("secondary OTA server: ");
    SERIAL_PRINTLN (get_secondary_OTA_url());   
    SERIAL_PRINTLN ("secondary OTA version check file: ");
    SERIAL_PRINTLN (get_secondary_version_url());    
    SERIAL_PRINTLN();

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
    yield();
    delay(1000);  // stabilize heap ? 
    print_heap();           
    SERIAL_PRINTLN("-----------------------------------------\n");       
}

short Config::get_num_files() {
    return NUM_CERTIFICATE_FILES;
}
// NOTE: reusable_string is a scratch pad string; it holds one URL at a time, to save memory!
// So don't call any of these functions while the previous string is still in use 
// NOTE: the following two functions introduce a slash between the prefix and the file name; so this is not
// to be added in the prefix string
const char* Config::get_primary_certificate_version_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s?dummy=%d", certificate_primary_prefix, CERT_VERSION_FILE, random(0,1000));  
    return ((const char*) reusable_string);
}

// add a random parameter to the URL to disable cache:
// https://stackoverflow.com/questions/50699554/my-esp8266-using-cached-how-to-fix
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
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s.bin?dummy=%d", firmware_primary_prefix, app_id, random(0,1000));  
    return ((const char*) reusable_string);
}

const char*  Config::get_primary_version_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s.txt?dummy=%d", firmware_primary_prefix, app_id, random(0,1000));
    return ((const char*) reusable_string);    
}

const char*  Config::get_secondary_OTA_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s.bin?dummy=%d", firmware_secondary_prefix, app_id, random(0,1000));
    return ((const char*) reusable_string);    
}

const char*  Config::get_secondary_version_url() {
    snprintf (reusable_string, MAX_LONG_STRING_LENGTH-1, "%s/%s.txt?dummy=%d", firmware_secondary_prefix, app_id, random(0,1000));
    return ((const char*) reusable_string);    
}
