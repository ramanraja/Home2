// otaHelper.cpp

// TODO: implement life cycle methods onConnect, OnProgress, OnError etc...
// TODO: download from secondary FW serer URL , if the primary is unreachable
// TODO: rationalize all return codes and error messages
#include "otaHelper.h"

#ifdef MQTT_ENABLED
  char tmpstr[MAX_LONG_STRING_LENGTH];
#endif

OtaHelper::OtaHelper() {
#ifdef MQTT_ENABLED
    SERIAL_PRINTLN("OtaHelper:  MQTT version");
#else
    SERIAL_PRINTLN("OtaHelper:  Non-MQTT version");
#endif
}

#ifdef MQTT_ENABLED
void OtaHelper::init (Config *configptr,  PubSubClient *mqttptr) {
    this->pC = configptr;
    this->pM = mqttptr;    
}
#else
void OtaHelper::init(Config *configptr) {
    this->pC = configptr;
}
#endif

int OtaHelper::check_and_update() { 
   if (WiFi.status() != WL_CONNECTED) {
        SERIAL_PRINTLN("OtaHelper: No wifi connection !");        
        return -1;   // all positive status codes are taken by ESPhttpUpdate.update :)
    } // TODO: rationalize and standardize these error codes
    SERIAL_PRINTLN("OtaHelper: Checking for new firmware...");
    use_backup_urls = false;  // start optimistically with the primary server
#ifdef MQTT_ENABLED
    sprintf (tmpstr, "{\"C\":\"Current firmware version: %d\"}", pC->current_firmware_version);
    SERIAL_PRINTLN (tmpstr);        
    pM->publish(pC->mqtt_pub_topic, tmpstr);  
#endif    
    int result = HTTP_UPDATE_NO_UPDATES;
    if (check_version()) 
        result = update_firmware(); // TODO: if failed, check the secondary FW server 
    return(result); 
}

// Version check: Have a text file at the version_check_url, containing just a single number. 
// The number is the version of the firmware ready to be downloaded.
bool OtaHelper::check_version() {
    if (!pC->verison_check_enabled) {
        SERIAL_PRINTLN ("Version checking not enabled. Proceeding to update firmware..");
        return (true);
    }
    char* url;
    if (use_backup_urls)
        url = (char *)pC->get_secondary_version_url();
    else
        url = (char *)pC->get_primary_version_url();
    SERIAL_PRINTLN("Downloading Firmware version check file: ");
    SERIAL_PRINTLN(url);
    HTTPClient client;
    bool result = false;
    client.begin(url);
    int httpCode = client.GET();
    if(httpCode >= 200 && httpCode <= 308) { // code 304 = content unmodified; TODO: revisit!
        String str_version = client.getString();
        SERIAL_PRINT("Current firmware version: ");
        SERIAL_PRINTLN(pC->current_firmware_version);
        SERIAL_PRINT("Available firmware version: ");
        SERIAL_PRINTLN(str_version);
        int newVersion = str_version.toInt();
        if(newVersion > pC->current_firmware_version) {
            SERIAL_PRINTLN ("A new FW version is available");
            result = true;
        } else { 
            SERIAL_PRINTLN("This device already has the latest version.");
            result = false;
#ifdef MQTT_ENABLED            
            sprintf (tmpstr, "{\"C\":\"No new FW updates.\"}"); 
            pM->publish(pC->mqtt_pub_topic, tmpstr);            
#endif            
        }
    } 
    else {
        SERIAL_PRINT( "Cannot check firmware version. HTTP error code: " );
        SERIAL_PRINTLN( httpCode );      
        result = false;
#ifdef MQTT_ENABLED        
        sprintf (tmpstr, "{\"C\":\"Version check failed: HTTP error %d\"}", httpCode); 
        pM->publish(pC->mqtt_pub_topic, tmpstr);               
#endif       
    }
    client.end();
    if (!result) {
        if (!use_backup_urls) { // it fails for the first time, so give it another try (only this time!) 
            use_backup_urls = true; // set this once and for all for the rest of its life
            SERIAL_PRINTLN("Primary version check URL failed. Trying the secondary server...");
            check_version(); // NOTE: recursive call; this must be done only once !
        }
        else 
            SERIAL_PRINTLN("Secondary version check URL also failed. Giving up!");
    }
    return (result);
}    

// TODO: send various status and error messages back to server
int OtaHelper::update_firmware() {
    SERIAL_PRINTLN("Updating firmware..");  
#ifdef MQTT_ENABLED    
    sprintf (tmpstr, "{\"C\":\"Updating firmware..\"}"); 
    pM->publish(pC->mqtt_pub_topic, tmpstr);           
#endif    
    char* url; 
    if (use_backup_urls)
        url = (char*)pC->get_secondary_OTA_url();
    else
        url = (char*)pC->get_primary_OTA_url();
    SERIAL_PRINT("Looking for FW image file: ");
    SERIAL_PRINTLN(url);
    
    ESPhttpUpdate.rebootOnUpdate(1);   // always reboot 
    t_httpUpdate_return result_code = ESPhttpUpdate.update(url);
    // ----------- Now the ESP8266 will reboot ------------------
    // >>> will not reach here if the update succeeds <<<<
    SERIAL_PRINT("Return code from FW server: ");
    SERIAL_PRINTLN(result_code);
    int error_code = 0; // 0 means no error
    switch (result_code) {
        case HTTP_UPDATE_OK: // will never reach this case
            SERIAL_PRINTLN("HTTP update success !");
            break;
        case HTTP_UPDATE_FAILED:
            // TODO: new check the secondary FW server 
            error_code = ESPhttpUpdate.getLastError();
            SERIAL_PRINT("HTTP update FAILED. Error code = ");
            SERIAL_PRINTLN (error_code); 
            SERIAL_PRINTLN (ESPhttpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            SERIAL_PRINTLN("NO updates available");
            break;
    }  
#ifdef MQTT_ENABLED    
    sprintf (tmpstr, "{\"C\":\"Firmware update failed. result:%d, HTTP: %s, Error: %d\"}", 
             result_code, ESPhttpUpdate.getLastErrorString().c_str(), error_code); 
    pM->publish(pC->mqtt_pub_topic, tmpstr);              
#endif    
    return (result_code);
}
