// settings.h
#ifndef SETTINGS_H
#define SETTINGS_H

#define  FIRMWARE_VERSION       10           // increment the firmware version for every revision
#define  CERTIFICATE_VERSION    0            // increment when you want to change the certificates or config.txt file on the Flash

#define  CERT_VERSION_FILE      "certversion.txt"   // found on the cloud server; controls downlod of certificates
#define  NUM_CERTIFICATE_FILES  4          // 3 TLS certificates and one config.txt file
#define  CONFIG_FILE_NAME       "/config.txt"   // found on SPIFF; overrides settings.h and keys.h (The leading slash is essential !)
#define  CONFIG_FILE_SIZE       500        // bytes; the raw text file on the flash
#define  JSON_CONFIG_FILE_SIZE  700        // bytes; including json overhead: https://arduinojson.org/v6/assistant/

#define  BAUD_RATE              115200       // serial port
#define  ACTIVE_LOW_RELAY       0            // 1 for active low; 0 for active high relays
#define  NUM_RELAYS             2            // can be a maximum of 8 (->software constraint; but also depends on hardware pins)
#define  BLINK_COUNT            6            // device identifier (IFF) blinking
#define  PRIMARY_RELAY          0            // the main light for automatic control is 0,1,2... NUM_RELAYS
#define  RADAR_TRIGGERS         0            // if 0, PIR alone can trigger occupied status; if 1, both PIR and radar have to trigger

#define  STATUS_FREQUENCEY      5            // in minutes
#define  UNIVERSAL_GROUP_ID     "0"          // TODO: use this to listen for pan-group messages
#define  UNIVERSAL_DEVICE_ID    "0"          // all devices in a group listen on this channel

#define  ORG_ID                 "Myorg"      // this is part of MQTT topic name 
#define  GROUP_ID               "Grpid"         // test group; to be overriden by config.txt file in Flash 

#ifdef PORTICO_VERSION
  #define  APP_ID                 "portico"       // this is part of MQTT topic
  #define  APP_NAME               "Portico controller"  // this is only for display
#else
  #define  APP_ID                 "bath"       // this is part of MQTT topic
  #define  APP_NAME               "Sky Light"  // this is only for display
#endif

#define  NIGHT_START_HOUR       18           // 6.00 PM to 6.30 AM is considered night (for winter)
#define  NIGHT_END_HOUR         6
#define  NIGHT_START_MINUTE     0      
#define  NIGHT_END_MINUTE       30

#define  DAY_LIGHT_THRESHOLD    300
#define  NIGHT_LIGHT_THRESHOLD  100

// (for bathroom controller only): auto switch off timer duration, in minutes  
#define  AUTO_OFF_TIME_MIN      1.5           
#define  WIFI_PORTAL_TIMEOUT    60   //  180  //in seconds 
#endif
