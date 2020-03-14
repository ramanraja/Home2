// CommandHandler.cpp
 
#include "CommandHandler.h" 
const char* modes[] = {"AUTO", "MANUAL"};

// external callback functions defined in main .ino file 
byte getManualOverride ();
byte setManualOverride (bool set_to_manual_mode);
const bool* getStatus();
void relay_on (short relay_number);
void relay_off (short relay_number);
void reset_wifi();
void check_for_updates();
void blinker(byte pin_number);

CommandHandler::CommandHandler(int numrelays) {
    num_relays = numrelays;
}
 
void CommandHandler::init(Config *pconfig, PubSubClient *pclient) {
    pC = pconfig;
    pClient = pclient;
}
 
// takes the global status message and publishes it
// NOTE: You MUST set up the status_message before calling this !
void CommandHandler::publish_message () {
    SERIAL_PRINT("Publishing: ");
    SERIAL_PRINTLN(status_msg);
    pClient->publish(pC->mqtt_pub_topic, status_msg);
}

void CommandHandler::send_status () {
    // TODO: introduce num_relays into the following !   
    const bool* status = getStatus();
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"S\":\"%d%d\"}", status[1], status[0]);    
    publish_message();
}

void CommandHandler::send_version() {
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"V\":\"%d\"}", pC->current_firmware_version);    
    publish_message();
}

// Send MAC address - This is useful for polling the devices on the broadcast topic
void CommandHandler::send_mac_address() {
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"M\":\"%s\"}", pC->mac_address);    
    publish_message();
}

void CommandHandler::send_heap() {
    long heap = ESP.getFreeHeap();
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"H\":\"%ld\"}", heap);    
    publish_message();
}

void CommandHandler::send_org() {
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"O\":\"%s\"}", pC->org_id);    
    publish_message();
}

void CommandHandler::send_group() {
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"G\":\"%s\"}", pC->group_id);    
    publish_message();
}

void CommandHandler::reboot_esp() {
    print_heap();  
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"R\":\"%s\"}", "Device is rebooting !");    
    publish_message();
    SERIAL_PRINTLN("\n *** OK. Device will reboot now... ***");
    delay(2000);
    ESP.restart();    
}

void CommandHandler::auto_mode () {
    setManualOverride(false);
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"A\":\"%s\"}", modes[0]);    
    publish_message();
}

void CommandHandler::manual_mode () {
    byte m = setManualOverride(true);
    if (m < 0 || m > 1) {
        SERIAL_PRINTLN("--- Invalid mode -----");
        return;
    }
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"A\":\"%s\"}", modes[m]);    
    publish_message();
}

void CommandHandler::send_mode() {
    byte m = getManualOverride();
    if (m < 0 || m > 1) {
        SERIAL_PRINTLN("--- Invalid mode -----");
        return;
    }    
    SERIAL_PRINT ("Control Mode: ");
    SERIAL_PRINTLN (modes[m]);
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"A\":\"%s\"}", modes[m]);    
    publish_message();
}

void CommandHandler::print_heap() {
    SERIAL_PRINT("Free Heap: "); 
    SERIAL_PRINTLN(ESP.getFreeHeap()); //Low heap can cause problems  
}
//--------------------------------------------------------------------------------------

#define  NUM_COMMANDS    14  // excluding on and off commands
// do not change the order of the following strings !
const char* commands[] = { "STA", "VER", "MAC", "GRO", "ORG", "HEA", "REB", "DEL", "UPD", "AUT", "MAN", "MOD", "BL1", "BL2" };

void CommandHandler::handle_command(const char* command_string) {
    if (strlen (command_string) < 3) {
        SERIAL_PRINTLN ("Command can be: STA,UPD,VER,MAC,HEA,DEL,GRO,ORG,RES,ONx,OFx");
        return;
    }
    // ON commands: ON0, ON1 etc
    if (command_string[0]=='O' && command_string[1]=='N') { 
        if (command_string[2] < '0' ||  command_string[2] >= num_relays+'0') 
            SERIAL_PRINTLN ("-- Error: Invalid relay number --");
        else
            relay_on(command_string[2] - '0'); // this sends the status also 
        print_heap();
        return;
    }
    // OFF commands: OF0, OF1 etc.
    if (command_string[0]=='O' && command_string[1]=='F') { 
        if (command_string[2] < '0' ||  command_string[2] >= num_relays+'0') 
            SERIAL_PRINTLN ("-- Error: Invalid relay number --");
        else
            relay_off(command_string[2] - '0'); // this sends the status also 
        print_heap();
        return;
    }
    int command_index = -1;
    for (int i=0; i<NUM_COMMANDS; i++) {
        if (strcmp(command_string, commands[i]) == 0) {
            command_index = i;
            break;
        }
    }
    if (command_index < 0) {
       SERIAL_PRINTLN ("-- Error: Invalid command --");
       print_heap();
       return;
    }
    switch (command_index) {
        case 0: // STA
            send_status();
            break;
        case 1: // VER
            send_version();
            break;
        case 2: // MAC
            send_mac_address();
            break;     
        case 3:  // GRO
            send_group();
            break;
        case 4: // ORG
            send_org();
            break;
        case 5:  // HEA
            send_heap();
            break;   
        case 6:  // REB
            reboot_esp();
            break;
        case 7:  // DEL
            reset_wifi();
            break;
        case 8:  // UPD
            check_for_updates();
            break;     
        case 9:  // AUT
            auto_mode();
            break;
        case 10: // MAN
            manual_mode();
            break;                
        case 11: // MOD
            send_mode();
            break;               
        case 12: // BL1
            blinker(1);
            break;                
        case 13: // BL2
            blinker(2);
            break;               
        default :
            SERIAL_PRINTLN ("-- Error: Invalid command index --");
            break;                              
    }
    print_heap();
}





 
