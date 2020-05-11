// Water level sensor merged with MQTT and OTA capabilities. 
// Code imported from motor2.ino and water8.ino
// TODO: on overflow, publish a command to stop the motor without human intervention
// TODO: do not switch on/off the alarm on ONx/OFFx message; do it based on the response from motor.ino
// ESP12 based sensor sounds a buzzer when water level in the tank reaches the top.
// New in this version:   
//When a motor is switched on, the same MQTT command starts the corresponding level
//  sensor also. If the motor is remotely switched off, the sensing stops.
//When water level reaches the sensor, the sensor issues the corresponding motor OFF command 
//  and automatically stops the motor.
// Every sensor uses two pins:
// One pin acts as digital input using internal pullup; the other pin is grounded
//    programmatically (not using the permanent ground pin)
// To avoid prolonged electrolysis:  
//   1) We enable the sensor briefly, only when taking measurement
//   2) the live and ground pins are interchanged for every measurement cycle
// Both electrodes are mounted near the top of the water tank, separated by about an inch.
// Integrates Alexa voice commands through IFTTT and Adafruit MQTT broker.
// Use it with Alexa, MQTT-FX, or the custom Android app  'Motor'.
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

short sweet_sensor[2] = {D1, D5}; // GPIO 5,14 
short salt_sensor[2] = {D2, D6}; // GPIO 4,12 
short sweet_led = D0;           // GPIO 16; built-in LED
short salt_led = D4;           // GPIO 2; built-in LED

/****
short enable_led = D3;        // GPIO 0; must be HIGH to boot
short enable_switch = D7;    // GPIO 13
short mute = D8;            // GPIO 15; must be LOW to boot; push button to mute beeper
short beeper = D9;         // GPIO 3; same as Rx pin *;  active low 3.3V operated buzzer  
***/

short beeper = D7; // GPIO 13; active low 3.3V operated buzzer

short NUM_SENSORS = 2;  // sweet, salt tank-full sensors

enum {SLOW, FAST};
short FAST_SAMPLE = 2;  // read a sensor once in 2 seconds
short SLOW_SAMPLE = 5;   // 60;  // once every minute
short sweet_sample_period = FAST_SAMPLE;
short salt_sample_period = FAST_SAMPLE;
bool sweet_overflow = false;
bool salt_overflow = false;
bool sweet_enabled = false;  // the sensors will be armed only when a motor ON command is received:
bool salt_enabled = false;   // avoid prolonged sensing when power is switched on 
bool sweet_muted = false;    // disable audible beep, but sensors will be working
bool salt_muted = false;
bool sweet_polarity_flag = false;  // flags to reverse the live and ground leads:
bool salt_polarity_flag = false;   // we reverse the polarity for every measurement
short live = 0;   // the two sensor electrodes in a tank
short ground = 1;
char reusable_str[96];
//const char *sweet_off_command = "OFF0";
//const char *salt_off_command  = "OFF1";

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
    sprintf (reusable_str, "-- Water level sensor V %d is restaring..", FIRMWARE_VERSION);
    M.publish(reusable_str);
    delay(2100);  // Adafruit needs 2 sec gap befor check_for_updates publishes to MQTT
    check_for_updates();  // this may reset the chip after update
    T.every (sweet_sample_period, sample_sweet);
    delay(1000); // start them staggered
    T.every (salt_sample_period, sample_salt);
    T.every (4000, beep1); // on trigger, the OFF command will be issued only after 2*4000 mSec
    delay(2000); // start them staggered
    T.every (4000, beep2);       
}

void loop() {
    T.update();
    // W.update(); // this is done by M
    M.update(); 
}

void sample_sweet() {
    if (!sweet_enabled) 
        return;
    sense_sweet(sweet_polarity_flag);
    sweet_polarity_flag = !sweet_polarity_flag;
}

void sample_salt() {
    if (!salt_enabled) 
        return;
    sense_salt(salt_polarity_flag);
    salt_polarity_flag = !salt_polarity_flag;
}

int sensor_number = 0;
void  app_callback(const char* command_string) {
    SERIAL_PRINTLN ("app_callback: MQTT msg received :");
    SERIAL_PRINTLN (command_string);
    if (strlen (command_string) < 3) {
        SERIAL_PRINTLN ("Command has to be either STA or UPD or ONx or OFFx where x is from 0-9");
        send_error();
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
        if (command_string[2] < '0' ||  command_string[2] >= NUM_SENSORS+'0') {
            SERIAL_PRINTLN ("-- Error: Invalid sensor number --");
            send_error();
            return;
        }
        sensor_number = command_string[2] - '0';
        SERIAL_PRINT ("Remote command: ENABLE; sensor : ");
        SERIAL_PRINTLN (sensor_number);
        handle_command(sensor_number+1); 
        send_status();       
        return;
    }
    // OFF commands
    if (strlen (command_string) < 4) {
        SERIAL_PRINTLN ("Command has to be either ONx or OFFx where x is from 0-9");
        send_error();
        return;
    }    
    if (command_string[0]=='O' && command_string[1]=='F' && 
        command_string[2]=='F') {
        if (command_string[3] < '0' ||  command_string[3] >= NUM_SENSORS+'0') {
            SERIAL_PRINTLN ("-- Error: Invalid sensor number --");
            send_error();
            return;
        }
        sensor_number = command_string[3] - '0';
        SERIAL_PRINT ("Remote command: DISABLE; sensor : ");
        SERIAL_PRINTLN (sensor_number);
        handle_command(sensor_number+3); 
        send_status();      
        return;
    }
    send_error(); // unexpected command string
}

