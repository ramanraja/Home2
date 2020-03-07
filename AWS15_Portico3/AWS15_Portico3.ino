/*
   New in this version: tell WiFi Manager to erase credentials;  subscribe to broadcast topic  xxxx/group_id/0. 
   ESP8266 AWS IoT portico light controller; this is initial skeleton code. To be extensively refactored.
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
  C:\Users\yourname\Documents\Arduino\tools\ESP8266FS\tool\esp8266fs.jar
  Upload the 3 certificates in /data folder to SPIFF using tools/"ESP8266 Sketch Data Upload" menu

  To test if your device's certificates are working and you can connect to AWS through SSL:
> openssl s_client -connect xxxxxxx-ats.iot.us-east-2.amazonaws.com:8443   -CAfile root-CA.crt 
  -cert portico_thing.cert.pem   -key portico_thing.private.key

  *** NOTE: 8266 subscriber is unable to receive the long MQTT messages (>100 bytes) from AWS. Use short custom messages ***
*/

#include "otaHelper.h"
#include "config.h"
#include <FS.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager
#include <NTPClient.h>        // https://github.com/arduino-libraries/NTPClient
#include <WiFiUdp.h>         // built in: /esp8266/Arduino/blob/master/libraries/ESP8266WiFi/
#include  <ArduinoJson.h>    // Blanchon, https://github.com/bblanchon/ArduinoJson
#include <Timer.h>            // https://github.com/JChristensen/Timer

#define  MQTT_KEEPALIVE   120   // override for PubSubClient keep alive, in seconds
//#define  MQTT_MAX_PACKET_SIZE  256  // PubSubClient default was 128, including headers
#include <PubSubClient.h>       // https://github.com/knolleary/pubsubclient 

void callback(char* topic, byte* payload, unsigned int length);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 55*360);  // UTC offset=5.5 hours * 3600 seconds
WiFiClientSecure espClient;
PubSubClient client(AWS_ENDPOINT, MQTT_PORT, callback, espClient); //set  MQTT port number to 8883 as per standard

Config C;
OtaHelper O;
Timer T;


// active high relay
#define  ON    1
#define  OFF   0
/*
// active low relay
#define  ON    0
#define  OFF   1
*/
#define SOFT_AP_SSID    "INTOF_AP"
#define SOFT_AP_PASSWD  "password"

char status_msg[MAX_MSG_LENGTH];   // Tx message

int PORTICO_LIGHT = 1;    // the main portico tube light is 1, at D2. (the side bulb is 0 at D1)
const int NUM_RELAYS = 2;
// For the circular box, the relays are connected to D1,D2.
byte relay_pin[] =  {D1,D2}; //GPIO 5 and GPIo 4 for prod.  //{D0,D4} GPIO 16 and GPIO 2 for testing 
byte led1 = D3;  // GPIO 0 NOTE: GPIO 0 cannot be used for the push button !
byte led2 = D9;  // Rx
bool relay_status[] = {0, 0};
const int pir = D6;
const int radar = D5;
const int ldr   = A0;  
const int temhum_sensor = D7; 
int current_hour = -1;
int current_minute = -1;
long last_time_stamp = 0;
bool is_night = false;
int tick_interval = 100;  // millisec
int check_interval = 10000;  // 10 sec
int sensor_interval = 60000;   // 1 minute
int MAX_BUCKET = 6;  // 6x10sec = 1 minute
int leaky_bucket = MAX_BUCKET; // auto switch off timer
bool manual_override = false; // for remote commands, set this to true
//-------------------------------------------------------------
// MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("]: ");
  Serial.print("Length: ");
  Serial.println(length);  
  // long messages are anyway dropped by PubSubClient !
  //if (length > MAX_MSG_LENGTH) return;
  /**/
  for (int i = 0; i < length; i++) 
      Serial.print((char)payload[i]);
  Serial.println();
  /**/
  //Serial.println ((char *)payload); // this prints a corrupted string ! TODO: study this further
  StaticJsonDocument<64> doc;  // stack
  //DynamicJsonDocument doc(64);   // heap
  DeserializationError error = deserializeJson(doc, (char *)payload); //  
  if (error) {
    Serial.print("Json deserialization failed: ");
    Serial.println(error.c_str());
    return;
  }
  if (!doc.containsKey("C")) {
    Serial.println("invalid command");
    return;
  }
  const char* cmd = doc["C"];
  Serial.print("Command: ");
  if (strlen(cmd) > 0) {
    Serial.println(cmd);
    handle_command(cmd);
  }   
}
//-------------------------------------------------------------
void setup() {
  init_serial();
  init_hardware();
  C.init();
  C.dump();
  if (!init_file_system()) {    
      enter_fiasco_mode();  // this is infinite loop
      return;  // will not reach here
  }
  init_wifi();  // this uses wifi manager
  init_time_client(); // this calls get_current_time to be used by the next day-night check
  init_light();  // set the status of light at startup; this needs time client to have been connected successfully
  O.init(&C, &client);  // OTA
  
  reconnect(false);  // priming connection to MQTT
  blink2();  
  T.every(tick_interval, check_movement);
//  T.every(check_interval, check_occupation);  
  T.every(sensor_interval, read_sensors);
}

