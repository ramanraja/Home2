//common.h
#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>
extern "C" {
  #include "user_interface.h"
}

// comment out the following for bathroom controller
////////#define  PORTICO_VERSION

#define  MAX_COMMAND_LENGTH          8   // the raw command string, without json formatting & keys (usually 3 chars)
#define  MAC_ADDRESS_LENGTH          14  // 12+1 needed to hold 6 hyte HEX address
#define  MAX_LONG_STRING_LENGTH      128 // URLs, messages etc
#define  MAX_SHORT_STRING_LENGTH     64  // topic names, keys-values etc
#define  MAX_TINY_STRING_LENGTH      32  // wifi ssid, password, org id, app id, group id etc
#define  MAX_CLIENT_ID_LENGTH        23  // * MQTT standard allows max 23 characters for client id *  

// keep MQTT Rx messages short (~100 bytes); PubSubClient silently drops long messages !
#define  MAX_MSG_LENGTH              96  // MQTT message boxy (usully a json.dumps() string)
// The following constants override those in PubSubClient
#define  MQTT_KEEPALIVE              120  // override for PubSubClient keep alive, in seconds
/////#define  MQTT_MAX_PACKET_SIZE   256  // PubSubClient default is 128, including headers

// comment out this line to disable some informative messages
/////////#define  VERBOSE_MODE 

// comment out this line to disable all serial messages
#define ENABLE_DEBUG

#ifdef ENABLE_DEBUG
  #define  SERIAL_PRINT(x)       Serial.print(x)
  #define  SERIAL_PRINTLN(x)     Serial.println(x)
  #define  SERIAL_PRINTLNF(x,y)  Serial.println(x,y)   
  #define  SERIAL_PRINTF(x,y)    Serial.printf(x,y) 
#else
  #define  SERIAL_PRINT(x)
  #define  SERIAL_PRINTLN(x)
  #define  SERIAL_PRINTLNF(x,y)
  #define  SERIAL_PRINTF(x,y)
#endif

#define  TIME_DAY       0
#define  TIME_NIGHT     1
#define  TIME_UNKNOWN   2

#define  MAX_RELAYS     8  // assumption: there will not be more than 8 relays
                           // TODO: in sevaral files you have hard coded values for NUM_RELAYS

struct data {
    char  relay_status[MAX_RELAYS+1]; 
    float temperature;
    float humidity;
    float hindex;  // heat index
    int   light;
    int   pir_hits;
    int   radar_hits;
} ;

enum {green=0, red=1}; // we use this as array index, so they have to be 0 and 1 only

enum connection_result {
    CODE_OK = 0,
    PROCEED_TO_UPDATE,
    UPDATE_OK,
    AWS_CONNECT_SUCCESS,
    
    AWS_CONNECT_FAILED,
    TLS_CERTIFICATE_FAILED,
    TIME_SERVER_FAILED,
    
    BAD_URL,
    NO_WIFI,
    HTTP_FAILED,
    NOT_FOUND,
    NO_ACCESS,
    VERSION_CHECK_FAILED,
    UPDATE_FAILED,
    NO_UPDATES,
    
    SPIFF_FAILED,
    FILE_OPEN_ERROR,
    FILE_WRITE_ERROR,
    FILE_TOO_LARGE,
    JSON_PARSE_ERROR
} ;

#endif
