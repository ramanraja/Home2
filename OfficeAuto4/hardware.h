// hardware.h

#ifndef HARDWARE_H
#define HARDWARE_H

#include "common.h"
#include "pins.h"
#include "config.h"
#include "settings.h"
#include "utilities.h"
#include <Timer.h>    // https://github.com/JChristensen/Timer
#include <DHT.h>      // https://github.com/adafruit/DHT-sensor-library (delete DHT_U.h & DHT_U.cpp)

#define  MAX_LIGHT        1024   // NodeMCU has 10 bit ADC
#define  NOISE_THRESHOLD  10    // when the light sensor fails, this will be the max ADC noise

class CommandHandler; // forward declaration

class Hardware {
public:
    data sensor_data;
    Hardware ();
    void  init (Config *configptr, Timer *timerptr, CommandHandler *cmdptr);
    void  infinite_loop ();
    void  reboot_esp ();
    short is_night_time(); 
    const char* getStatus ();
    const data* getData ();
    bool  getPir (); 
    bool  getRadar ();
    bool  showPirStatus ();
    bool  showRadarStatus ();
    bool  read_sensors();
    void  release_all_relays();    
    void  relay_on  (short relay_number);
    void  relay_off (short relay_number);
    void  primary_light_on ();
    void  primary_light_off ();
    void  blink1 ();
    void  blink2 ();
    void  blink3 ();    
    void  led_on (byte led_num);
    void  led_off (byte led_num); 
    void  set_led (byte led_num, bool is_on);
    void  blink_led (byte led_num, byte times);
    void  print_data ();
    
private:
    Timer  *pT;
    Config *pC;
    CommandHandler *pCmd;
   
    // GPIO pins in Node MCU nomenclature; note that the number of relays is hard coded in the next 3 lines
    byte  relay_pin[2] = {RELAY1, RELAY2};   // TODO: introduce NUM_RELAYS here  
    byte  led1 = LED1;              
    byte  led2 = LED2;                
    byte  LEDS[2] = {led1, led2};  // enum: green,red    
#ifdef PIR_PRESENT      
    const byte pir = PIR;
    bool pir_fired = false;
#endif
#ifdef RADAR_PRESENT      
    const byte radar = RADAR;
    bool radar_fired = false;
#endif
#ifdef LDR_PRESENT      
    const byte ldr = LDR;  
#endif  
    char    relay_status_str[3];       // including the null // TODO: introduce NUM_RELAYS here   
    boolean relay_status[2] = {0, 0};  // TODO: introduce NUM_RELAYS here   
    boolean pir_status; 
    boolean radar_status;
    float   temperature; // holds cumulative intermediate values before averaging
    float   humidity;    // holds cumulative intermediate values before averaging
    float   hindex;
    short   light;
    
    int valid_reading_count = 0;
    int sensor_reading_count;
    
    void init_serial();
    void print_configuration();
    void init_hardware();
    void read_dht_ldr();    
};


#endif
