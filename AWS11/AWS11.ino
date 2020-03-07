/* ESP8266 AWS IoT example by Evandro Luis Copercini
   Code based on:
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
> openssl s_client -connect axxxxx-ats.iot.us-east-2.amazonaws.com:8443   -CAfile root-CA.crt 
  -cert portico_thing.cert.pem   -key portico_thing.private.key

  *** NOTE: 8266 subscriber is unable to receive the long MQTT messages from AWS. Use short custom messages ***
*/

#include "otaHelper.h"
#include "config.h"
#include <FS.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h> // https://github.com/knolleary/pubsubclient 
#include <NTPClient.h> // https://github.com/arduino-libraries/NTPClient
#include <WiFiUdp.h>
#include  <ArduinoJson.h>  // Blanchon, https://github.com/bblanchon/ArduinoJson

void callback(char* topic, byte* payload, unsigned int length);

const char* ssid = "xxxx";  // case sensitive
const char* password = "xxxxxx";
const int STATUS_INTERVAL = 60000;  // mSec

char status_msg[MAX_MSG_LENGTH];   // Tx message

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 55*360);  // UTC offset=5.5 hours * 3600 seconds
WiFiClientSecure espClient;
PubSubClient client(AWS_ENDPOINT, MQTT_PORT, callback, espClient); //set  MQTT port number to 8883 as per standard

Config C;
OtaHelper O;

int PORCH_LIGHT = 1;    // the main portico tube light is 1. (the side bulb is 0)
const int NUM_RELAYS = 2;
// For Vaidy's circular box, the relays are connected to D1,D2.
byte relay_pin[] =  {D1,D2}; //GPIO 5 and GPIo 4 for prod.  //{D0,D4} GPIO 16 and GPIO 0 for testing 
byte led1 = D3;  // GPIO 0
byte led2 = D9;  // Rx
bool relay_status[] = {0, 0};

int current_hour = -1;
int current_minute = -1;
long last_time_stamp = 0;

//-------------------------------------------------------------
// MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("]: ");
  Serial.print("Length: ");
  Serial.println(length);  
  // long messages are anyway dropped by 8266 !
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
  Serial.begin(115200);
  for (int i=0; i<NUM_RELAYS; i++) {
    pinMode(relay_pin[i], OUTPUT);
    digitalWrite(relay_pin[i],HIGH);  // active high
  }  
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  blink1();
  Serial.println("\n ************** AWS IoT X-509 client starting.. ***************");
  Serial.setDebugOutput(true);
  C.init();
  C.dump();
  O.init(&C, &client);
  init_wifi();
  init_time_client();
  init_file_system();
  check_day_or_night(); // this is done only once at start up; but you can do this periodically also
  reconnect();  // priming connect
  blink2();  
}

