// settings.h
#ifndef SETTINGS_H
#define SETTINGS_H

#define  FIRMWARE_VERSION   5  // increment the firmware version for every revision

/////#define  ACTIVE_LOW_RELAY   // comment this out for active HIGH relays

#ifdef ACTIVE_LOW_RELAY
    // active low relays
    #define  ON    0
    #define  OFF   1
#else
    // active high relays
    #define  ON    1
    #define  OFF   0
#endif

#define  NUM_RELAYS         2
#define  BLINK_COUNT        6     // IFF blinking
#define  PRIMARY_LIGHT      0     // the main bathroom light is 0 at D1

#define  ORG_ID             "intof"
#define  GROUP_ID           "G1"
#define  APP_ID             "bath"     // this is part of MQTT topic
#define  APP_NAME           "Sky Light"  // this is only for display

#define  STATUS_REPORT_FREQUENCEY     5  // in minutes

// 6 PM to 6.30 AM is considered night (for winter)
#define  NIGHT_START_HOUR       18   
#define  NIGHT_END_HOUR         6
#define  NIGHT_START_MINUTE     0      
#define  NIGHT_END_MINUTE       30

//---------------- application specific -------------------------------
// auto switch off timer duration, in minutes  
#define  MAX_BUCKET             6    // 6x10sec = 1 minute time out to switch off
//---------------- end application specific ---------------------------

#endif
