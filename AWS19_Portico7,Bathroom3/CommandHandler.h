// commandHandler.h
 
#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H


#include "config.h"

#include <PubSubClient.h>       // https://github.com/knolleary/pubsubclient 

class CommandHandler  {
public:
    CommandHandler(int num_relays);
    void init (Config *pC, PubSubClient *pClient);
    void handle_command(const char* command_string);    
    void publish_message ();
    void send_status ();
    void send_version();
    void send_mac_address();
    void send_heap();
    void send_org();
    void send_group();
    void reboot_esp();
    void auto_mode ();
    void manual_mode ();
    void send_mode();
    void print_heap();
    
private:
    char status_msg[MAX_MSG_LENGTH];   // Tx message
    int num_relays;
    Config *pC;
    PubSubClient *pClient;
};

#endif 
