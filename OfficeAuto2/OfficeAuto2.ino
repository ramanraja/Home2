/*-------------------------------------------------------------------------------------------------------------
   ESP8266 AWS IoT  portico  and bath room light controller; Cloned from AWS21_BathRoom5 and enhanced with
   code from Boot3
   New in this version:  
      1) Single consolidated code base for Bathroom and Portico controller
      2) If time server connectivity fails, use LDR to decide day or night
      3) Downloads TLS certificate and config.txt files in response to an MQTT command
   For testing purposes, we set the group id temporarily to "G0".  Later you can delete these rows from DB.
   If OTA from amazon fails, then it tries a local server. Run a Python OTA server on the local network.
   TODO: F() for all string constants
   TODO: some web interface (modify WiFi Manager?) to see/edit config.txt and upload to SPIFF
   TODO: Remotely get/set config items through MQTT/ download config.txt
   ---------------------------------------------------------------------------------
   Difference between bathroom and portico controllers:
   Change the app name, app_id etc.  -- settings.h, config.txt
   URL of OTA files -- this is automatically composed from app_id
   Bathroom has only one relay at 1 (pin D2). -- settings.h, config.txt
   Business logic in main .ino : 
     Check time every 5 minutes. If it is night time, just set a flag is_night=true.
     If is_night, and we are not in manual override mode, any movement triggers the light on.
     A leaky bucket starts counting down, and eventually switches off the light. For this, we
     introduce a new Timer controlled function: T.every(check_interval, ten_second_logic); -- main .ino
     Additional variables like AUTO_OFF_TIME_MIN, leaky_bucket, is_night etc -- Hardware.h
   ---------------------------------------------------------------------------------
   Concept based on Evandro Luis Copercini:
   https://github.com/copercini/esp8266-aws_iot/blob/master/examples/mqtt_x509_DER/mqtt_x509_DER.ino
   Dependencies:
   https://github.com/esp8266/arduino-esp8266fs-plugin
   https://github.com/arduino-libraries/NTPClient
   https://github.com/knolleary/pubsubclient 
   
   Cloud configuration:
   Create a new device in AWS IoT,  
   give it AWS IoT permissions and download its certificates (in pem format).
   OTA files can be accessed from S3 now. They are of the form:
   https://my-bucket.s3.us-east-2.amazonaws.com/my-folder/my-file.jpg
   
   Transforming the certificates:
   Use OpenSSL (install it for Windows) to convert the certificates to DER format:
   > openssl x509 -in my_thing.cert.pem    -out  cert.der    -outform  DER
   > openssl rsa -in  my_thing.private.key -out  private.der -outform  DER
   > openssl x509 -in root-CA.crt  -out  ca.der  -outform  DER
  
   Copy the der files to a data' folder under your project and upload it to SPIFFS using tools/"ESP8266 Sketch Data Upload"
   To upload to SPIFF, use arduino-esp8266fs-plugin:
   https://github.com/esp8266/arduino-esp8266fs-plugin/releases/tag/0.5.0
   and put the jar file in
   C:\Users\<your name>\Documents\Arduino\tools\ESP8266FS\tool\esp8266fs.jar
   http://arduino.esp8266.com/Arduino/versions/2.0.0/doc/filesystem.html
   Upload the 3 certificates in /data folder to SPIFF using tools/"ESP8266 Sketch Data Upload" menu
   
   To test if your device's certificates are working and you can connect to AWS through SSL:
   > openssl s_client -connect xxxxx-ats.iot.us-east-2.amazonaws.com:8443   -CAfile root-CA.crt 
           -cert portico_thing.cert.pem   -key portico_thing.private.key
   NOTE: By default, 8266 subscriber is unable to receive the long MQTT messages (>100 bytes) from AWS. 
        So use short custom messages 
------------------------------------------------------------------------------------------------------------*/
/*
 Time of the day/ motion sensor based automatic light controller
 Author: rajavaidya@gmail.com
 Firmware version: see settings.h 
Created: 3 April 2020
*/
 
#include "main.h"
#define  OPEN_TRAP_DOOR

// helper classes
Config C;
Hardware hard;
OtaHelper ota;
Timer T;
AWS aws;
MyFiManager myfi;
CommandHandler cmd;
PubSubClient* pClient;

enum { COMM_OK, COMM_BROKEN } comm_status;
#ifndef PORTICO_VERSION   
  int leaky_bucket = 10; // auto switch off timer duration, in minutes (defined in settings.h)
  int MAX_BUCKETS = 10;
  bool is_night = true;   // Global variable; periodically updated using the Time Server  
 #endif
//-------------------------------------------------------------------------
// this is invoked from MQTT callback
void notify_command (const char* command) {
    //SERIAL_PRINTLN(command);
    // there is no manual override_check here; remote commands will work even in auto mode.
    cmd.handle_command(command); // TODO: this runs on the MQTT thread; move this to the main loop
}
//-------------------------------------------------------------------------

