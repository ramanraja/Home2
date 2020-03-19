/*-------------------------------------------------------------------------------------------------------------
   New in this version: Temperature, humidity and light are read and transmitted.
   TODO: move all configurable items to a file settings.h
   ESP8266 AWS IoT portico light controller; 
   Code based on Evandro Luis Copercini:
   https://github.com/copercini/esp8266-aws_iot/blob/master/examples/mqtt_x509_DER/mqtt_x509_DER.ino
   Dependencies:
   https://github.com/esp8266/arduino-esp8266fs-plugin
   https://github.com/arduino-libraries/NTPClient
   https://github.com/knolleary/pubsubclient 
   Create a new device in AWS IoT,  
   give it AWS IoT permissions and 
   download its certificates (in pem format).
   OTA files can be accessed from S3 now. They are of the form:
   https://my-bucket.s3.us-east-2.amazonaws.com/my-folder/my-file.jpg
  Use OpenSSL (install it for Windows) to convert the certificates to DER format:
  > openssl x509 -in portico_thing.cert.pem    -out  cert.der    -outform  DER
  > openssl rsa -in  portico_thing.private.key -out  private.der -outform  DER
  > openssl x509 -in root-CA.crt  -out  ca.der  -outform  DER
  copy the der files to a data' folder under your project and upload it to SPIFFS using tools/"ESP8266 Sketch Data Upload"
  To upload to SPIFF, use arduino-esp8266fs-plugin:
  https://github.com/esp8266/arduino-esp8266fs-plugin/releases/tag/0.5.0
  and put the jar file in
  C:\Users\raman\Documents\Arduino\tools\ESP8266FS\tool\esp8266fs.jar
  Upload the 3 certificates in /data folder to SPIFF using tools/"ESP8266 Sketch Data Upload" menu
  To test if your device's certificates are working and you can connect to AWS through SSL:
> openssl s_client -connect xxxxxx-ats.iot.us-east-2.amazonaws.com:8443   -CAfile root-CA.crt 
  -cert portico_thing.cert.pem   -key portico_thing.private.key
  *** NOTE: 8266 subscriber is unable to receive the long MQTT messages (>100 bytes) from AWS. Use short custom messages ***
------------------------------------------------------------------------------------------------------------*/
/*
 Portico controller
 Author: rajavaidya@gmail.com
 Firmware version #: see config.h 
 Date: March 2020
 */
 
#include "main.h"

// helper classes
Config C;
Hardware hard;
OtaHelper ota;
Timer T;
AWS aws;
MyFiManager myfi;
CommandHandler cmd;
PubSubClient* pClient;

#define WIFI_FAIL_RESTART_INTERVAL    5*60*1000L  // 5 minuts

//-------------------------------------------------------------------------
// this is invoked from MQTT callback
void notify_command (const char* command) {
    //SERIAL_PRINTLN(command);
    // there is no manual override_check here; remote commands will work even in auto mode.
    cmd.handle_command(command); // TODO: this runs on the MQTT thread; move this to the main loop
}
//-------------------------------------------------------------------------

void setup() {
  hard.init(&C, &T, &cmd); // this initializes serial port also
  C.init(); 
  C.dump();  // this requires serial port to have been initialized

  hard.led_off(red);
  hard.led_on(green); // indicates start of wifi config mode
  bool wifi_result = myfi.init();  // this will start the web portal if no WiFi credentials are found
  hard.led_off(green); // end of config mode  
  if (!wifi_result) {
      hard.led_on(red); // wifi failure indicator
      // TODO: if the wifi modem is temporarily down you will reach here!
      // restart the ESP ? continue without network ?
      delay(3000);
      hard.led_off(red);
      SERIAL_PRINTLN("\n-------- No Wifi connection! Just working as a motion sensor... ------");
      T.every(C.tick_interval, check_movement);  // TODO: revisit
      T.after (WIFI_FAIL_RESTART_INTERVAL, restart_ESP); // TODO: remove this; instead, just check if wifi is available and initialize AWS.
      return;
  }
  SERIAL_PRINTLN ("Connecting to AWS cloud...");
  int result = aws.init(&C, true);  // this initializes Time Server, needed for the next two steps; 
  switch (result) {
    case AWS_CONNECT_SUCCESS:
      SERIAL_PRINTLN("Connected to AWS successfully.");
      break;
    case CERTIFICATE_FAILED:
      enter_fiasco_mode();  // this is an infinite loop
      break;    
    case TIME_SERVER_FAILED: // TODO: you cannot proceed with AWS connection without time server !
      SERIAL_PRINTLN("Time server failed.");    
      break;
    case AWS_CONNECT_FAILED:
      SERIAL_PRINTLN ("Could not connect to AWS.");
      break;
    }
    //---------------  application specific ----------------------------------
    
    // initial status of portico light at startup
    operate_lights (true);  // this needs time server to have been initialized
    
    //--------------- end application specific --------------------------------

    pClient = aws.getPubSubClient(); 
    cmd.init(&C, pClient, &hard);
    ota.init(&C, pClient);  // OTA; this needs the pubsub client from AWS class
    hard.blink2();  // all initializations completed
    
    T.every(C.tick_interval, check_movement);
    T.every(C.sensor_interval, check_sensors);
}

void loop() {
  T.update();
  // ASSUMPTION: if wifi connection is lost, it will auto connect after some time
  if (WiFi.status() == WL_CONNECTED)  // what if wifi is present, but internet fails ?
      aws.update();   // TODO: this is not enough; check for network failure and re-initialize aws
  // TODO: else, wait for  a time out and then reboot
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
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    Serial.end();
    hard.infinite_loop(); // *** this is an infinite loop outside main loop ***
}
//---------------  application specific ----------------------------------------

// in this version, this just serves as a check on the pir and radar
void check_movement() {
  hard.showPirStatus();
  hard.showRadarStatus(); 
}

// this is called every 1 minute by the Timer
void check_sensors() {
    if (hard.read_sensors()) // returns true if it is time to send a status report
        operate_lights (false);
} 

// switch on/off portico light depending on time of the day
void operate_lights (bool verbose) {
  short time_code = aws.is_night_time();  // This returns a ternary: day,night,unknown
  if (time_code == TIME_NIGHT) {
      hard.primary_light_on();
      if (verbose) 
            SERIAL_PRINTLN ("Time: NIGHT");
  }
  else if (time_code == TIME_DAY) {
      hard.primary_light_off();    
      if (verbose)   
            SERIAL_PRINTLN ("Time: DAY");
  }
  else {
      // if TIME_UNKNOWN the light stays in its previous state
      SERIAL_PRINTLN ("Time: UNKNOWN");  
  }
  // do NOT send any MQTT message from here, since pubsub client may not have been initialized
}
//--------------- end application specific --------------------------------------
