/*-------------------------------------------------------------------------------------------------------------
   ESP8266 AWS IoT  bath room light controller;  
   For testing purposes, we set the group id temporarily to "G0".   
   If OTA from amazon fails, then it tries a local server. Run a Python OTA server on the local network.
   New in this version: 
   (1) utilities.cpp: A major overflow bug in safe_strncat fixed; safe_strncpy_slash newly introduced.
   (2) There was a deadly bug in config.cpp where 'firmware_server_prefix' had a slash
        at the end; the code added another slash, rendering the URL invalid. Now this is fixed. 
   (3) Remote get/set for all settings.txt parameters 
       NOTE: set() overwrites the parameter temporarily. To save it to Flash, download a new settings.txt file.
   TODO: some web interface (use WiFi Manager's getParam()) to see/edit config.txt and save
   TODO: increment pir/radar hit counts (after implementing Button interface)
   TODO: command handler runs on the MQTT thread; move this to the main loop
   The AWS class is the custodian of Time Server connection (TODO: create a separate TimeManager class)
   TODO: Start_wifi_manager_portal() with a push button
   TODO: in sevaral files we have hard coded values for NUM_RELAYS; introduce it as a variable 
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
   > openssl x509 -in portico_thing.cert.pem    -out  cert.der    -outform  DER
   > openssl rsa -in  portico_thing.private.key -out  private.der -outform  DER
   > openssl x509 -in root-CA.crt  -out  ca.der  -outform  DER
  
   Copy the .der files to a 'data' folder under your project and upload it to SPIFFS using tools/"ESP8266 Sketch Data Upload"
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
Created: 25 April 2020
*/
#include "main.h"

// if TLS certificates are missing, try downloading new firmware; (possibly Boto3.ino, renamed as bath.bin)
// if the following line is commented out, the device will freeze if the certificates are missng:
#define  OPEN_TRAP_DOOR

// helper classes
Config C;
Hardware hard;
OtaHelper ota; // TODO: create this only when needed ?
Timer T;
AWS aws;
MyFiManager myfi;
CommandHandler cmd;
PubSubClient* pClient;

enum { COMM_OK, COMM_BROKEN } comm_status;
#ifndef PORTICO_VERSION   
  int leaky_bucket = 10; // auto switch off timer duration, in minutes (defined in settings.h)
  int MAX_BUCKETS = 10;
  bool occupied = false;
  bool trigger_cue = false;  
#endif
bool is_night = true;   // Global variable; periodically updated using the Time Server  
bool pir_status, radar_status;

//-------------------------------------------------------------------------
// these are invoked from MQTT callback
void notify_command (const char* command) {
    SERIAL_PRINTLN(command);
    // there is no manual override_check here; remote commands will work even in auto mode.
    cmd.handle_command(command); // TODO: this runs on the MQTT thread; move this to the main loop
}

void notify_get_param (const char* param) {
    cmd.get_param(param); // TODO: this runs on the MQTT thread; move this to the main loop  
}

void notify_set_param (const char* param, const char *value){
  // TODO: this runs on the MQTT thread; move this to the main loop 
    bool result = C.set_param(param, value); // this returns true if there was an error
    if (result) {
        SERIAL_PRINTLN (F("--- SET Failed ---"));
        pClient->publish(C.mqtt_pub_topic, "{\"I\":\"SET-ERROR\"}");
    }
    else {
        SERIAL_PRINTLN (F("SET: OK"));
        pClient->publish(C.mqtt_pub_topic, "{\"C\":\"SET-OK\"}");    
    }
}
//-------------------------------------------------------------------------

void setup() {
    // Note that C.init is called *after* hard.init; so C.active_low is not available at the time of hardware.init()
    hard.init(&C, &T, &cmd);  // this initializes LEDs and serial port; all the following lines need serial port
    hard.blink1();   // this needs LEDs to be initialized
    if (!C.init())            // TLS certificates not found
        enter_fiasco_mode();  // this is an infinite loop **
    hard.release_all_relays();  // this uses the correctly initialized value of C.OFF
    C.dump();    

    check_day_or_night (true);  // initialize based on light; this will be overridden by Time Server
    SERIAL_PRINT (F("Light-based time: "));
    SERIAL_PRINTLN(is_night ? "NIGHT": "DAY");
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
    SERIAL_PRINTLN(F("[Main] Connecting to Wifi.."));
    hard.led_off(red);
    hard.led_on(green); // indicates start of wifi config mode
    bool wifi_result = myfi.init(&C);  // this will start the web portal if no WiFi credentials are found
    // At this point, the AP portal, if launched, has timed out and returned
    hard.led_off(green); // end of wifi config mode  
    if (wifi_result) {
        SERIAL_PRINTLN(F("[Main] Wifi connected"));
        return true;
    }
    hard.led_on(red); // wifi failure indicator
    delay(3000);
    hard.led_off(red);
    SERIAL_PRINTLN(F("\n[Main] -------- No Wifi connection! Just working as a motion sensor... ------"));

    // The wifi manager config portal times out
    WiFi.softAPdisconnect(true); // close the portal; otherwise the AP lingers on **
    WiFi.mode(WIFI_OFF);  // Prevents reconnection issue (taking too long to connect) ***
    delay(1000);
    // it is important to set STA mode: https://github.com/knolleary/pubsubclient/issues/138
    WiFi.mode(WIFI_STA); 
    wifi_set_sleep_type(NONE_SLEEP_T);  // revisit & understand this
    //wifi_set_sleep_type(LIGHT_SLEEP_T);   
        
    return false;
}
 
