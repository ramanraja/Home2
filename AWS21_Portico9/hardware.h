// hardware.h

#ifndef HARDWARE_H
#define HARDWARE_H

#include "common.h"
#include "config.h"
#include "settings.h"
#include <Timer.h>    // https://github.com/JChristensen/Timer
#include <DHT.h>      // https://github.com/adafruit/DHT-sensor-library (delete DHT_U.h & DHT_U.cpp)

class CommandHandler; // forward declaration

class Hardware {
public:
    data sensor_data;
    Hardware ();
    void init (Config *configptr, Timer *timerptr, CommandHandler *cmdptr);
    void infinite_loop ();
    void reboot_esp();
    const char* getStatus();
    const data* getData();
    bool getPir(); 
    bool getRadar();
    bool showPirStatus();
    bool showRadarStatus();
    bool read_sensors();
    void relay_on (short relay_number);
    void relay_off (short relay_number);
    void primary_light_on();
    void primary_light_off();
    void blink1();
    void blink2();
    void led_on (byte led_num);
    void led_off (byte led_num); 
    void set_led (byte led_num, bool is_on);
    void blink_led(byte led_num, byte times);
    void print_data();
private:
    Config *pC;
    Timer *pT;
    CommandHandler *pCmd;

    // GPIO pins in Node MCU nomenclature; note that the number of relays is hard coded in the next 3 lines
    byte relay_pin[2] =  {D1,D2}; //GPIO 5 and GPIo 4 for prod.  //{D0,D4} GPIO 16 and GPIO 2 for testing 
    boolean relay_status[2] = {0, 0};
    char relay_status_str[3];  // including the null
           
    byte led1 = D3;  // GPIO 0; NOTE: now, GPIO 0 cannot be used for the push button !
    byte led2 = D9;  // Rx
    byte LEDS[2] = {led1, led2};  // enum: green,red    
    const byte pir = D6;
    const byte radar = D5;
    const byte ldr   = A0;  
//    const byte temhum = D7; // DHT22 is at GPIO13
//    DHT dht(13, DHT22);   // this is actually a macro
    
    boolean pir_stat, radar_stat;
    float tem=0.0f, hum=0.0f, ind=0.0f;
    int valid_reading_count = 0;
    unsigned int light;
    int sensor_reading_count;
    void init_serial();
    void init_hardware();
    void read_dht_ldr();    
};


#endif
