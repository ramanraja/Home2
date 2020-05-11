// mqttLite.cpp

//#include "config.h"
#include "mqttLite.h"
//#include <ESP8266WiFi.h>

WiFiClient   wifi_client;
PubSubClient pubsub_client(wifi_client);

extern void  app_callback(const char* command_string);

//------------------------------------------------------------------------------------------------
// static callback to handle mqtt command messages
char rx_payload_string [MAX_STRING_LENGTH]; 

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    SERIAL_PRINT ("Message received @ ");
    SERIAL_PRINTLN (topic);
    if (length >= MAX_STRING_LENGTH) {
        SERIAL_PRINTLN("* Mqtt Rx message is too long ! *");
        return;  // incoming message is killed
    }
    char *ptr = rx_payload_string;
    for (int i = 0; i < length; i++) {  // ugly, but needed. TODO: find another way 
       *ptr++ = (char)payload[i];        // what if sizeof(char) is not equal to sizeof(byte) ?
    }
    *ptr= '\0';
    //SERIAL_PRINTLN(rx_payload_string);  
    app_callback ((const char*)rx_payload_string);  
}
//------------------------------------------------------------------------------------------------
// class methods

MqttLite::MqttLite() {
}

void MqttLite::init (Config* configptr, MyFi*  wifiptr) {
    this->pC = configptr; // first, keep the pointers safe
    this->pW = wifiptr;
    strncpy (mqtt_client_id, pC->mqtt_client_prefix, MAX_STRING_LENGTH-10);  
    SERIAL_PRINT("MQTT client id base: ");
    SERIAL_PRINTLN(mqtt_client_id);  
    if (reconnect ())  // initial connection
        subscribe ();  // TODO: is there sticky subscription ?
} 

// you must periodically call this function; othewise the MQTT pump will starve
void MqttLite::update () {
    if (pubsub_client.connected ())      
        pubsub_client.loop ();       // keep mqtt pump running
    else {    
        if (reconnect ())
            subscribe ();      
    }
}

//connects to mqtt layer
bool MqttLite::reconnect () {
    if (!pW->isConnected()) {
        SERIAL_PRINTLN ("MqttLite:No WiFi connection.");
        pW->reconnect();
        return false;
    }
    SERIAL_PRINTLN ("Connecting to MQTT server...");
    if (pubsub_client.connected())  
        pubsub_client.disconnect ();  // to avoid leakage ?

    if (pC-> generate_random_client_id) {
        generateClientID ();              // TODO: revisit
        SERIAL_PRINT("MQTT client id: ");
        SERIAL_PRINTLN(mqtt_client_id);  
    }
    pubsub_client.setServer(pC->mqtt_server, pC->mqtt_port);
    WiFi.mode(WIFI_STA);  // see https://github.com/knolleary/pubsubclient/issues/138 

 #ifdef USE_MQTT_CREDENTIALS
    if (pubsub_client.connect(mqtt_client_id, pC->mqtt_user, pC->mqtt_password)) {
 #else
    if (pubsub_client.connect(mqtt_client_id) {
 #endif
        SERIAL_PRINTLN("Connected to MQTT broker.");     
        return true;
    } else {
        SERIAL_PRINT("MQTT connection failed, rc=");
        SERIAL_PRINTLN(pubsub_client.state());
        return false;
    }
}

//generate random mqtt clientID
void MqttLite::generateClientID () {
    sprintf (mqtt_client_id, "%s_%x%x",  
        pC->mqtt_client_prefix, 
        random(0xffff), random(0xffff));  
}

//subscribe to the mqtt command topic
bool MqttLite::subscribe () {
    if (strlen(pC->mqtt_sub_topic) < 1) {
        SERIAL_PRINT("No topic to subscribe.");
        return false;
    }
    SERIAL_PRINT("Subscribing to topic: ");
    SERIAL_PRINTLN(pC->mqtt_sub_topic);  
    pubsub_client.setCallback(mqtt_callback); // static function outside the class
    bool result = pubsub_client.subscribe(pC->mqtt_sub_topic, QOS);  
    if (result) 
        SERIAL_PRINTLN ("Subscribed.");    
    else
        SERIAL_PRINTLN ("Subscribing failed.");
    return(result);
}

//send a message to the primary mqtt publish topic
bool MqttLite::publish (const char *msg) {
    SERIAL_PRINTLN ("Publishing MQTT message: ");    
    SERIAL_PRINTLN (msg);
    int result = pubsub_client.publish(pC->mqtt_pub_topic, msg);
    if (!result) 
        SERIAL_PRINTLN ("Publishing failed.");    
    return((bool)result);  // true: success, false:failed to publish
}

//send a message to the secondary mqtt publish topic
//bool MqttLite::publish_command (const char *msg) {
//}

 