void enter_fiasco_mode(){
    Serial.println ("\n\n**** FATAL ERROR ! Device security certificate not found ***");
    WiFi.mode(WIFI_OFF);
    WiFi.forceSleepBegin();
    pinMode(D0, OUTPUT);  // Node MCU built in LEDs
    pinMode(D4, OUTPUT);
    static bool temp = 0;
    while (true)  {  // NOTE: infinite loop outside main loop *
      digitalWrite(D0, temp);   
      digitalWrite(D4, !temp); 
      temp = !temp;
      delay(1000);
    }
}

void loop() {
  T.update();
  if (!client.connected()) {
     Serial.println("LOOP: client is not connected.");
     delay(5000);  // to avoid repeated triggers ?
     reconnect(false);  // do not restart
  }
  client.loop();
}

void check_movement() {
  digitalWrite(led1, !digitalRead(pir));
  digitalWrite(led2, !digitalRead(radar));  
}

void init_serial() {
  Serial.begin(115200);
  Serial.println("\n\n ***************** AWS IoT X-509 client starting.. ******************\n\n");    
  Serial.setDebugOutput(true);
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
  blink1();
}

void init_light() {
  check_day_or_night(); // this is done once at start up; then repeated every 10 minutes 
  if (is_night)
      Serial.println ("This is night time. Auto mode is ON");
  else 
      Serial.println ("This is day time. Auto mode is OFF");
  operate_light();  // set initial light condition at start up
}  
/*
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
//    if (!is_night)    // TODO: enable this after testing
//        return;
    leaky_bucket = MAX_BUCKET;  // keep filling the bucket
    if (!occupied) {
        occupied = true; // is there a subtle bug here?
        digitalWrite(relay_pin[PORTICO_LIGHT], ON); 
        Serial.println("Relay is ON...");
    }
}

void check_occupation() {
    if (manual_override) 
        return;
//    if (!is_night)  // TODO: enable this after testing
//        return;      
    leaky_bucket--;
    if (leaky_bucket > 0)
        return;
    leaky_bucket = MAX_BUCKET; // to avoid negative run off!
    if (occupied) {
        occupied = false;
        digitalWrite(relay_pin[PORTICO_LIGHT], OFF);
        Serial.println("Relay is OFF.");
    }
}
*/
int sensor_reading_count = 0;
int STATUS_INTERVAL = 10;   // once in 10 minutes

