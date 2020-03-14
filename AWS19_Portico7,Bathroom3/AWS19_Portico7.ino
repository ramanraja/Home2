/*-------------------------------------------------------------------------------------------------------------
   New in this version: command handler class introduced.
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
> openssl s_client -connect as44o8iem3pn1-ats.iot.us-east-2.amazonaws.com:8443   -CAfile root-CA.crt 
  -cert portico_thing.cert.pem   -key portico_thing.private.key
  *** NOTE: 8266 subscriber is unable to receive the long MQTT messages (>100 bytes) from AWS. Use short custom messages ***
------------------------------------------------------------------------------------------------------------*/
/*
 Portico controller
 Author: ra@g.in
 Firmware version #: see config.h 
 Date: March 2020
 */
#include "main.h"

// hardware
int PRIMARY_LIGHT = 1;    // the main portico tube light is 1, at D2. (the side bulb is 0 at D1)
bool relay_status[] = {0, 0};
// GPIO pins in Node MCU nomenclature
byte relay_pin[] =  {D1,D2}; //GPIO 5 and GPIo 4 for prod.  //{D0,D4} GPIO 16 and GPIO 2 for testing 
byte led1 = D3;  // GPIO 0; NOTE: now, GPIO 0 cannot be used for the push button !
byte led2 = D9;  // Rx
const int pir = D6;
const int radar = D5;
const int ldr   = A0;  
const int temhum = D7; // DHT22
bool manual_override = false; // for remote commands, set this to true

//---------------  application specific --------------------------------
const int NUM_RELAYS = 2;
int tick_interval = 100;  // millisec
int sensor_interval = 60000;   // 1 minute
//--------------- end application specific --------------------------------

// helper classes
Config C;
OtaHelper O;
Timer T;
AWS aws;
MyFiManager myfi;
CommandHandler cmd(NUM_RELAYS);
PubSubClient* pClient;

//-------------------------------------------------------------
// this is invoked from MQTT callback
void notify_command (const char* command) {
    //SERIAL_PRINTLN(command);
    // there is no manual override_check here; remote commands will work even in auto mode.
    cmd.handle_command(command); // TODO: this runs on the MQTT thread; move this to the main loop
}

byte setManualOverride (bool set_to_manual_mode) {
    manual_override = set_to_manual_mode;
    return ((byte)manual_override);
}

byte getManualOverride () {
    return ((byte)manual_override);
}

const bool* getStatus() {
    return ((const bool *)relay_status);
}
//-------------------------------------------------------------
void setup() {
  init_serial();
  init_hardware();
  C.init();
  C.dump();
  digitalWrite(led2, LOW); // indicates start of wifi config mode
  myfi.init();  // this will start the web portal if no WiFi credentials are found
  digitalWrite(led2, HIGH); // end of config mode
  // TODO: what will the web portal do, if the wifi modem is temporarily down ? ****
  
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
    //---------------  application specific --------------------------------
    operate_lights (true);  // this needs time server to have been initialized
    //--------------- end application specific --------------------------------

    pClient = aws.getPubSubClient(); 
    cmd.init(&C, pClient);
    O.init(&C, pClient);  // OTA; this needs the pubsub client from AWS class
    blink2();  // all initializations completed
    
    T.every(tick_interval, check_movement);
    T.every(sensor_interval, read_sensors);
}

void loop() {
  T.update();
  // ASSUMPTION: if wifi connection is lost, it will auto connect after some time
  if (WiFi.status() == WL_CONNECTED)  // what if wifi is present, but internet fails ?
      aws.update();   // this is not enough; check for network failure and re-initialize aws?
  // TODO: else, wait for  a time out and then reboot
  // TODO: if command_received flag is set, handle the command here, in this thread
}

void enter_fiasco_mode(){
    SERIAL_PRINTLN ("\n\n**** FATAL ERROR ! Device security certificate not found ***");
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    Serial.end();
    pinMode(D0, OUTPUT);  // hard coded: Node MCU built in LEDs
    pinMode(D4, OUTPUT);
    static bool temp = 0;
    while (true)  {  // NOTE: infinite loop outside main loop *
      digitalWrite(D0, temp);   
      digitalWrite(D4, !temp); 
      temp = !temp;
      delay(1000);
    }
}

//---------------  application specific --------------------------------
// in this version, this just serves as a check on the pir and radar
void check_movement() {
  digitalWrite(led1, !digitalRead(pir));
  digitalWrite(led2, !digitalRead(radar));  
}

