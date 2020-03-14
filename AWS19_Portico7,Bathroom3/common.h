//common.h
#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>
extern "C" {
  #include "user_interface.h"
}
#define MAX_COMMAND_LENGTH           12  // the raw command string, without json formatting & keys
#define MAC_ADDRESS_LENGTH           14
// URLs, messages etc
#define  MAX_LONG_STRING_LENGTH      128
// topic names, keys-values etc
#define  MAX_SHORT_STRING_LENGTH     64
// * MQTT standard allows max 23 characters for client id *
#define  MAX_CLIENT_ID_LENGTH        23    
// keep MQTT Rx messages short (<100 bytes); PubSubClient silently drops long messages !
// https://github.com/knolleary/pubsubclient/issues/66
#define  MAX_MSG_LENGTH              64  

// The following constants override those in PubSubClient
#define  MQTT_KEEPALIVE             120   // override for PubSubClient keep alive, in seconds
//#define  MQTT_MAX_PACKET_SIZE     256  // PubSubClient default was 128, including headers

// comment out this line to disable some informative messages
#define  VERBOSE_MODE 
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

enum connection_result {
    AWS_CONNECT_SUCCESS,
    CERTIFICATE_FAILED,
    TIME_SERVER_FAILED,
    AWS_CONNECT_FAILED
} ;
/*
struct clock_time {
    short  hour = -1;
    short  min = -1;
    short  sec = -1;
};
*/ 
#endif
