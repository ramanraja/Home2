//config.cpp
#include "config.h"

extern void safe_strncpy (char *dest, const char *src, int length = MAX_LONG_STRING_LENGTH);

Config::Config(){
}

/* Example topics:
 publish:   intof/portico/status/G1/2CF432173BC0
 subscribe: intof/portico/cmd/G1/2CF432173BC0
 subscribe: intof/portico/cmd/G1/0
 */
void Config::init() {
    byte mac[6];
    WiFi.macAddress(mac);
    snprintf (mac_address, MAC_ADDRESS_LENGTH-1, "%2X%2X%2X%2X%2X%2X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    safe_strncpy (firmware_server_url,  FW_SERVER_URL);
    safe_strncpy (version_check_url,    FW_VERSION_URL);   
    snprintf (mqtt_pub_topic, MAX_SHORT_STRING_LENGTH-1, "%s/%s/%s/%s/%s",  org_id, app_id, PUB_TOPIC_PREFIX, group_id, mac_address); 
    snprintf (mqtt_sub_topic, MAX_SHORT_STRING_LENGTH-1, "%s/%s/%s/%s/%s",  org_id, app_id, SUB_TOPIC_PREFIX, group_id, mac_address);
    snprintf (mqtt_broadcast_topic, MAX_SHORT_STRING_LENGTH-1, "%s/%s/%s/%s/%s",  org_id, app_id, SUB_TOPIC_PREFIX, group_id, UNIVERSAL_DEVICE_ID); 
    //snprintf (data_prod_url, MAX_LONG_STRING_LENGTH, "%s%s", DATA_PROD_URL, API_KEY);
    //safe_strncpy (data_test_url,  DATA_TEST_URL);       
}

void Config::dump() {
    SERIAL_PRINTLN("\n-----------------------------------------");
    SERIAL_PRINTLN("               configuration             ");
    SERIAL_PRINTLN("-----------------------------------------");    
    SERIAL_PRINT ("MAC address: ");
    SERIAL_PRINTLN (mac_address);
    SERIAL_PRINT ("Firmware version: ");
    SERIAL_PRINTLN (current_firmware_version);
    long freeheap = ESP.getFreeHeap();
    SERIAL_PRINT ("Free heap: ");
    SERIAL_PRINTLN (freeheap);   
    SERIAL_PRINT ("Firmware server URL: ");
    SERIAL_PRINTLN (firmware_server_url);    
    SERIAL_PRINT("Firmware version URL: ");
    SERIAL_PRINTLN(version_check_url);      

    SERIAL_PRINTLN("MQTT server: AWS");
    //SERIAL_PRINT(mqtt_server); 
    //SERIAL_PRINT(", ");
    //SERIAL_PRINTLN(mqtt_port); 
    SERIAL_PRINT ("Publish topic: ");
    SERIAL_PRINTLN (mqtt_pub_topic); 
    SERIAL_PRINT ("Subscribe topic: ");
    SERIAL_PRINTLN (mqtt_sub_topic); 
    SERIAL_PRINT ("Broadcast topic (sub): ");
    SERIAL_PRINTLN (mqtt_broadcast_topic);

    //SERIAL_PRINT ("Data-Production URL: ");
    //SERIAL_PRINTLN (data_prod_url);    
    //SERIAL_PRINT("Data-Test URL: ");
    //SERIAL_PRINTLN(data_test_url);      
    SERIAL_PRINTLN("-----------------------------------------\n");                     
}
 

 