void read_sensors() {
    // TODO: read light, temp and humidity
    sensor_reading_count++;
    if (sensor_reading_count < STATUS_INTERVAL) 
        return;
    sensor_reading_count = 0;
    // NOTE: You  MUST call get_current_time before calling check_day_or_night
    get_current_time();
    check_day_or_night();  // this should be preceeded by get_Current_time
    operate_light();
}

void operate_light() {
    if (is_night)
        switch_light_on();
    else
        switch_light_off();
}

void switch_light_on() {
  digitalWrite (relay_pin[PORTICO_LIGHT], ON);
  relay_status[PORTICO_LIGHT] = 1;
  Serial.println("Portico light is ON");
}

void switch_light_off() {
  digitalWrite (relay_pin[PORTICO_LIGHT], OFF);
  relay_status[PORTICO_LIGHT] = 0;
  Serial.println("Portico light is OFF");  
}

void init_wifi() {
    Serial.println("Checking Wifi Manager...");
    wifi_manager_auto_connect();
}

void wifi_manager_auto_connect() {
    // WiFiManager local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    
    digitalWrite(led2, LOW);  // entering Wifi config mode
    // Uncomment and run this once, if you want to erase all the stored wifi credentials:
    //wifiManager.resetSettings();
    
    // if you want a custom ip for the wifi manager portal
    //wifiManager.setAPConfig(IPAddress(10,0,1,10), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
    Serial.println ("If this is the first time, we will start Wifi configuration portal.");
    Serial.println ("In that case, point your browser to 192.168.4.1");
  
     // make it reset and retry,or go to sleep
     // wifiManager.setTimeout(120);  //in seconds
      
    // fetches ssid and passwd from eeprom and tries to connect;
    // if it cannot connect, it starts an access point 
    // and goes into a *blocking* loop awaiting configuration
    wifiManager.autoConnect(SOFT_AP_SSID);
    // -- OR --
    // If you want to password protect your AP:
    // wifiManager.autoConnect(SOFT_AP_SSID, SOFT_AP_PASSWD)
    // -- OR --
    // or use this for auto generated name ESP + ChipID
    //wifiManager.autoConnect();
  
    digitalWrite(led2, HIGH);  // exiting wifi config mode
    // if you get here, you have connected to the WiFi
    Serial.print("Connected to your Wifi network: ");
    Serial.println(WiFi.SSID());
    Serial.print("Local IP address: ");
    Serial.println(WiFi.localIP());     
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());
}

