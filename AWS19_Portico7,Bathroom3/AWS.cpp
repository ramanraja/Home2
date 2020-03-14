// aws.cpp
#include "aws.h"

void callback(char* topic, byte* payload, unsigned int length);
// external callback function defined in .ino:
extern void notify_command (const char* command);
extern void safe_strncpy (char *dest, const char *src, int length = MAX_LONG_STRING_LENGTH);

// Defining the following objects within the AWS class results in errors; possibly clash among similar libraries
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 55*360);  // UTC offset=5.5 hours * 3600 seconds
WiFiClientSecure espClient;
PubSubClient client(AWS_END_POINT, MQTT_PORT, callback, espClient); //set  MQTT port number to 8883 as per standard  

char command_str[MAX_COMMAND_LENGTH];

// MQTT callback
// no need for the static keyword in the function definition, if it is part of the AWS class
// https://stackoverflow.com/questions/5373107/how-to-implement-static-class-member-functions-in-cpp-file
void callback(char* topic, byte* payload, unsigned int length) {
  SERIAL_PRINT("Message arrived [");
  SERIAL_PRINT(topic);
  SERIAL_PRINTLN("]: ");
  SERIAL_PRINT("Length: ");
  SERIAL_PRINTLN(length);  
  // long messages are anyway dropped by PubSubClient !
  //if (length > MAX_MSG_LENGTH) return;  // MAX_MSG_LENGTH refers to the full json formatted payload
  /**/
  for (int i = 0; i < length; i++) 
      SERIAL_PRINT((char)payload[i]);
  SERIAL_PRINTLN();
  /**/
  //SERIAL_PRINTLN ((char *)payload); // this prints a corrupted string ! TODO: study this further
  StaticJsonDocument<MAX_MSG_LENGTH> doc;  // stack
  //DynamicJsonDocument doc(MAX_MSG_LENGTH);   // heap
  DeserializationError error = deserializeJson(doc, (char *)payload); //  
  if (error) {
    SERIAL_PRINT("Json deserialization failed: ");
    SERIAL_PRINTLN(error.c_str());
    return;
  }
  if (!doc.containsKey("C")) {
    SERIAL_PRINTLN("invalid command");
    return;
  }
  //const char* cmd = doc["C"];  // do not hold on to the jsondoc object !
  safe_strncpy (command_str, doc["C"], MAX_COMMAND_LENGTH); // COMMAND_LENGTH is for the inner string, without json formatting artifacts.
  SERIAL_PRINT("Command: ");
  if (strlen(command_str) > 0) {
    SERIAL_PRINTLN(command_str);
    notify_command((const char*)command_str); // this is defined in the main  portico.ino file
  }   
}
//------------------------------------------------------------------------------------------------
AWS::AWS() {
  SERIAL_PRINTLN("AWS object created.");
}

short AWS::init(Config *configptr) {
   return (init(configptr, false));
}

short AWS::init(Config *configptr, bool restart) {
  this->pC = configptr;
  this->restart = restart;
  if (!init_file_system()) 
      return CERTIFICATE_FAILED;  
  if (!init_time_client())  
      return TIME_SERVER_FAILED;
  // NOTE: without a time server, you cannot connect to AWS. You get the message:
  // "WiFiClientSecure SSL error: Certificate is expired or not yet valid".
  bool mqtt_connection_result = reconnect(); //  priming connection to MQTT
  if (!mqtt_connection_result)
      return AWS_CONNECT_FAILED;
  return AWS_CONNECT_SUCCESS;
}