void send_error() {
    SERIAL_PRINTLN("invalid command: send_error");
    strcpy (reusable_str, "-- ERROR");
    M.publish(reusable_str);
}

// TODO: Make this json string
void send_status() {
    sprintf (reusable_str, "%1d%1d%1d%1d", sweet_enabled,sweet_overflow,salt_enabled,salt_overflow);
    M.publish(reusable_str);
}

void handle_command (int command) {
    switch(command) {
        case 1:
            sweet_enabled = true;
            break;
        case 2:
            salt_enabled = true;
            break;
        case 3:
            sweet_enabled = false;
            sweet_overflow = false;  // force clear it; otherwise the alarm will be stuck
            digitalWrite(sweet_led, !sweet_overflow);
            break;
        case 4:
            salt_enabled = false;
            salt_overflow = false; // clear the flag, to avoid alarm latching
            digitalWrite(salt_led, !salt_overflow);
            break;
        default:
            Serial.print ("invalid command:");
            Serial.println (command);       
            return; 
            //break;  
    }
    Serial.print ("command:");
    Serial.println (command);     
    /////send_status();
}

// NOTE: sense_Sweet and sense_salt are twin functions with identical logic:
// Any change in one function must be carefully replicated in the other also
void sense_sweet(bool reverse) {
    if (reverse) 
    { live = 1; ground = 0; } 
    else 
    { live = 0; ground = 1; }
    pinMode(sweet_sensor[live], INPUT_PULLUP);
    pinMode(sweet_sensor[ground], OUTPUT);
    digitalWrite(sweet_sensor[ground], LOW);  // make it act as the ground pin
    for (int i=0; i<10; i++);   // stabilize the spike ?
    sweet_overflow = !digitalRead(sweet_sensor[live]);
    pinMode(sweet_sensor[live], OUTPUT);
    digitalWrite(sweet_sensor[live], LOW);
    digitalWrite(sweet_led, !sweet_overflow);    
}

// NOTE: sense_Sweet and sense_salt are twin functions with identical logic:
// Any change in one function must be carefully replicated in the other also
void sense_salt(bool reverse) {
    if (reverse) 
    { live = 1; ground = 0; } 
    else 
    { live = 0; ground = 1; }
    pinMode(salt_sensor[live], INPUT_PULLUP);
    pinMode(salt_sensor[ground], OUTPUT);
    digitalWrite(salt_sensor[ground], LOW);  // make it act as the ground pin
    for (int i=0; i<10; i++);   // stabilize the spike ?
    salt_overflow = !digitalRead(salt_sensor[live]);
    pinMode(salt_sensor[live], OUTPUT);
    digitalWrite(salt_sensor[live], LOW);
    digitalWrite(salt_led, !salt_overflow);    
}

void init_hardware() {
    pinMode(sweet_led, OUTPUT);
    pinMode(beeper, OUTPUT);    
    digitalWrite (beeper, HIGH);// beeper is active low
    digitalWrite (sweet_led, HIGH); // active low
    pinMode(salt_led, OUTPUT);
    digitalWrite (salt_led, HIGH); // active low    
    blink();
}

void init_serial () {
    #ifdef ENABLE_DEBUG
        Serial.begin(BAUD_RATE); 
        #ifdef VERBOSE_MODE
          Serial.setDebugOutput(true);
        #endif
        Serial.setTimeout(250);
    #endif    
    SERIAL_PRINTLN("\n***  Water level sensor starting...  ***\n"); 
    SERIAL_PRINT("FW version: ");
    SERIAL_PRINTLN(FIRMWARE_VERSION);
}

void check_for_updates() {
    SERIAL_PRINTLN ("\n------  checking for FW updates... ---------\n");
    int result = O.check_and_update();  // if there was an update, this will restart 8266
    SERIAL_PRINT ("OtaHelper: response code: ");   // if the update failed
    SERIAL_PRINTLN (result);
    beep();  // 'no updates, and ready again' signal
}

void beep() {  // this does not depend on Timer being initialized
    digitalWrite(beeper, LOW);  // active low
    delay(500);  
    digitalWrite(beeper, HIGH);    
}

void beep1() {
    if (!sweet_muted && sweet_overflow)
        T.oscillate(beeper, 200, HIGH, 4);  // the cycle starts up in LOW state,...
//    if (sweet_enabled && sweet_overflow)
//        M.publish_command(sweet_off_command);
}

void beep2() {        
    if (!salt_muted && salt_overflow)
        T.oscillate(beeper, 500, HIGH, 2);   // the cycle ends up in a steady state of HIGH    
//    if (salt_enabled && salt_overflow)
//        M.publish_command(salt_off_command);        
}

void blink () {
  for (int i=0; i<4; i++) {
      digitalWrite (sweet_led, LOW);
      digitalWrite (salt_led, LOW);
      delay(250);
      digitalWrite (sweet_led, HIGH);
      digitalWrite (salt_led, HIGH);      
      delay(250);      
  }
}