bool init_cloud() {
    SERIAL_PRINTLN(F("\nConnecting to AWS cloud..."));
    int result = aws.init(&C, false);  // this initializes Time Server, needed for the next two steps; 
    switch (result) {
      case AWS_CONNECT_SUCCESS:
        SERIAL_PRINTLN(F("Connected to AWS successfully."));
        break;
      case TLS_CERTIFICATE_FAILED:
        enter_fiasco_mode();  // this is an infinite loop
        return false;  // will not reach here
        break;    
      case TIME_SERVER_FAILED: 
        SERIAL_PRINTLN(F("Time server failed."));    
        return false;  // NOTE: you cannot connect to AWS without time server 
        break;
      case AWS_CONNECT_FAILED:
        SERIAL_PRINTLN(F("Could not connect to AWS."));
        return false;
        break;
    }  
    pClient = aws.getPubSubClient(); 
    cmd.init(&C, pClient, &hard);
    ota.init(&C, pClient);  // OTA; this needs the pubsub client from AWS class    
    // priming read, based on time server
    check_day_or_night(false);  
    SERIAL_PRINT (F("Time server-based time: "));
    SERIAL_PRINTLN(is_night ? "NIGHT": "DAY");    
    ////cmd.send_status(); status will be known only after the main loop starts
    return true;
}

void init_timers() {
    T.every(C.tick_interval, tick);
    T.every(C.sensor_interval, one_minute_logic);
#ifndef PORTICO_VERSION    
    T.every(C.check_interval, ten_second_logic);  // leaky bucket; for occupancy monitor only  
#endif    
    SERIAL_PRINTLN(F("Software timers initialized."));
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
    SERIAL_PRINTLN(F("\n[Main] No WiFi, so rebooting...")); 
    hard.reboot_esp(); 
}

void reset_wifi() {
    SERIAL_PRINTLN(F("\n*** ERASING ALL WI-FI CREDENTIALS ! ***"));   
    SERIAL_PRINTLN(F("You must configure WiFi after restarting"));
    pClient->publish(C.mqtt_pub_topic, "{\"I\":\"Erasing all WiFi credentials !\"}");
    delay(500);
    pClient->publish(C.mqtt_pub_topic, "{\"C\":\"Configure WiFi after restarting\"}");  
    myfi.erase_wifi_credentials(true); // true = reboot ESP
}

void check_for_updates() {
    SERIAL_PRINTLN(F("---Firmware update command received---"));
    ota.check_and_update();
    SERIAL_PRINTLN(F("Reached after OTA-check for updates.")); // reached if the update fails
}
 
// security certificates not found: freeze the hardware
void enter_fiasco_mode(){
    SERIAL_PRINTLN(F("\n\n**** FATAL ERROR ! Device security certificate not found ***"));
#ifdef OPEN_TRAP_DOOR    
    delay(30000);
    check_for_updates();
#else
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    Serial.end();
    hard.infinite_loop(); // *** this is an infinite loop outside main loop ***
    // here we can put the ESP to sleep permanently, but our approach keeps blinking the 2 LEDs,
    // indicating that it is in certificate-failure lock down status
#endif    
}

bool is_night_now() {
    return is_night;
}

#ifndef PORTICO_VERSION 
bool is_occupied() { 
    return occupied;  // this is valid only for Bathroom version
}
#endif 

// Finds if it is day or night, and stores the status in the global variable is_night
// In the case of automatic portico light, it also switches on/off the light
short time_code;
void check_day_or_night (bool light_based) {
    if (light_based)
        time_code = hard.is_night_time();  // This returns a ternary: day,night,unknown
    else    
        time_code = aws.is_night_time();   // This *assumes* aws and time server are working
           
    if (time_code == TIME_NIGHT)  {
        is_night = true;
#ifdef PORTICO_VERSION         
        hard.primary_light_on(); 
#endif          
    }
    else if (time_code == TIME_DAY) {
        is_night = false;
#ifdef PORTICO_VERSION         
        hard.primary_light_off(); 
#endif          
    }
    // else keep relay status unchanged  
}

// this will be called once in 5 minutes if there is no cloud connectivity, ie, comm_status != COMM_OK
 void repair_comm() {
     SERIAL_PRINTLN(F("Atempting to repair communication.."));
     if (WiFi.status() != WL_CONNECTED) {  // to see if wifi auto connect has worked
        SERIAL_PRINTLN(F("No wifi !"));
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

void tick() {
    pir_status = hard.showPirStatus(); // this displays it on the LED and also returns the status 
    radar_status = hard.showRadarStatus(); 
#ifndef PORTICO_VERSION    
    // TODO: increment hit counts (after implementing Button interface)
    if (!(pir_status || radar_status))  // no motion detected, nothing to do
        return;
    if ((!is_night) || cmd.manual_override)
       return;
    leaky_bucket = MAX_BUCKETS;  // keep filling the leaky bucket, only during night and in auto mode
    if (occupied)  // already the light is on, so just refill the bucket and return 
        return;
     trigger_cue = pir_status;   
     if (C.radar_triggers)
        trigger_cue &= radar_status;
     if (trigger_cue) {
        occupied = true; // there is a subtle bug in this line:
        // the occupied flag and relay_status can diverge if you manually operate the relay.        
        hard.primary_light_on();
        SERIAL_PRINTLN(F("Room occupied. Switching ON."));
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
        SERIAL_PRINTLN(F("Good morning ! I am forcing the light off.."));
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
        SERIAL_PRINTLN(F("Room is vacant. Switching OFF."));
    }
}
#endif
