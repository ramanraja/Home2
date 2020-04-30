// CommandHandler.cpp
 
#include "CommandHandler.h" 
#include "hardware.h"

const char* modes[] = {"AUTO", "MANUAL"};  // NOTE: boolean manual_override is used as index into this array

// external callback functions defined in main .ino file 
void reset_wifi();
void check_for_updates();
bool is_night_now(); // this gets the cached value in main .ino
#ifndef PORTICO_VERSION 
  bool is_occupied();
#endif


CommandHandler::CommandHandler() {
}
 
void CommandHandler::init(Config *pconfig, PubSubClient *pclient, Hardware *phardware) {
    pC = pconfig;
    pClient = pclient;
    pHard = phardware;
}
 
// takes the global status message and publishes it
// NOTE: You MUST set up the status_message before calling this !
void CommandHandler::publish_message () {
    SERIAL_PRINT(F("Publishing: "));
    SERIAL_PRINTLN(status_msg);
    pClient->publish(pC->mqtt_pub_topic, status_msg);
}

// TODO: In the following two cases, add additional overloaded methods: they should
// take the status or data as function arguments (push model)
void CommandHandler::send_status () {  
    // this is pull model; in response to an MQTT command
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"S\":\"%s\"}", pHard->getStatus());    
    publish_message();
}

void CommandHandler::send_data () { // TODO: take the status as an argument?
    if (data_paused) {
        send_paused_msg();
        return; 
    }
    // TODO: introduce num_relays into the following !   
    const data* dat = pHard->getData();  // this is pull model; in response to an MQTT command; gets the last known values (not current)
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"D\":{\"S\":\"%s\",\"T\":%.1f,\"H\":%.1f,\"I\":%.1f,\"L\":%d,\"P\":%d,\"R\":%d}}",  
              pHard->getStatus(),dat->temperature, dat->humidity, dat->hindex, dat->light, dat->pir_hits, dat->radar_hits);    
    publish_message();
}

void CommandHandler::get_param(const char* param) {
     snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"P\":\"%s\"}", pC->get_param(param)); 
     publish_message();
}

void CommandHandler::send_paused_msg() {
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"I\":\"DATA_PAUSED\"}");    
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

// To check whether we are following active low or active high
void CommandHandler::send_active_onoff() {
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"L\":\"ON:%d, OFF:%d\"}", pC->ON,pC->OFF);    
    publish_message();
}

void CommandHandler::send_is_night() {
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"L\":\"IS_NIGHT: %d\"}", is_night_now());    
    publish_message();
}

void CommandHandler::send_is_occupied() {
#ifndef PORTICO_VERSION  
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"L\":\"IS_OCCUPIED: %d\"}", is_occupied());    
    publish_message();
#else
    SERIAL_PRINTLN(F("is_occupied flag is not valid for Portico Controller"));    
#endif
}

// This rebooting happens in response to a remote MQTT command
void CommandHandler::reboot() {
    print_heap();  
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"L\":\"%s\"}", "Device is rebooting !");    
    publish_message();
    pHard->reboot_esp();
}

void CommandHandler::auto_mode () {
    manual_override = false;
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"A\":\"%s\"}", modes[0]); // 0=auto, 1=manual   
    publish_message();
}

void CommandHandler::manual_mode () {
    manual_override = true;
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"A\":\"%s\"}", modes[1]); // 0=auto, 1=manual   
    publish_message();
}

void CommandHandler::send_mode() {
    byte m = (byte)manual_override;  // 0=auto, 1=manual
    SERIAL_PRINT(F("Control Mode: "));
    SERIAL_PRINTLN (modes[m]);
    snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"A\":\"%s\"}", modes[m]);    
    publish_message();
}

// Downloads TLS certificates and config.txt file. If success, restarts the device
void CommandHandler::download_certificates(){
    print_heap();
    int result = pC->download_certificates();
    print_heap();
    if (result==CODE_OK) {
        snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"I\":\"%s\"}", "TLS certificates updated.");  
        publish_message();
        SERIAL_PRINTLN(F("[Config] Restarting ESP..."));
        yield();
        delay(2000);
        ESP.restart();
    } else {
        snprintf (status_msg, MAX_MSG_LENGTH-1, "{\"I\":\"%s%s\"}", "TLS certificates failed: ", pC->get_error_message(result));  
        publish_message();      
    }
} 
//--------------------------------------------------------------------------------------

#define  NUM_COMMANDS    21  // excluding on and off commands; index runs from 0 to NUM_COMMANDS-1
// NOTE: If you change the order of the following strings, you must change the switch cases also !
const char* commands[] = { "STA", "VER", "MAC", "GRO", "ORG", "HEA", "REB", "DEL", "UPD", "AUT", 
                           "MAN", "MOD", "BL0", "BL1", "DAT", "CER", "LOJ", "ISN", "OCC", "PAU", "RES" };

void CommandHandler::handle_command(const char* command_string) {
    if (strlen (command_string) < 3) {
        SERIAL_PRINTLN(F("Command can be: STA,UPD,VER,MAC,HEA,DEL,GRO,ORG,RES,ONx,OFx etc"));
        return;
    }
    // ON commands: ON0, ON1 etc
    if (command_string[0]=='O' && command_string[1]=='N') { 
        if (command_string[2] < '0' ||  command_string[2] >= num_relays+'0') 
            SERIAL_PRINTLN(F("-- Error: Invalid relay number --"));
        else
            pHard->relay_on(command_string[2] - '0'); // this sends the status also 
        print_heap();
        return;
    }
    // OFF commands: OF0, OF1 etc.
    if (command_string[0]=='O' && command_string[1]=='F') { 
        if (command_string[2] < '0' ||  command_string[2] >= num_relays+'0') 
            SERIAL_PRINTLN(F("-- Error: Invalid relay number --"));
        else
            pHard->relay_off(command_string[2] - '0'); // this sends the status also 
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
       SERIAL_PRINTLN(F("-- Error: Invalid command --"));
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
            reboot();
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
        case 12: // BL0
            pHard->blink_led(0, BLINK_COUNT);
            break;                
        case 13: // BL1
            pHard->blink_led(1, BLINK_COUNT);
            break;       
        case 14: // DAT
            send_data(); // on-demand data
            break;            
        case 15: // CER
            download_certificates(); // TLS certificates and config.txt file
            break;                        
        case 16: // LOJ : on/off logic levels
            send_active_onoff();
            break;
        case 17: // ISN
            send_is_night();
            break;
        case 18: // OCC
            send_is_occupied();
            break;
        case 19: // PAU            
            data_paused = true;
            break;
        case 20: // RES            
            data_paused = false;
            break;            
        default :
            SERIAL_PRINTLN(F("-- Error: Invalid command index --"));
            break;                              
    }
    print_heap();
}





 