void start_wifi_manager_portal() {
    digitalWrite(led2, LOW); // indicates start of config mode
    // WiFiManager local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    
    // Uncomment and run this once, if you want to erase all the stored wifi credentials:
    //wifiManager.resetSettings();
    
    // if you want a custom ip for the wifi manager portal
    //wifiManager.setAPConfig(IPAddress(10,0,1,10), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    
    // make it reset and retry, or go to sleep
    // wifiManager.setTimeout(120);  //in seconds     
     
    Serial.println ("On your request, Wifi configuration portal is starting....");
    Serial.println ("Point your browser to 192.168.4.1");
     
    if (!wifiManager.startConfigPortal(SOFT_AP_SSID)) {
      Serial.println("failed to connect to Wifi and timed out");
      delay(5000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(1000);
    }
    // -- OR --
    // If you want to password protect your AP:
    // wifiManager.startConfigPortal(SOFT_AP_SSID, SOFT_AP_PASSWD)
    // -- OR --
    // or use this for auto generated name: ESP + ChipID
    //wifiManager.startConfigPortal();
        
    digitalWrite(led2, HIGH);  // exiting wifi config mode
    // if you get here, you have connected to the WiFi
    Serial.print("Successfully connected to your Wifi network: ");
    Serial.println(WiFi.SSID());
    Serial.print("Local IP address: ");
    Serial.println(WiFi.localIP());    
    Serial.print("MAC address: ");
    Serial.println(WiFi.macAddress());    
    digitalWrite(led2, HIGH); // indicates end of config mode    
}

void erase_wifi_credentials() {
    digitalWrite(led2, LOW); // indicates start of config mode
    Serial.println("*** ERASING ALL WI-FI CREDENTIALS ! ***");
    client.publish(C.mqtt_pub_topic, "{\"I\":\"Erasing all WiFi credentials !\"}");
    delay(500);
    client.publish(C.mqtt_pub_topic, "{\"C\":\"Configure WiFi after restarting\"}");    
    // WiFiManager local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    // erase all the stored wifi credentials:
    wifiManager.resetSettings();
    Serial.println("Restarting the device...");
    delay(2000);
    ESP.reset();
}
//---------------------------------------------------------------------------------
const int MAX_TIME_ATTEMPTS = 100;
int time_attempts = 0;
void init_time_client() {
    Serial.println("Trying to open time client...");
    timeClient.begin();
    while(!timeClient.update()){
      timeClient.forceUpdate();
      Serial.print("-");
      delay(100);
      time_attempts++;
      if (time_attempts > MAX_TIME_ATTEMPTS) {
        Serial.println("\nCould not initialize time server client. Restarting...");
        delay(2000);
        ESP.restart();
      }
    }
    Serial.println("\nTime client initialized.");
    delay(1000); // delay is needed to stablize ?
    get_current_time();
    espClient.setBufferSizes(512, 512);  // any other suitable place to put this ?
    espClient.setX509Time(timeClient.getEpochTime());  
    delay(1000); // delay is needed to stablize ?
}  

bool init_file_system() {
    bool result = true;
    Serial.println("\nTrying to mount the file system...");
    Serial.print("Heap size: "); Serial.println(ESP.getFreeHeap());  
    if (!SPIFFS.begin()) {
        Serial.println("--- Failed to mount file system. ---");
        return false;
    }
    Serial.println("File system mounted.");
    Serial.print("Heap size: "); Serial.println(ESP.getFreeHeap());  
    // Load certificate file
    File cert = SPIFFS.open("/cert.der", "r"); //replace cert.crt eith your uploaded file name
    if (!cert) {
      Serial.println("--- Failed to open certificate file. ---");
      result = false;
    }
    else
      Serial.print("Opened device certificate...");
    delay(1000); // the delay seems to be necessary ?! it is from the original example
    if (espClient.loadCertificate(cert))
        Serial.println("Loaded certificate.");
    else {
        Serial.println("---- Device cert could not be loaded. ---");
        result = false;
    }
    File private_key = SPIFFS.open("/private.der", "r"); //replace private eith your uploaded file name
    if (!private_key) {
        Serial.println("--- Failed to open private key file. ---");
      result = false;
    }        
    else
        Serial.print("Opened private key...");
    delay(1000); // the delay seems to be necessary ?!
    if (espClient.loadPrivateKey(private_key))
        Serial.println("Loaded private key.");
    else {
        Serial.println("--- Private key could not be loaded. ---");
        result = false;
    }        
    // Load CA file
    File ca = SPIFFS.open("/ca.der", "r"); //replace ca eith your uploaded file name
    if (!ca) {
        Serial.println("--- Failed to open root CA. ---");
        result = false;
    }        
    else
        Serial.print("Opened root CA...");
    delay(1000); // is this a necessary delay ?
    if(espClient.loadCACert(ca))
        Serial.println("Loaded root CA.");
    else {
        Serial.println("--- Failed to load root CA. ---");
        result = false;
    }        
    Serial.print("Heap size: "); 
    Serial.println(ESP.getFreeHeap());
    Serial.println("");
    return (result);
}

const int RETRY_DELAY = 30000;  // mSec
const int MAX_CONNEXTION_ATTEMPTS = 20;  // 20*30 sec=10 minutes
int connection_attempts = 0;

void reconnect(bool restart) {
    Serial.println("\n(Re)connecting to AWS...");  
    if (WiFi.status() != WL_CONNECTED) 
        init_wifi();
    char  priming_msg[MAX_SHORT_STRING_LENGTH];
    // TODO: add the MAC id also
    snprintf (priming_msg, MAX_SHORT_STRING_LENGTH-1, "{\"C\":\"Portico server V 2.%d starting..\"}", C.current_firmware_version);
    // block until we're reconnected to AWS server/times out
    while (!client.connected()) {
        Serial.println("Attempting MQTT connection...");
        // Generate random client ID. otherwise it keeps connecting & disconnecting !!!
        // see  https://github.com/mqttjs/MQTT.js/issues/684 for client id collision problem, and  
        // https://www.cloudmqtt.com/blog/2018-11-21-mqtt-what-is-client-id.html      
        char client_id[MAX_CLIENT_ID_LENGTH]; // MQTT standard allows max 23 characters for client id
        //snprintf (client_id, MAX_CLIENT_ID_LENGTH-1, "%s%x%x",C.mqtt_client_prefix,random(0xffff), random(0xffff));
        snprintf (client_id, MAX_CLIENT_ID_LENGTH-1, "%s%s",C.mqtt_client_prefix, C.mac_address);
        Serial.print ("client ID: ");
        Serial.println(client_id);
        WiFi.mode(WIFI_STA);  // see https://github.com/knolleary/pubsubclient/issues/138 
                              // improves stability
        if (client.connect(client_id)) {  
          Serial.println("connected to AWS cloud.");
          // Once connected, publish an announcement...
          Serial.print ("Publishing to ");
          Serial.print (C.mqtt_pub_topic);
          Serial.println (" : ");
          Serial.println(priming_msg);
          client.publish(C.mqtt_pub_topic, priming_msg);
          // ... and resubscribe
          client.subscribe(C.mqtt_sub_topic);
          Serial.print("Subscribed to: ");
          Serial.println(C.mqtt_sub_topic);
          client.subscribe(C.mqtt_broadcast_topic);          
          Serial.print("Subscribed to (broadcast): ");
          Serial.println(C.mqtt_broadcast_topic);
        } else {
          Serial.print("AWS connection failed, rc=");
          Serial.println(client.state());
          Serial.println("Trying again in 60 seconds...");
          char buf[256];
          espClient.getLastSSLError(buf,256);
          Serial.print("WiFiClientSecure SSL error: ");
          Serial.println(buf);
          connection_attempts++;
          if (connection_attempts > MAX_CONNEXTION_ATTEMPTS) {
              if (restart) {
                Serial.println ("==== Failed to connect to AWS! Restarting ESP8266... ====");
                delay(2000);
                ESP.restart();
              }
              else
                Serial.println ("--- Failed to connect to AWS! ---");
           }          
          // Wait 30 seconds before retrying
          delay(RETRY_DELAY);
        } // else
    } // while
}

void send_status () {
    // TODO: introduce NUM_RELAYS into the following !   
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"S\":\"%d%d\"}", relay_status[1], relay_status[0]);    
    Serial.print("Publishing message: ");
    Serial.println(status_msg);
    client.publish(C.mqtt_pub_topic,status_msg);
    Serial.print("Free Heap: "); 
    Serial.println(ESP.getFreeHeap()); //Low heap can cause problems
}

