// otaHelper.cpp

#include "otaHelper.h"

OtaHelper::OtaHelper() {
}

void OtaHelper::init (Config *configptr) {
    SERIAL_PRINTLN("OtaHelper: Non-Mqtt version");
    this->pC = configptr;
    this->pM = NULL;
}

void OtaHelper::init(Config* configptr, MqttLite* mqttptr) {
    SERIAL_PRINTLN("OtaHelper starts. (Mqtt based)");  
    this->pM = mqttptr;
    this->pC = configptr;
}
        
int OtaHelper::check_and_update() { 
   if (WiFi.status() != WL_CONNECTED) {
        SERIAL_PRINTLN("OtaHelper: No wifi connection !");        
        return -1;   // all positive status codes are taken by ESPhttpUpdate.update :)
    }
    SERIAL_PRINTLN("OtaHelper: Checking for new firmware...");
    if (pM != NULL) {
        char tmpstr[200];
        sprintf (tmpstr, "-- Current firmware version: %d", pC->current_firmware_version);
        pM->publish(tmpstr);  
    }  
    int result = HTTP_UPDATE_NO_UPDATES; // if version number has not increased,..
    if (check_version())  // ...this will return false
        result = update_firmware(); 
    return(result); 
}

// Version check: Have a text file at the version_check_url, containing just a single number. 
// The number is the version of the firmware ready to be downloaded.
// TODO: send various status and error messages back to server
bool OtaHelper::check_version() {
    if (!pC->verison_check_enabled) {
        SERIAL_PRINTLN ("Version checking not enabled. Proceeding to update firmware..");
        return (true);
    }
    SERIAL_PRINTLN("Checking for firmware files..");
    SERIAL_PRINT("Firmware version file: ");
    SERIAL_PRINTLN(pC->version_check_url);

    bool result = false;
    HTTPClient client;
    client.begin(pC->version_check_url);
    int httpCode = client.GET();
    if(httpCode >= 200 && httpCode <= 308) { // code 304 = content unmodified
        String str_version = client.getString();
        SERIAL_PRINT("Current firmware version: ");
        SERIAL_PRINTLN(pC->current_firmware_version);
        SERIAL_PRINT("Available firmware version: ");
        SERIAL_PRINTLN(str_version);
        int newVersion = str_version.toInt();
        if(newVersion > pC->current_firmware_version) {
            SERIAL_PRINTLN ("A new version is available");
            result = true;
        } else { 
            SERIAL_PRINTLN("This device already has the latest version.");
            result = false;
            if (pM != NULL)
                pM->publish("-- Firmware: No new updates");            
        }
    } 
    else {  // http codde is not 200
        SERIAL_PRINT( "Cannot check firmware version. HTTP error code: " );
        SERIAL_PRINTLN( httpCode );      
        result = false;
        if (pM != NULL)
            pM->publish("-- Firmware update failed: HTTP error");
    }
    client.end();
    return (result);
}    

// TODO: send various status and error messages back to server
int OtaHelper::update_firmware() {
    SERIAL_PRINTLN("Updating firmware..");  
    if (pM != NULL)
        pM->publish("-- Updating firmware..");    
    SERIAL_PRINT("Looking for image file: ");
    SERIAL_PRINTLN(pC->firmware_server_url);
    
    ESPhttpUpdate.rebootOnUpdate(1);    // TODO: use a reboot flag in Config, instead of hard coding
    t_httpUpdate_return result_code = ESPhttpUpdate.update(pC->firmware_server_url);

    // ->>> will not reach here if the update succeeds; 8266 may be rebooted <<<<-
    SERIAL_PRINT("Return code from FW server: ");
    SERIAL_PRINTLN(result_code);
    switch (result_code) {
        case HTTP_UPDATE_OK:
            SERIAL_PRINTLN("HTTP update success !"); // this will not be reached if reboot is true
            break;
        case HTTP_UPDATE_FAILED:
            SERIAL_PRINT("HTTP update FAILED. Error code = ");
            SERIAL_PRINTLN (ESPhttpUpdate.getLastError()); 
            SERIAL_PRINTLN (ESPhttpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            SERIAL_PRINTLN("NO updates available");
            break;
    }  
    if (pM != NULL)
        pM->publish("-- Firmware update failed.");       
    return (result_code);
}