void setup() {
    hard.init(&C, &T, &cmd);  // this initializes LEDs and serial port; all the following lines need serial port
    hard.blink1();   // this needs LEDs to be initialized
    if (!C.init())            // TLS certificates not found
        enter_fiasco_mode();  // this is an infinite loop **
    C.dump();    
    check_day_or_night (true);  // initialize based on light; will be overridden by Time Server
#ifndef PORTICO_VERSION       
    MAX_BUCKETS = C.get_auto_off_ticks();
#endif    
    comm_status = COMM_BROKEN;
    if (init_wifi())
        if (init_cloud())
            comm_status = COMM_OK;
    if (comm_status == COMM_OK)
        hard.blink2(); // green
    else      
        hard.blink3();  // red
    init_timers();       
    print_heap(); 
}
    
bool init_wifi() {    
    hard.led_off(red);
    hard.led_on(green); // indicates start of wifi config mode
    bool wifi_result = myfi.init();  // this will start the web portal if no WiFi credentials are found
    hard.led_off(green); // end of wifi config mode  
    if (wifi_result) {
        SERIAL_PRINTLN("Wifi connected");
        return true;
    }
    hard.led_on(red); // wifi failure indicator
    delay(3000);
    hard.led_off(red);
    SERIAL_PRINTLN("\n-------- No Wifi connection! Just working as a motion sensor... ------");
    return false;
}
 
bool init_cloud() {
    SERIAL_PRINTLN ("\nConnecting to AWS cloud...");
    int result = aws.init(&C, true);  // this initializes Time Server, needed for the next two steps; 
    switch (result) {
      case AWS_CONNECT_SUCCESS:
        SERIAL_PRINTLN("Connected to AWS successfully.");
        break;
      case TLS_CERTIFICATE_FAILED:
        enter_fiasco_mode();  // this is an infinite loop
        return false;  // will not reach here
        break;    
      case TIME_SERVER_FAILED: 
        SERIAL_PRINTLN("Time server failed.");    
        return false;  // NOTE: you cannot connect to AWS without time server 
        break;
      case AWS_CONNECT_FAILED:
        SERIAL_PRINTLN ("Could not connect to AWS.");
        return false;
        break;
    }  
    pClient = aws.getPubSubClient(); 
    cmd.init(&C, pClient, &hard);
    ota.init(&C, pClient);  // OTA; this needs the pubsub client from AWS class    
    // priming read, based on time server
    check_day_or_night(false);  
    ////cmd.send_status(); status will be known only after the main loop starts
    return true;
}

void init_timers() {
    T.every(C.tick_interval, tick);
    T.every(C.sensor_interval, one_minute_logic);
#ifndef PORTICO_VERSION    
    T.every(C.check_interval, ten_second_logic);  // leaky bucket; TODO: for occupancy monitor only  
#endif    
    SERIAL_PRINTLN("Timers initialized.");
}
  
void loop() {
    T.update();
    // ASSUMPTION: if wifi connection is lost, it will auto connect after some time
    if (WiFi.status()==WL_CONNECTED && comm_status==COMM_OK)   
        aws.update();   
    // TODO: if command_received flag is set, handle the command here, in this thread
}
//-------------------------------------------------------------------------------------------------

void restart_ESP () {
    SERIAL_PRINTLN("\n[Main] No WiFi, so rebooting..."); 
    hard.reboot_esp(); 
}

void reset_wifi() {
    SERIAL_PRINTLN("\n*** ERASING ALL WI-FI CREDENTIALS ! ***");   
    SERIAL_PRINTLN("You must configure WiFi after restarting");
    pClient->publish(C.mqtt_pub_topic, "{\"I\":\"Erasing all WiFi credentials !\"}");
    delay(500);
    pClient->publish(C.mqtt_pub_topic, "{\"C\":\"Configure WiFi after restarting\"}");  
    myfi.erase_wifi_credentials(true); // true = reboot ESP
}

void check_for_updates() {
    SERIAL_PRINTLN("---Firmware update command received---");
    ota.check_and_update();
    SERIAL_PRINTLN("Reached after OTA-check for updates."); // reached if the update fails
}
 