void send_version() {
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"V\":\"%d\"}", C.current_firmware_version);    
    Serial.print("Publishing message: ");
    Serial.println(status_msg);
    client.publish(C.mqtt_pub_topic,status_msg);
    Serial.print("Free Heap: "); 
    Serial.println(ESP.getFreeHeap()); //Low heap can cause problems
}

void send_heap() {
    Serial.print("Free Heap: "); 
    long heap = ESP.getFreeHeap();
    Serial.println(heap); //Low heap can cause problems
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"H\":\"%ld\"}", heap);    
    Serial.print("Publishing message: ");
    Serial.println(status_msg);
    client.publish(C.mqtt_pub_topic,status_msg);
}

int relay_number = 0;
void handle_command(const char* command_string) {
    if (strlen (command_string) < 3) {
        Serial.println ("Command can be: STA,UPD,VER,HEA,ONx,OFFx");
        return;
    }
    // STATUS command
    if (command_string[0]=='S' && command_string[1]=='T' && command_string[2]=='A') {
        send_status();
        return;
    }
    // Version command
    if (command_string[0]=='V' && command_string[1]=='E' && command_string[2]=='R') {
        send_version();
        return;
    }    
    // Get free heap command
    if (command_string[0]=='H' && command_string[1]=='E' && command_string[2]=='A') {
        send_heap();
        return;
    }       
    // Erase WiFi AP credentials command
    if (command_string[0]=='D' && command_string[1]=='E' && command_string[2]=='L') {
        erase_wifi_credentials();
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
            Serial.println ("-- Error: Invalid relay number --");
            return;
        }
        relay_number = command_string[2] - '0';
        Serial.print ("Remote command: ON; Relay : ");
        Serial.println (relay_number);
        relay_status[relay_number] = 1;  // 1 means the realy is ON
        digitalWrite (relay_pin[relay_number], LOW);  // active low
        send_status();       
        return;
    }
    // OFF commands
    if (strlen (command_string) < 4) { // TODO: you may need to add another use case <3 below this !
        Serial.println ("Command can be ONx or OFFx where x is from 0-9");
        return;
    }    
    if (command_string[0]=='O' && command_string[1]=='F' && 
        command_string[2]=='F') {
        if (command_string[3] < '0' ||  command_string[3] >= NUM_RELAYS+'0') {
            Serial.println ("-- Error: Invalid relay number --");
            return;
        }
        relay_number = command_string[3] - '0';
        Serial.print ("Remote command: OFF; Relay : ");
        Serial.println (relay_number);
        relay_status[relay_number] = 0;  // 0 means the realy is OFF
        digitalWrite (relay_pin[relay_number], HIGH);  // active low
        send_status();  
    } 
}

