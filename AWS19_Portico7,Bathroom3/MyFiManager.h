// myfiManager.h

#ifndef MYFI_MANAGER__H
#define MYFI_MANAGER__H

#include "common.h" 
#include "config.h"
#include "keys.h" 
#include <ESP8266WiFi.h>
#include <WiFiManager.h>     // https://github.com/tzapu/WiFiManager 
 
class  MyFiManager {
public : 
  void init();
  void wifi_manager_auto_connect(); 
  void start_wifi_manager_portal();
  void erase_wifi_credentials(bool reboot);
private:
  Config *pC;
};
#endif
