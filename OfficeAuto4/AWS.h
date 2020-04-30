#ifndef AWS_H
#define AWS_H

#include "keys.h"
#include "common.h" 
#include "config.h"
#include "utilities.h"
#include <PubSubClient.h>       // https://github.com/knolleary/pubsubclient 
#include <ESP8266WiFi.h>
#include <NTPClient.h>        // https://github.com/arduino-libraries/NTPClient
#include <WiFiUdp.h>         // built in: /esp8266/Arduino/blob/master/libraries/ESP8266WiFi/
#include  <ArduinoJson.h>    // Blanchon, https://github.com/bblanchon/ArduinoJson
 
class AWS {
public : 
  //static char command_str[MAX_COMMAND_LENGTH];
  //static void callback(char* topic, byte* payload, unsigned int length);
  bool restart = false;
  AWS();
  short init(Config *configptr);
  short init(Config *configptr, bool restart);
  void update();
  PubSubClient* getPubSubClient();
  short  is_night_time(); 
private:
  Config *pC;
  short current_hour = -1;  // to detect time server failure
  short current_minute = -1;
    
  // TODO: revisit the following block of constants and reduce them:
  // The device should be able to work without an AWS connection, only periodically trying to connect
  // for time server connection:
  const int MAX_TIME_ATTEMPTS = 20;  // each attempt takes several seconds
  int time_attempts = 0;
  // for AWS connection
  const int RETRY_DELAY = 30000;  // mSec
  const int MAX_CONNECTION_ATTEMPTS = 10;  // 10*30 sec= at least 5 minutes
  int connection_attempts = 0;

  bool init_time_client();
  bool init_file_system();
  bool reconnect();
  void get_current_time(); 

/*
https://www.esp8266.com/viewtopic.php?p=82086
Error: cannot declare variable 'ntpUDP' to be of abstract type 'WiFiUDP'
There is also an advice about two WiFiUdp.h libraries, one in Arduino folder and one in ESP8266 folder. 
The IDE uses the version in the Arduino folder.
I deleted the WIFI library in Arduino folder and now I cam able to ompile my code.
*/
/*
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, "pool.ntp.org", 55*360);  // UTC offset=5.5 hours * 3600 seconds
  WiFiClientSecure espClient;
  PubSubClient client(AWS_END_POINT, MQTT_PORT, callback, espClient); //set  MQTT port number to 8883 as per standard  
*/
};
#endif
