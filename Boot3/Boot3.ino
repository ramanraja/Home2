/*
Boot Loader stub program:
 - Reads basic configurations from a file config.txt in SPIFF
 - Gets Wifi credentials with Tzapu Wifi Manager.
 - Downloads TLS certificates and config.txt. This config.txt should have the links to your business program.
 - Updates bootloader.bin firmware to its latest version through OTA & then restarts.
 - (If there is no update for bootloader, goes into an idle loop. Now manually restart the device).
 - It takes the new OTA details from the config.txt and downloads your business logic application.
new in this version:
 - uniform string versions of HTTP error codes for certificate download and OTA updates
*/
#include "main.h"

Config C;
MyFiManager myfi;
OtaHelper ota;

short red_led = D0;  // D3;
short green_led = D4;  // D9;

void setup() {
    init_hardware();
    init_serial();
    C.init(); 
    C.dump();  // this requires serial port to have been initialized
    init_wifi();   // this restarts ESP if failed
    print_heap();
    yield();
    delay(1000);   
    if (download_files()) { // ASSUMPTION: we briefly activate the download URL only when needed *
        fast_blink(green_led);
    else
        fast_blink(red_led);
    print_heap();
    ota.init(&C);    
    int ota_result = ota.check_and_update();
    
    // >>> will not reach here if the update succeeds <<<
    SERIAL_PRINT ("Reached after OTA-check for updates: ");
    SERIAL_PRINTLN (C.get_error_message(ota_result));
    fast_blink(green_led);      
    print_heap(); 
}

int count = 0;
void loop() {
    SERIAL_PRINT(".");
    count++;
    if (count % 50 == 0) {
        SERIAL_PRINTLN();
        print_heap();
    }
    delay(4000);
    if (count >= 1000) {
        //SERIAL_PRINTLN("----- Restarting ESP... ----------");
        //ESP.restart();
        count = 0;
    }
}

void  init_wifi() {
    led_off(red_led);
    led_on(green_led); // indicates start of wifi config mode
    bool wifi_result = myfi.init();  // this will start the web portal if no WiFi credentials are found
    led_off(green_led); // end of wifi config mode    
    print_heap();
    if (!wifi_result) {
        SERIAL_PRINTLN("\n-------- No Wifi connection! restarting... ------");
        fast_blink (red_led);
        led_on(red_led);
        delay(5000);
        ESP.restart();
    }
}

/*
    ASSUMPTION: The download files are publicly available on AWS cloud/local server only for a brief window
    when you need to update a new device. Otherwise they are open to misuse. 
*/
bool download_files() {
    SERIAL_PRINTLN("Downloading certificate files..");
    Downloader D; // local, adhoc variable
    D.init(&C);
    int result = D.download_files();
    SERIAL_PRINT ("Downloader result: ");
    SERIAL_PRINTLN (C.get_error_message(result));
    return (result==CODE_OK);  // 0
}

void  init_serial() {
  #ifdef ENABLE_DEBUG
      Serial.begin (BAUD_RATE); 
      #ifdef VERBOSE_MODE
        Serial.setDebugOutput (true);
      #endif
      Serial.setTimeout (250);
  #endif    
  SERIAL_PRINTLN ("\n\n***************** Intof IoT boot loader starting.. ******************\n");  
//  SERIAL_PRINT ("FW version: ");
//  SERIAL_PRINTLN (FIRMWARE_VERSION);
}

void init_hardware ()
{
    pinMode (red_led, OUTPUT);
    pinMode (green_led, OUTPUT);
    led_off(red_led);
    slow_blink (green_led);
}

void led_off (short led) {
    digitalWrite (led, HIGH);
}

void led_on (short led) {
    digitalWrite (led, LOW);
}

void slow_blink (short led) {
    for (int i=0; i<5; i++) {
         digitalWrite (led, LOW);
         delay(300);
         digitalWrite (led, HIGH);
         delay(300);
    }
}

void fast_blink (short led) {
    for (int i=0; i<10; i++) {
         digitalWrite (led, LOW);
         delay(100);
         digitalWrite (led, HIGH);
         delay(100);
    }
} 
