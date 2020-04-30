// myfiManager.cpp

#include "myfiManager.h"
  
bool MyFiManager::init(Config *configptr) {
    this->pC = configptr;
    SERIAL_PRINTLN(F("[WiFiMan] Starting Wifi Manager..."));
    snprintf (soft_AP_SSID, MAX_TINY_STRING_LENGTH-1, "%s%s", SOFT_AP_SSID_PREFIX, (char *)(pC->mac_address+6));  
    soft_AP_SSID[MAX_TINY_STRING_LENGTH-1] = '\0';
    SERIAL_PRINT(F("Soft AP SSID: "));
    SERIAL_PRINTLN(soft_AP_SSID);
    SERIAL_PRINT(F("Portal time out: "));
    SERIAL_PRINTLN(WIFI_PORTAL_TIMEOUT);    
    bool wifi_result = wifi_manager_auto_connect();
    // TODO: tryagain with a different backup AP ?
    return wifi_result;
}

bool MyFiManager::wifi_manager_auto_connect() {
    // WiFiManager local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    
    // Uncomment and run this once, if you want to erase all the stored wifi credentials:
    //wifiManager.resetSettings();
    
    // if you want a custom ip for the wifi manager portal
    //wifiManager.setAPConfig(IPAddress(10,0,1,10), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
    SERIAL_PRINTLN(F("If this is the first time, we will start Wifi configuration portal."));
    SERIAL_PRINTLN(F("Connect to INTOF_xxx AP, and point your browser to 192.168.4.1"));  // TODO: use SOFT_AP_SSID
  
     // make it reset and retry, or go to sleep
     wifiManager.setTimeout(WIFI_PORTAL_TIMEOUT); //in seconds  
      
    // fetches ssid and passwd from eeprom and tries to connect;
    // if it cannot connect, it starts an access point 
    // and goes into a *blocking* loop awaiting configuration
    SERIAL_PRINTLN(F("[WiFiMan] Auto connecting to Wifi..."));
    
    bool connection_result = wifiManager.autoConnect((const char*)soft_AP_SSID);
   
    // -- OR --
    // If you want to password protect your AP:
    // bool connection_result = wifiManager.autoConnect(soft_AP_SSID, SOFT_AP_PASSWD)
    // -- OR --
    // or use this for auto generated name ESP + ChipID
    //bool connection_result = wifiManager.autoConnect();

    SERIAL_PRINT(F("[WiFiMan] Auto Connect result: "));
    SERIAL_PRINTLN (connection_result);

    if (!connection_result) {
      SERIAL_PRINTLN(F("--- [WiFiMan] Could not connect to your WiFi network! ---"));
      return false;
    }
    SERIAL_PRINT(F("[WiFiMan] Connected to the Wifi network: "));
    SERIAL_PRINTLN(WiFi.SSID());
    SERIAL_PRINT(F("Local IP address: "));
    SERIAL_PRINTLN(WiFi.localIP());     
    SERIAL_PRINT(F("MAC address: "));
    SERIAL_PRINTLN(WiFi.macAddress());
    return true;
}

// TODO: this funcation is not normally called; Typically, he user has to trigger it using a dedicated button.
// NOTE: the button on GPIO0 on the circular Intof IoT box cannot be used, since it is  OUTPUT for an LED. 
// So the alternative is to send a command DEL to erase the wifi credentials, so that the portal runs on next boot.
void MyFiManager::start_wifi_manager_portal() {
    // WiFiManager local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    
    // Uncomment and run this once, if you want to erase all the stored wifi credentials:
    //wifiManager.resetSettings();
    
    // if you want a custom ip for the wifi manager portal
    //wifiManager.setAPConfig(IPAddress(10,0,1,10), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    
    // make it reset and retry, or go to sleep
    wifiManager.setTimeout(WIFI_PORTAL_TIMEOUT);  //in seconds     
     
    SERIAL_PRINTLN(F("On your request, Wifi configuration portal is starting...."));
    SERIAL_PRINTLN(F("Point your browser to 192.168.4.1"));
     
    if (!wifiManager.startConfigPortal(soft_AP_SSID)) {
      SERIAL_PRINTLN(F("failed to connect to Wifi and timed out"));
      delay(5000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(1000);
    }
    // -- OR --
    // If you want to password protect your AP:
    // wifiManager.startConfigPortal(soft_AP_SSID, SOFT_AP_PASSWD)
    // -- OR --
    // or use this for auto generated name: ESP + ChipID
    //wifiManager.startConfigPortal();
        
    //digitalWrite(led2, HIGH);  // exiting wifi config mode
    // if you get here, you have connected to the WiFi
    SERIAL_PRINT(F("Successfully connected to your Wifi network: "));
    SERIAL_PRINTLN(WiFi.SSID());
    SERIAL_PRINT(F("Local IP address: "));
    SERIAL_PRINTLN(WiFi.localIP());    
    SERIAL_PRINT(F("MAC address: "));
    SERIAL_PRINTLN(WiFi.macAddress());    
    //digitalWrite(led2, HIGH); // indicates end of config mode    
}

void MyFiManager::erase_wifi_credentials(bool reboot=true) {
    // WiFiManager local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    // erase all the stored wifi credentials:
    wifiManager.resetSettings();
    SERIAL_PRINTLN(F("WiFi credentials erased."));
    SERIAL_PRINTLN(F("Connect to INTOF_xxx AP and point your browser to 192.168.4.1"));   // TODO: use SOFT_AP_SSID
    if (reboot) {
      SERIAL_PRINTLN(F("\n---- Restarting the device... ----"));
      delay(2000);
      ESP.reset();
    }
}
