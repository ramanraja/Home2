// myfiManager.cpp

#include "myfiManager.h"
  
void MyFiManager::init() {
    SERIAL_PRINTLN("Starting Wifi Manager...");
    wifi_manager_auto_connect();
}

void MyFiManager::wifi_manager_auto_connect() {
    // WiFiManager local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    
    // Uncomment and run this once, if you want to erase all the stored wifi credentials:
    //wifiManager.resetSettings();
    
    // if you want a custom ip for the wifi manager portal
    //wifiManager.setAPConfig(IPAddress(10,0,1,10), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
    SERIAL_PRINTLN ("If this is the first time, we will start Wifi configuration portal.");
    SERIAL_PRINTLN ("Connect to INTOF_AP, and point your browser to 192.168.4.1");  // TODO: use SOFT_AP_SSID
  
     // make it reset and retry,or go to sleep
     // wifiManager.setTimeout(120);  //in seconds
      
    // fetches ssid and passwd from eeprom and tries to connect;
    // if it cannot connect, it starts an access point 
    // and goes into a *blocking* loop awaiting configuration
    
    wifiManager.autoConnect(SOFT_AP_SSID);
    
    // -- OR --
    // If you want to password protect your AP:
    // wifiManager.autoConnect(SOFT_AP_SSID, SOFT_AP_PASSWD)
    // -- OR --
    // or use this for auto generated name ESP + ChipID
    //wifiManager.autoConnect();
  
    // if you get here, you have connected to the WiFi
    SERIAL_PRINT("Connected to your Wifi network: ");
    SERIAL_PRINTLN(WiFi.SSID());
    SERIAL_PRINT("Local IP address: ");
    SERIAL_PRINTLN(WiFi.localIP());     
    SERIAL_PRINT("MAC address: ");
    SERIAL_PRINTLN(WiFi.macAddress());
}

// TODO: this funcation is never called now; The user has to trigger it using a dedicated button.
// NOTE: the button on GPIO0 on the circular box cannot be used, since it is connected to an LED. 
void MyFiManager::start_wifi_manager_portal() {
    // WiFiManager local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    
    // Uncomment and run this once, if you want to erase all the stored wifi credentials:
    //wifiManager.resetSettings();
    
    // if you want a custom ip for the wifi manager portal
    //wifiManager.setAPConfig(IPAddress(10,0,1,10), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    
    // make it reset and retry, or go to sleep
    // wifiManager.setTimeout(120);  //in seconds     
     
    SERIAL_PRINTLN ("On your request, Wifi configuration portal is starting....");
    SERIAL_PRINTLN ("Point your browser to 192.168.4.1");
     
    if (!wifiManager.startConfigPortal(SOFT_AP_SSID)) {
      SERIAL_PRINTLN("failed to connect to Wifi and timed out");
      delay(5000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(1000);
    }
    // -- OR --
    // If you want to password protect your AP:
    // wifiManager.startConfigPortal(SOFT_AP_SSID, SOFT_AP_PASSWD)
    // -- OR --
    // or use this for auto generated name: ESP + ChipID
    //wifiManager.startConfigPortal();
        
    //digitalWrite(led2, HIGH);  // exiting wifi config mode
    // if you get here, you have connected to the WiFi
    SERIAL_PRINT("Successfully connected to your Wifi network: ");
    SERIAL_PRINTLN(WiFi.SSID());
    SERIAL_PRINT("Local IP address: ");
    SERIAL_PRINTLN(WiFi.localIP());    
    SERIAL_PRINT("MAC address: ");
    SERIAL_PRINTLN(WiFi.macAddress());    
    //digitalWrite(led2, HIGH); // indicates end of config mode    
}

void MyFiManager::erase_wifi_credentials(bool reboot=true) {
    // WiFiManager local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    // erase all the stored wifi credentials:
    wifiManager.resetSettings();
    SERIAL_PRINTLN("WiFi credentials erased.");
    SERIAL_PRINTLN("Connect to INTOF_AP and point your browser to 192.168.4.1");   // TODO: use SOFT_AP_SSID
    if (reboot) {
      SERIAL_PRINTLN("\n---- Restarting the device... ----");
      delay(2000);
      ESP.reset();
    }
}