void check_for_updates() {
    Serial.println("---Firmware update command received---");
    O.check_and_update();
    Serial.println("Reached after OTA-check for updates."); // reached if the update fails
}

// This updates global hour and minute variables for use by check_day_or_night function
void get_current_time() {
    current_hour = -1;  // to detect time server failure
    current_minute = -1;
    current_hour = timeClient.getHours();  
    current_minute = timeClient.getMinutes();
    Serial.print("The time is: ");
    Serial.println(timeClient.getFormattedTime());    
}

// updates global is_night variable
// this is called every 10 minutes and switch the portico light to a schedule
// NOTE: You  MUST call get_current_time before calling check_day_or_night ***
void check_day_or_night() {  
    if (current_hour < 0) {   
      Serial.println("Could not get time from Time Server");
      return;
    }
    if (current_hour > C.NIGHT_START_HOUR  || current_hour < C.NIGHT_END_HOUR) 
        is_night = true;
    if (current_hour == C.NIGHT_START_HOUR && current_minute >= C.NIGHT_START_MINUTE)  
        is_night = true;        
    if (current_hour == C.NIGHT_END_HOUR && current_minute <= C.NIGHT_END_MINUTE)  
        is_night = true;
}

void blink1() {
  for (int i=0; i<8; i++) {
      digitalWrite (led1, LOW);  // active low, green LED, for PIR
      delay(50);
      digitalWrite (led1, HIGH);  // active low
      delay(50);               
  }
  digitalWrite (led2, LOW); // let the red light be on till we connect to the Net
}


void blink2() {
  for (int i=0; i<4; i++) {
      digitalWrite (led2, LOW);  // active low, red LED, for radar
      delay(150);
      digitalWrite (led2, HIGH);  // active low 
      delay(150);               
  }
}
