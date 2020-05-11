
// Internet switch to control 2 pump motors ('sweet' and 'salt' water pumps).
// Receives an MQTT message to switch ON/ OFF using Adafruit. Can be controlled by Alexa through IFTTT.
// The topic names have been unified across projects.
// Say "Alexa, trigger drinking water on" / ""Alexa, trigger salt water off"
// Replies with a binary string with status of the 2 relays.
// Use the rudimentary Android application 'motor'  to test this, and also to update firmware.
// Use the test tool: MQTT-FX for Mqtt. This is also useful to issue the firmware update command 'UPD'.
// Adafruit feed nameing guidelines:  https://io.adafruit.com/blog/tips/2016/07/14/naming-feeds/
/*
The following Node MCU pins output 3.3v signal on Boot: GPIO 1,3,9,10,16
All other GPIOs pin provide LOW signal on Boot - except GPIO4 and GPIO5. 
So GPIO4 (D2) and GPIO5 (D1) are the best pins to connect relays, for stable results.
*/

#include "common.h" 
#include "config.h"
#include "myfi.h"
#include "MqttLite.h"
#include "otaHelper.h"
#include <Timer.h>          // https://github.com/JChristensen/Timer
  
#define NUM_RELAYS         2 
int relays[2] =  {D1,D2};  // {D0, D4} ;  // D1 and D2 are safe for relays
int leds[2] = {D6, D7}; 
boolean stats[2] = {0,0};
int buzzer = D3;   // active low 3.3V operated buzzer

// LEDs are active low
// 30A motor relays are configured active high (ON=HIGH) by default
#define ON    HIGH
#define OFF   LOW

#define SWEET_MOTOR     0
#define SALT_MOTOR      1
char text[128];

Timer T;
Config C;
MyFi W;
MqttLite M;
OtaHelper O;

void setup() {
    init_serial();
    init_hardware();
    C.init();
    C.dump();    
    W.init (&C);
    W.dump();    
    M.init (&C, &W);
    O.init (&C, &M); 
    sprintf (text, "-- Pump motor controller V %d is restaring...", FIRMWARE_VERSION);
    M.publish(text);
    delay(2000);  // Adafruit broker rate
    check_for_updates();  // this may reset the chip after update
}

void loop() {
    T.update(); 
    // W.update(); // this is done by M
    M.update();    
}

void motor_on (int motor) {
    digitalWrite (relays[motor], ON); 
    stats[motor] = 1;
    digitalWrite (leds[motor], LOW);  // active low  
    beep1();
}

void motor_off (int motor) {
    digitalWrite (relays[motor], OFF); 
    stats[motor] = 0;
    digitalWrite (leds[motor], HIGH);  // active low  
    beep2();
}

void beep1 () {
    T.pulse (buzzer, 500, HIGH); 
}

void beep2 () {
    T.oscillate (buzzer, 200, HIGH, 3); 
}

void init_hardware() {         
    pinMode(buzzer, OUTPUT);    
    digitalWrite (buzzer, HIGH);     // active low
    for (int i=0; i<NUM_RELAYS; i++) {   // TODO: remember last status after power failure
        pinMode(leds[i], OUTPUT);       
        pinMode(relays[i], OUTPUT);  
        digitalWrite (relays[i], OFF);  // defined above  
        stats[i] = 0;  // 0 means relay status is OFF
    }   
    blinker();
}

void init_serial () {
    #ifdef ENABLE_DEBUG
        Serial.begin(BAUD_RATE); 
        #ifdef VERBOSE_MODE
          Serial.setDebugOutput(true);
        #endif
        Serial.setTimeout(250);
    #endif    
    SERIAL_PRINTLN("\n*******  Pump motor controller starting... ********\n"); 
    SERIAL_PRINT("FW version: ");
    SERIAL_PRINTLN(FIRMWARE_VERSION);
}

void blinker() {
    for (int i=0; i<4; i++) {
        digitalWrite(leds[0], LOW); // active low
        digitalWrite(leds[1], HIGH); // active low        
        delay(200);
        digitalWrite(leds[0], HIGH);
        digitalWrite(leds[1], LOW);        
        delay(200);        
    }
    digitalWrite(leds[1], HIGH);     
}

void check_for_updates() {
    SERIAL_PRINTLN ("\n<<<<<<---------  checking for FW updates... ----------->>>>>>\n");
    int result = O.check_and_update();  // if there was an update, this will restart 8266
    SERIAL_PRINT ("OtaHelper: response code: ");   // if the update failed
    SERIAL_PRINTLN (result);
    beep2(); // ready to receive commands again
}

//-----------------------------------------------------------------------------------

int relay_number = 0;
void  app_callback(const char* command_string) {
    SERIAL_PRINTLN ("app_callback: MQTT message received :");
    SERIAL_PRINTLN (command_string);
    if (strlen (command_string) < 3) {
        SERIAL_PRINTLN ("Command has to be either STA or UPD or ONx or OFFx where x is from 0-9");
        return;
    }
    // STATUS command
    if (command_string[0]=='S' && command_string[1]=='T' && command_string[2]=='A') {
        send_status();
        return;
    }
    // UPDATE firmware by OTA
    // Run a HTTP server at the specified URL and serve the firmware file
    if (command_string[0]=='U' && command_string[1]=='P' && command_string[2]=='D') {
        check_for_updates();  // this may reset the chip after update
        return;
    }    
    // ON commands
    if (command_string[0]=='O' && command_string[1]=='N') {
        if (command_string[2] < '0' ||  command_string[2] >= NUM_RELAYS+'0') {
            SERIAL_PRINTLN ("-- Error: Invalid relay number --");
            return;
        }
        relay_number = command_string[2] - '0';
        SERIAL_PRINT ("Remote command: ON; Relay : ");
        SERIAL_PRINTLN (relay_number);
        motor_on(relay_number); 
        send_status();       
        return;
    }
    // OFF commands
    if (strlen (command_string) < 4) {
        SERIAL_PRINTLN ("Command has to be either ONx or OFFx where x is from 0-9");
        return;
    }    
    if (command_string[0]=='O' && command_string[1]=='F' && 
        command_string[2]=='F') {
        if (command_string[3] < '0' ||  command_string[3] >= NUM_RELAYS+'0') {
            SERIAL_PRINTLN ("-- Error: Invalid relay number --");
            return;
        }
        relay_number = command_string[3] - '0';
        SERIAL_PRINT ("Remote command: OFF; Relay : ");
        SERIAL_PRINTLN (relay_number);
        motor_off(relay_number); 
        send_status();      
    }
    return;   
    /***
    // EXIT command: may be useful to shut down the app, in case of a hacker attack ?
    if (command_string[0]=='E' && command_string[1]=='X' && 
        command_string[2]=='I' && command_string[3]=='T') {
        SERIAL_PRINTLN ("Remote cam app has terminated !");
        if (C.sleep_deep) {
            M.publish("IoT relay goes to sleep..");
            digitalWrite (led, LOW);
            delay(2000);
            digitalWrite (led, HIGH);
            ESP.deepSleep(0);
        }
        else
            T.pulse(led,2000,HIGH);
    }   asz
    TODO: disconnect from MQTT server so that LWT is invoked
    ***/  
}

char status_str [5];
void send_status() {
    sprintf (status_str, "%1d%1d", stats[0],stats[1]);
    M.publish(status_str);
}
