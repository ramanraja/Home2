// pins.h

#ifndef PINS_H
#define PINS_H

#include "common.h"

// comment out if DHT sensor is not there:
#define  DHT_PRESENT
// comment out if you do not have custom external LEDs:
#define LEDS_PRESENT
// comment out if you do not have a PIR sensor:
#define PIR_PRESENT
// comment out if you do not have a microwave radar:
#define RADAR_PRESENT
// comment out if you do not have a light sensor:
#define LDR_PRESENT

// TODO: introduce NUM_RELAYS here      
#define RELAY1   5       // D1
#define RELAY2   4       // D2
#define PIR      12      // D6
#define RADAR    14      // D5
#define DHT_PIN  13      // D7
#define LDR      A0 

#ifdef LEDS_PRESENT
  #define LED1   0      // GPIO 0/ D3;   NOTE: now, GPIO 0 cannot be used for any push button 
  #define LED2   3      // D9 = Rx pin;  NOTE: you cannot receive any serial port messages (ESP will crash)
#else
  #define LED1   16     // D0; built into NodeMCU
  #define LED2   2      // D4; built into NodeMCU
#endif  

#endif
