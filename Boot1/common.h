//common.h
#ifndef COMMON_H
#define COMMON_H

#include <Arduino.h>
extern "C" {
  #include "user_interface.h"
}

#define  MAX_LONG_STRING_LENGTH      128 // URLs, messages etc
#define  MAX_SHORT_STRING_LENGTH     64  // topic names, keys-values etc
#define  MAX_TINY_STRING_LENGTH      32  // wifi ssid, password, org id, app id, group id etc

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



#endif
