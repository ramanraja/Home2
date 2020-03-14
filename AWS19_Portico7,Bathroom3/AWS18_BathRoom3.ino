/*-------------------------------------------------------------------------------------------------------------
   Bathroom light controller using circuler PCB.
   New in this version: Command Handler class introduced.
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
 Bath room controller
 Author: ra@g.in
 Firmware version #: see config.h 
 Date: March 2020
 */
#include "main.h"

// hardware
int PRIMARY_LIGHT = 0;    // the bath room controller has only one relay at D1
bool relay_status[] = {0, 0}; // for now, keep the extra relay!
// GPIO pins in Node MCU nomenclature
byte relay_pin[] =  {D1,D2}; //GPIO 5 and GPIo 4 for prod.  //{D0,D4} GPIO 16 and GPIO 2 for testing 
byte led1 = D3;  // GPIO 0; NOTE: now, GPIO 0 cannot be used for the push button !
byte led2 = D9;  // Rx
const int pir = D6;
const int radar = D5;
const int ldr   = A0;  
const int temhum = D7; // DHT22
bool manual_override = false; // to disable sensor based light control, set this to true

//---------------  application specific --------------------------------
const int NUM_RELAYS = 1;
int tick_interval = 100;  // millisec
int check_interval = 10000;  // 10 sec; for occupancy based light controller only
int sensor_interval = 60000;   // 1 minute
// for occupancy based light control only:
int MAX_BUCKET = 6;  // 6x10sec = 1 minute
int leaky_bucket = MAX_BUCKET; // auto switch off timer
bool is_night = true;  // globally updatd by the timer server client
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
    pClient = aws.getPubSubClient(); 
    cmd.init(&C, pClient);
    O.init(&C, pClient);  // OTA; this needs the pubsub client from AWS class
    blink2();  // all initializations completed
    
    T.every(tick_interval, check_movement);
    T.every(sensor_interval, read_sensors);
    
    //---------------  application specific --------------------------------
    T.every(check_interval, check_occupation);  // for occupancy based light controller only
    //--------------- end application specific --------------------------------
}

void loop() {
  T.update();
  // ASSUMPTION: if wifi connection is lost, it will auto connect again without our code
  if (WiFi.status() == WL_CONNECTED) 
      aws.update();
  // TODO: else, wait for  a time out and then reboot
  // TODO: if command_received flag is set, hanlde the command here
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
// for motion sensor based light control
bool occupied = false;
bool pir_status, radar_status;
void check_movement() {
    pir_status = digitalRead(pir);  
    radar_status = digitalRead(radar);
    digitalWrite (led1, !pir_status);  // active low
    digitalWrite (led2, !radar_status);
    // TODO: increment hit counts (after implementing Button interface)
    if (!(pir_status || radar_status))
        return;
    if (manual_override)
        return;
    if (!is_night)     
       return;
    leaky_bucket = MAX_BUCKET;  // keep filling the bucket
    if (!occupied) {
        occupied = true; // is there a subtle bug here?
        switch_light_on();
    }
}

// periodically checks if there has been a motion sensor hit;
// if not, switches off the light
void check_occupation() {
    if (!is_night)  
        return;      
    if (manual_override) 
        return;
    leaky_bucket--;
    if (leaky_bucket > 0) // not yet timed out
        return;
    leaky_bucket = MAX_BUCKET; // to avoid negative run off!
    if (occupied) {
        occupied = false;
        switch_light_off();
    }
}

 // check every 5 minutes if it is day or night, and just set a flag; the
 // actual switch on/off will be handled by the motion sensor and leaky bucket timer.
int sensor_reading_count = 0;
int STATUS_INTERVAL = 5;   // once in 5 minutes
void read_sensors() {
    // TODO: read light, temp and humidity
    sensor_reading_count++;
    if (sensor_reading_count < STATUS_INTERVAL) 
        return;
    sensor_reading_count = 0; 
   ////// operate_lights (false); // this is only for time based automatic on/off
    short time_code = aws.is_night_time();  // This returns a ternary: day,night,unknown
    if (time_code == TIME_NIGHT) 
        is_night = true;
    else if (time_code == TIME_DAY) {
        is_night = false;
        // if the light was on when you transition to day time, switch it off
        if (occupied && !manual_override) {  
            occupied = false;
            switch_light_off();
        }      
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
  //SERIAL_PRINTLN("light is ON"); // this will be called too frequently 
  //////send_status();   // do not flood the server !
}

void switch_light_off() {
  digitalWrite (relay_pin[PRIMARY_LIGHT], OFF);
  relay_status[PRIMARY_LIGHT] = 0;
  //SERIAL_PRINTLN("light is OFF");   // this will be called too frequently 
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