void loop() {
  if (!client.connected()) {
     Serial.println("LOOP: client is not connected.");
     reconnect();
     delay(2000);  // to avoid repeated triggers ?
  }
  client.loop();
  /*
  long now = millis();
  if (now - last_time_stamp > STATUS_INTERVAL) {
    last_time_stamp = now;
    send_status();
  }
  */
}
//-------------------------------------------------------------
#define MAX_WIFI_ATTEMPTS  120
void init_wifi() {
    delay(10);
    Serial.println();
    Serial.print("\nConnecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED) {   
      delay(500);  
      Serial.print(".");
      attempts++;
      if (attempts > MAX_WIFI_ATTEMPTS) {
          Serial.println("\nCould not connect to WiFi. Restarting...");
          delay(2000);
          ESP.restart();
      }
    }
    Serial.println("\nWiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

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

void init_file_system() {
    Serial.println("\nTrying to mount the file system...");
    Serial.print("Heap size: "); Serial.println(ESP.getFreeHeap());  
    if (!SPIFFS.begin()) {
        Serial.println("--- Failed to mount file system. ---");
        return;
    }
    Serial.println("File system mounted.");
    Serial.print("Heap size: "); Serial.println(ESP.getFreeHeap());  
    // Load certificate file
    File cert = SPIFFS.open("/cert.der", "r"); //replace cert.crt eith your uploaded file name
    if (!cert) 
      Serial.println("--- Failed to open certificate file. ---");
    else
      Serial.print("Opened device certificate...");
    delay(1000); // the delay seems to be necessary ?! it is from the original example
    if (espClient.loadCertificate(cert))
        Serial.println("Loaded certificate.");
    else
        Serial.println("Device cert could not be loaded");
    File private_key = SPIFFS.open("/private.der", "r"); //replace private eith your uploaded file name
    if (!private_key) 
        Serial.println("--- Failed to open private key file. ---");
    else
        Serial.print("Opened private key...");
    delay(1000); // the delay seems to be necessary ?!
    if (espClient.loadPrivateKey(private_key))
        Serial.println("Loaded private key.");
    else
        Serial.println("--- Private key could not be loaded. ---");
    // Load CA file
    File ca = SPIFFS.open("/ca.der", "r"); //replace ca eith your uploaded file name
    if (!ca) 
        Serial.println("--- Failed to open root CA. ---");
    else
        Serial.print("Opened root CA...");
    delay(1000); // is this a necessary delay ?
    if(espClient.loadCACert(ca))
        Serial.println("Loaded root CA.");
    else
        Serial.println("--- Failed to load root CA. ---");
    Serial.print("Heap size: "); 
    Serial.println(ESP.getFreeHeap());
    Serial.println("");
}

const int RETRY_DELAY = 30000;  // mSec
const int MAX_CONNEXTION_ATTEMPTS = 20;  // 20*30 sec=10 minutes
int connection_attempts = 0;

void reconnect() {
    Serial.println("\n(Re)connecting to AWS...");  
    if (WiFi.status() != WL_CONNECTED) 
        init_wifi();
    char  priming_msg[MAX_SHORT_STRING_LENGTH];
    snprintf (priming_msg, MAX_SHORT_STRING_LENGTH-1, "{\"C\":\"portico server V 2.%d starting..\"}", C.current_firmware_version);
    // block until we're reconnected to AWS server/times out
    while (!client.connected()) {
        Serial.println("Attempting MQTT connection...");
        // Generate random client ID. otherwise it keeps connecting & disconnecting !!!
        // see  https://github.com/mqttjs/MQTT.js/issues/684 for client id collision problem, and  
        // https://www.cloudmqtt.com/blog/2018-11-21-mqtt-what-is-client-id.html      
        char client_id[MAX_CLIENT_ID_LENGTH]; // MQTT standard allows max 23 characters for client id
        snprintf (client_id, MAX_CLIENT_ID_LENGTH-1, "%s_%x%x",C.mqtt_client_prefix,random(0xffff), random(0xffff));
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
              Serial.println ("==== Failed to connect to AWS! Restarting ESP8266... ====");
              delay(2000);
              ESP.restart();
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

void get_current_time() {
    current_hour = -1;  // to detect time server failure
    current_minute = -1;
    current_hour = timeClient.getHours();  
    current_minute = timeClient.getMinutes();
    Serial.print("The time is: ");
    Serial.println(timeClient.getFormattedTime());    
}

void check_day_or_night() {  // TODO: do this every 10 minutes and switch on the light
    if (current_hour < 0) {
      Serial.println("Could not get time from Time Server");
      return;
    }
    bool is_night = false;
    if (current_hour > C.NIGHT_START_HOUR  || current_hour < C.NIGHT_END_HOUR) 
        is_night = true;
    if (current_hour == C.NIGHT_START_HOUR && current_minute >= C.NIGHT_START_MINUTE)  
        is_night = true;        
    if (current_hour == C.NIGHT_END_HOUR && current_minute <= C.NIGHT_END_MINUTE)  
        is_night = true;
    if (is_night) {
        Serial.println ("Starting at night time. Porch light is ON");
        relay_status[PORCH_LIGHT] = 1;  // 1 means the realy is ON
        digitalWrite (relay_pin[PORCH_LIGHT], LOW);  // active low
        //send_status(); // MQTT is not yet initialized
    }
    else {
        Serial.println ("Starting at day time. Porch light is OFF");
        relay_status[PORCH_LIGHT] = 0;  // 0 means the realy is OFF
        digitalWrite (relay_pin[PORCH_LIGHT], HIGH);  // active low
        //send_status(); // MQTT is not yet initialized
    }
}

void blink1() {
  for (int i=0; i<8; i++) {
      digitalWrite (led1, LOW);  // active low
      delay(50);
      digitalWrite (led1, HIGH);  // active low
      delay(50);               
  }
}


void blink2() {
  for (int i=0; i<4; i++) {
      digitalWrite (led2, LOW);  // active low
      delay(150);
      digitalWrite (led2, HIGH);  // active low 
      delay(150);               
  }
}