bool AWS::init_time_client() {
    SERIAL_PRINTLN("Trying to connect to Time Server...");
    timeClient.begin();
    while(!timeClient.update()){
      timeClient.forceUpdate();
      SERIAL_PRINT("-");
      delay(100);
      time_attempts++;
      if (time_attempts > MAX_TIME_ATTEMPTS) {
        if (restart) {
            SERIAL_PRINTLN("\n*** Failed to initialize time server client. Restarting...");
            delay(2000);
            ESP.restart();
        }
        else {
            SERIAL_PRINTLN("\n*** Failed to initialize time server client! ***");
            delay(1000);
            return (false);
        }
      }
    }
    SERIAL_PRINTLN("\nTime server client initialized.");
    delay(1000); // delay is needed to stablize ?
    get_current_time();
    espClient.setBufferSizes(512, 512);  // TODO: find another suitable place to put this  
    espClient.setX509Time(timeClient.getEpochTime());  
    delay(1000); // delay is needed to stablize ?
    return (true);
}  

bool AWS::init_file_system() {
    bool result = true;
    SERIAL_PRINTLN("\nTrying to mount the file system...");
    SERIAL_PRINT("Heap size: "); SERIAL_PRINTLN(ESP.getFreeHeap());  
    if (!SPIFFS.begin()) {
        SERIAL_PRINTLN("--- Failed to mount file system. ---");
        return false;
    }
    SERIAL_PRINTLN("File system mounted.");
    SERIAL_PRINT("Heap size: "); SERIAL_PRINTLN(ESP.getFreeHeap());  
    // Load certificate file
    File cert = SPIFFS.open("/cert.der", "r"); //replace cert.crt eith your uploaded file name
    if (!cert) {
      SERIAL_PRINTLN("--- Failed to open certificate file. ---");
      result = false;
    }
    else
      SERIAL_PRINT("Opened device certificate...");
    delay(1000); // the delay seems to be necessary ?! it is from the original example
    if (espClient.loadCertificate(cert))
        SERIAL_PRINTLN("Loaded certificate.");
    else {
        SERIAL_PRINTLN("---- Device cert could not be loaded. ---");
        result = false;
    }
    File private_key = SPIFFS.open("/private.der", "r"); //replace private eith your uploaded file name
    if (!private_key) {
        SERIAL_PRINTLN("--- Failed to open private key file. ---");
      result = false;
    }        
    else
        SERIAL_PRINT("Opened private key...");
    delay(1000); // the delay seems to be necessary ?!
    if (espClient.loadPrivateKey(private_key))
        SERIAL_PRINTLN("Loaded private key.");
    else {
        SERIAL_PRINTLN("--- Private key could not be loaded. ---");
        result = false;
    }        
    // Load CA file
    File ca = SPIFFS.open("/ca.der", "r"); //replace ca eith your uploaded file name
    if (!ca) {
        SERIAL_PRINTLN("--- Failed to open root CA. ---");
        result = false;
    }        
    else
        SERIAL_PRINT("Opened root CA...");
    delay(1000); // is this a necessary delay ?
    if(espClient.loadCACert(ca))
        SERIAL_PRINTLN("Loaded root CA.");
    else {
        SERIAL_PRINTLN("--- Failed to load root CA. ---");
        result = false;
    }        
    SERIAL_PRINT("Heap size: "); 
    SERIAL_PRINTLN(ESP.getFreeHeap());
    SERIAL_PRINTLN("");
    return (result);
}

// we assume WiFi is available. The caller (main loop) has to ensure that.
void AWS::update() {
  if (!client.connected()) {
     SERIAL_PRINTLN("AWS loop: client is not connected.");
     delay(5000);  // to avoid repeated triggers ?
     reconnect();   
  }
  client.loop();
}