// security certificates not found: freeze the hardware
void enter_fiasco_mode(){
    SERIAL_PRINTLN ("\n\n**** FATAL ERROR ! Device security certificate not found ***");
#ifdef OPEN_TRAP_DOOR    
    delay(30000);
    check_for_updates();
#else
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    Serial.end();
    hard.infinite_loop(); // *** this is an infinite loop outside main loop ***
#endif    
}
/*
// Enable movement based light control only if it is night time:
// sets the global variable 'is_night'
void init_day_or_night () {
    short time_code = aws.is_night_time();  // This returns a ternary: day,night,unknown
    if (time_code == TIME_NIGHT)  {
        is_night = true;
        SERIAL_PRINTLN ("Starting the app during NIGHT..");          
    }
    else if (time_code == TIME_DAY) {
        is_night = false;
        SERIAL_PRINTLN ("Starting the app during DAY..");              
    }
    else {
        // if TIME_UNKNOWN, the light stays in its previous state
        SERIAL_PRINTLN ("Starting time is UNKNOWN");  
    }
    // do NOT send any MQTT message from here, since pubsub client may not have been initialized    
    // TODO: for automatic portico light controller, operate_lights() now
}
*/
// Finds if it is day or night, and stores the status in the global variable is_night
// In the case of automatic portico light, it also switches on/off the light
short time_code;
void check_day_or_night (bool light_based) {
    if (light_based)
        time_code = hard.is_night_time();  // This returns a ternary: day,night,unknown
    else    
        time_code = aws.is_night_time();   // This returns a ternary: 
#ifdef PORTICO_VERSION            
    if (time_code == TIME_NIGHT)  
        hard.primary_light_off(); 
    else if (time_code == TIME_DAY) 
        hard.primary_light_on(); 
    // else light unchanged
#else
    if (time_code == TIME_NIGHT)  
        is_night = true;
    else if (time_code == TIME_DAY) 
        is_night = false;
    // else status unchanged
#endif    
}

// this will be called once in 5 minutes if there is no cloud connectivity, ie, comm_status != COMM_OK
 void repair_comm() {
     if (WiFi.status() != WL_CONNECTED) {  // to see if wifi auto connect has worked
        SERIAL_PRINTLN("No wifi !");
        return;   // Do not launch the Config AP once in 5 minutes, just wait for auto connect!
     }
     if (init_cloud())
        comm_status = COMM_OK;  
     else    
        comm_status = COMM_BROKEN;
 }

// one_minute_logic() is called every 1 minute by the Timer
// Reads temperature, light etc. every one minute
// Contacts Time Server once in 5 minutes,  and stores it in the global variable is_night
void one_minute_logic() {
    bool time_for_status = hard.read_sensors(); //<- this returns true if it is time to send a status report, ie, 5 minutes elapsed
    if (!time_for_status)
        return;
    if (comm_status == COMM_OK) {  // if AWS and time server were properly initialized at the beginning
        check_day_or_night (false);  // this stores it in global variable is_night
    #ifndef PORTICO_VERSION 
        handle_transition();  // handle that one special case of day break
    #endif
        cmd.send_data();  // NOTE: pubsubclient.reconnect() is regulary called in aws.update()
    } else {    // status is COMM_BROKEN
        check_day_or_night (true);  // fall back on light based determination
        repair_comm();    // this is to repair AWS initialization failure in init_cloud() at the beginning 
    }
}

 

#ifndef PORTICO_VERSION
  bool occupied = false;
#endif
bool pir_status, radar_status;

void tick() {
    pir_status = hard.showPirStatus(); // this displays it on the LED and also returns the status 
    radar_status = hard.showRadarStatus(); 
#ifndef PORTICO_VERSION    
    // TODO: increment hit counts (after implementing Button interface)
    if (!(pir_status || radar_status))
        return;
    if (cmd.manual_override)  
        return;
    if (!is_night) 
       return;
    leaky_bucket = MAX_BUCKETS;  // keep filling the leaky bucket
    if (!occupied) {
       if (pir_status && radar_status) { // switch on only if both sensors fire
            occupied = true; // is there a subtle bug here? how manual_override interacts with the occupied flag ?
            hard.primary_light_on();
       }
    }  
#endif    
}

#ifndef PORTICO_VERSION  
// if you just entered day time, check for a special condition:
// if the light was ON when you transition from night to day, then force  it OFF now.
// this case will not be handled by ten_second_logic(), since is_night will be false.
void handle_transition() {    
    if (is_night) // if it is night, ten_second_logic() will take care of switching it OFF
        return;
    if (occupied && !cmd.manual_override) {  
        occupied = false;
        hard.primary_light_off();
        SERIAL_PRINTLN("Good morning ! I am forcing the light off..");
    }        
} 

// periodically (say, 10 seconds) check if there has been a motion sensor hit;
// if not, leak the bucket counter. If empty, switch off the light
void ten_second_logic() {
    if (!is_night)  
        return;      
    if (cmd.manual_override) 
        return;
    leaky_bucket--;
    if (leaky_bucket > 0) // not yet timed out
        return;
    leaky_bucket = MAX_BUCKETS; // to avoid negative run off!
    if (occupied) {
        occupied = false;
        hard.primary_light_off();
    }
}
#endif
