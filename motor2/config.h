//config.h
#ifndef CONFIG_H
#define CONFIG_H 
 
#include "common.h"
#include "keys.h"
#include <ESP8266WiFi.h>

// increment this number for every version
#define  FIRMWARE_VERSION       2

#define  FW_SERVER_URL          "http://192.168.0.100:8000/ota/motor.bin"
#define  FW_VERSION_URL         "http://192.168.0.100:8000/ota/motor.txt"

#define  BAUD_RATE              115200 

class Config {
public :
int  current_firmware_version =  FIRMWARE_VERSION;  
bool sleep_deep = true;

char firmware_server_url [MAX_STRING_LENGTH];
char version_check_url [MAX_STRING_LENGTH];
bool verison_check_enabled = true;

/* The following constants should be updated in  "keys.h" file  */
const char *wifi_ssid1        = WIFI_SSID1;
const char *wifi_password1    = WIFI_PASSWORD1;
const char *wifi_ssid2        = WIFI_SSID2;
const char *wifi_password2    = WIFI_PASSWORD2;
const char *wifi_ssid3        = WIFI_SSID3;
const char *wifi_password3    = WIFI_PASSWORD3;

//MQTT 
// see the post at: http://www.hivemq.com/demos/websocket-client/
// Or in case of Adafruit MQTT, use MQTT-FX application to see the messages.
bool  generate_random_client_id = true;   // false; // 
const char*  mqtt_client_prefix = "ind_che_rajas_cli";

const char*  mqtt_server = "io.adafruit.com";
const int    mqtt_port = 1883;

// comment this out if not using login for the broker:
#define  USE_MQTT_CREDENTIALS
const char*  mqtt_user  = MQTT_USER;
const char*  mqtt_password  = MQTT_PASSWORD;

const char*  mqtt_sub_topic  = MQTT_SUB_TOPIC;
const char*  mqtt_pub_topic  = MQTT_PUB_TOPIC;

// for HttpPoster, if any
//char data_prod_url [MAX_STRING_LENGTH];
//char data_test_url [MAX_STRING_LENGTH];

Config();
void init();
void dump();
 
};  
#endif 
 
