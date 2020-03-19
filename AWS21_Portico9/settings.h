// settings.h
#ifndef SETTINGS_H
#define SETTINGS_H

#define  FIRMWARE_VERSION   11  // increment the firmware version for every revision

#define  ACTIVE_LOW_RELAY   // comment this out for active HIGH relays

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
#define  PRIMARY_LIGHT      1     // the main portico tube light is 1, at D2. (the side bulb is 0 at D1)

#define  ORG_ID             "intof"
#define  GROUP_ID           "G1"
#define  APP_ID             "portico"     // this is part of MQTT topic
#define  APP_NAME           "Portico Controller"  // this is only for display

#define  STATUS_REPORT_FREQUENCEY     5  // in minutes

// 6 PM to 6.30 AM is considered night (for winter)
#define  NIGHT_START_HOUR       18   
#define  NIGHT_END_HOUR         6
#define  NIGHT_START_MINUTE     0      
#define  NIGHT_END_MINUTE       30

#endif