bool AWS::reconnect() {
    SERIAL_PRINTLN("\n(Re)connecting to AWS...");  
    char  priming_msg[MAX_SHORT_STRING_LENGTH];
    snprintf (priming_msg, MAX_SHORT_STRING_LENGTH-1, "{\"B\":\"%s [%s] V 2.%d starting..\"}", 
              pC->app_name, pC->mac_address, pC->current_firmware_version);
    // block until we're reconnected to AWS server/times out
    while (!client.connected()) {
        SERIAL_PRINTLN("Attempting MQTT connection...");
        // Generate random client ID. otherwise it keeps connecting & disconnecting !!!
        // see  https://github.com/mqttjs/MQTT.js/issues/684 for client id collision problem, and  
        // https://www.cloudmqtt.com/blog/2018-11-21-mqtt-what-is-client-id.html      
        char client_id[MAX_CLIENT_ID_LENGTH]; // MQTT standard allows max 23 characters for client id
        //snprintf (client_id, MAX_CLIENT_ID_LENGTH-1, "%s%x%x",pC->mqtt_client_prefix,random(0xffff), random(0xffff));
        snprintf (client_id, MAX_CLIENT_ID_LENGTH-1, "%s_%s",pC->mqtt_client_prefix, pC->mac_address);
        SERIAL_PRINT ("client ID: ");
        SERIAL_PRINTLN(client_id);
        WiFi.mode(WIFI_STA);  // see https://github.com/knolleary/pubsubclient/issues/138 
                              // improves stability
        if (client.connect(client_id)) {  
          SERIAL_PRINTLN("connected to AWS cloud.");
          // Once connected, publish an announcement...
          SERIAL_PRINT ("Publishing to ");
          SERIAL_PRINT (pC->mqtt_pub_topic);
          SERIAL_PRINTLN (" : ");
          SERIAL_PRINTLN(priming_msg);
          client.publish(pC->mqtt_pub_topic, priming_msg);
          // ... and resubscribe
          client.subscribe(pC->mqtt_sub_topic);
          SERIAL_PRINT("Subscribed to: ");
          SERIAL_PRINTLN(pC->mqtt_sub_topic);
          client.subscribe(pC->mqtt_broadcast_topic);          
          SERIAL_PRINT("Subscribed to (broadcast): ");
          SERIAL_PRINTLN(pC->mqtt_broadcast_topic);
        } else {
          SERIAL_PRINT("AWS connection failed, rc=");
          SERIAL_PRINTLN(client.state());
          SERIAL_PRINTLN("Trying again in 30 seconds...");
          char buf[256];
          espClient.getLastSSLError(buf,256);
          SERIAL_PRINT("WiFiClientSecure SSL error: ");
          SERIAL_PRINTLN(buf);
          connection_attempts++;
          if (connection_attempts > MAX_CONNEXTION_ATTEMPTS) {
              if (restart) {
                SERIAL_PRINTLN ("*** Failed to connect to AWS! Restarting... ***");
                delay(2000);
                ESP.restart();
              }
              else {
                SERIAL_PRINTLN ("*** Failed to connect to AWS! ***");
                return (false);
              }
           }          
          // Wait 30 seconds before retrying
          delay(RETRY_DELAY);
        } // else
    } // while
    return (true);
}

PubSubClient* AWS::getPubSubClient() {
    return (&client);
}

// This updates global hour and minute variables  
void AWS::get_current_time() {
    current_hour = -1;  // to detect time server failure
    current_minute = -1;
    current_hour = timeClient.getHours();  
    current_minute = timeClient.getMinutes();
    //SERIAL_PRINT("Time: ");
    //SERIAL_PRINTLN(timeClient.getFormattedTime());    
}

// The output is ternary: day,night,unknown
// this is called every 10 minutes and switch the portico light to a schedule
short AWS::is_night_time() {  
    get_current_time();
    if (current_hour < 0 || current_minute < 0) {   
      SERIAL_PRINTLN("Could not get time from Time Server");
      return TIME_UNKNOWN;
    }
    if (current_hour > pC->NIGHT_START_HOUR  || current_hour < pC->NIGHT_END_HOUR) 
        return TIME_NIGHT;
    if (current_hour == pC->NIGHT_START_HOUR && current_minute >= pC->NIGHT_START_MINUTE)  
        return TIME_NIGHT;        
    if (current_hour == pC->NIGHT_END_HOUR && current_minute <= pC->NIGHT_END_MINUTE)  
        return TIME_NIGHT;
    return TIME_DAY;
}

 