int sensor_reading_count = 0;
int STATUS_INTERVAL = 5;   // once in 5 minutes
void read_sensors() {
    // TODO: read light, temp and humidity
    sensor_reading_count++;
    if (sensor_reading_count < STATUS_INTERVAL) 
        return;
    sensor_reading_count = 0; 
    operate_lights (false);
} 

// check every 5 minutes if it is day or night, and switch on/off the light
void operate_lights (bool verbose) {
  short time_code = aws.is_night_time();  // This returns a ternary: day,night,unknown
  if (time_code == TIME_NIGHT) {
      switch_light_on();
      if (verbose) 
            SERIAL_PRINTLN ("Starting at night time.");
  }
  else if (time_code == TIME_DAY) {
      switch_light_off();    
      if (verbose)   
            SERIAL_PRINTLN ("Starting at day time.");
  }
  // there is a third condition, TIME_UNKNOWN, but we ignore it
}
//--------------- end application specific -----------------------------

// NOTE: the round PCB hardware can only transmit; the serial Rx line is taken away for an LED.
void init_serial() {
  #ifdef ENABLE_DEBUG
      Serial.begin(BAUD_RATE); 
      #ifdef VERBOSE_MODE
        Serial.setDebugOutput(true);
      #endif
      Serial.setTimeout(250);
  #endif    
  SERIAL_PRINTLN("\n\n ***************** AWS IoT X-509 client starting.. ******************\n\n");  
  SERIAL_PRINT("FW version: ");
  SERIAL_PRINTLN(FIRMWARE_VERSION);
}

void init_hardware() {
    for (int i=0; i<NUM_RELAYS; i++) {
    pinMode(relay_pin[i], OUTPUT);
    digitalWrite(relay_pin[i],OFF);  
  }  
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(pir, INPUT);
  pinMode(radar, INPUT);
  // TODO: start DHT22 and LDR
  blink1();
}
  
// these two are invoked by algorithm based on light level or time of the day    
void switch_light_on() {
  digitalWrite (relay_pin[PRIMARY_LIGHT], ON);
  relay_status[PRIMARY_LIGHT] = 1;
  //SERIAL_PRINTLN("Portico light is ON"); // this will be called once in 5 minutes!
  //////send_status();   // do not flood the server !
}

void switch_light_off() {
  digitalWrite (relay_pin[PRIMARY_LIGHT], OFF);
  relay_status[PRIMARY_LIGHT] = 0;
  //SERIAL_PRINTLN("Portico light is OFF");   // this will be called once in 5 minutes!
  //////send_status();   // do not flood the server !
}       
        
// these two are invoked by remote MQTT command
void relay_on (short relay_number) {
  SERIAL_PRINT ("Remote command: ON; Relay : ");
  SERIAL_PRINTLN (relay_number);  
  digitalWrite (relay_pin[relay_number], ON);
  relay_status[relay_number] = 1;
  cmd.send_status();
}

void relay_off (short relay_number) {
  SERIAL_PRINT ("Remote command: OFF; Relay : ");
  SERIAL_PRINTLN (relay_number);    
  digitalWrite (relay_pin[relay_number], OFF);
  relay_status[relay_number] = 0;
  cmd.send_status();
}
//-------------------------------------------------------------------------

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
    O.check_and_update();
    SERIAL_PRINTLN("Reached after OTA-check for updates."); // reached if the update fails
}

void blink1() {
  for (int i=0; i<8; i++) {
      digitalWrite (led1, LOW);  // active low, green LED, for PIR
      delay(50);
      digitalWrite (led1, HIGH);  // active low
      delay(50);               
  }
  //digitalWrite (led2, LOW); // let the red light be on till we connect to the Net
}

void blink2() {
  for (int i=0; i<4; i++) {
      digitalWrite (led2, LOW);  // active low, red LED, for radar
      delay(150);
      digitalWrite (led2, HIGH);  // active low 
      delay(150);               
  }
}

#define BLINK_COUNT  6
void blinker(byte pin_number) {
    if (pin_number==1)
        T.oscillate(led1, 300, HIGH, BLINK_COUNT);
    else if (pin_number==2)
        T.oscillate(led2, 300, HIGH, BLINK_COUNT);
    else // error
        SERIAL_PRINTLN("Invalid LED number");        
}
