// commandHandler.h
 
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include "common.h"
#include "settings.h"
#include "utilities.h"
#include "config.h"
#include <PubSubClient.h>       // https://github.com/knolleary/pubsubclient 

class Hardware;  // required forward declaration

class CommandHandler  {
public:
    // manual_override is public because it is accessed frequently in main .ino 
    bool manual_override = false; // for remote commands, set this to true
    CommandHandler();
    void init (Config *pC, PubSubClient *pClient, Hardware *phardware);
    void handle_command(const char* command_string);    
    void publish_message ();
    void send_status ();
    void send_data ();
    void send_version();
    void send_mac_address();// this can be used for a quick roll call
    void send_heap();
    void send_org();
    void send_group();
    void reboot();
    void auto_mode ();
    void manual_mode ();
    void send_mode();
    ///void print_heap();

private:
    char status_msg[MAX_MSG_LENGTH];   // Tx message
    int num_relays = NUM_RELAYS;
    Config *pC;
    PubSubClient *pClient;
    Hardware *pHard;    
};

#endif 
