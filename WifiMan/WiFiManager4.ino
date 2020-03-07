/*********
// Wifi manager tutorial from:
https://github.com/tzapu/WiFiManager/blob/master/examples/OnDemandConfigPortal/OnDemandConfigPortal.ino
NOTE: This uses the older Wifi Manaer by tzapu, but it is simpler and neat, and it works !

At any time during the main loop, you can press the GPIO0 (Flash) button to open the WiFi Manager portal
*********/

#include <ESP8266WiFi.h>
#include <WiFiManager.h>         // https://github.com/tzapu/WiFiManager

// Assign output variables to GPIO pins
const int led1 = 2;  
const int led2 = 16;   
const int button = 0;  // GPIO 0, Flash button

#define SOFT_AP_SSID    "MY_OWN_AP"
#define SOFT_AP_PASSWD  "my_password"

void setup() {
    Serial.begin(115200);
    Serial.println ("\n\n---------------  Wifi Manager with button starting... -----------");
    pinMode(button, INPUT_PULLUP);
    pinMode(led1, OUTPUT);
    pinMode(led2, OUTPUT);
    digitalWrite(led1, HIGH); // active low
    digitalWrite(led2, HIGH);  
    blink();
    // connect if credentials are available, or start the config portal
    wifi_manager_auto_connect();  
  }

void loop(){
    if (digitalRead(button)==LOW) 
       handle_button();
    digitalWrite(led1, LOW);
    delay(200);
    digitalWrite(led1, HIGH);
    delay(200);
}

void handle_button() {
    digitalWrite(led2, HIGH);  // ensure it is off
    for (int i=0; i<12; i++)  // long press = 3 seconds
    {
        delay(250);   
        if (digitalRead(button)==HIGH)   
            return;
    }
    start_wifi_manager_portal(); 
}

void wifi_manager_auto_connect() {
    // WiFiManager local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    
    digitalWrite(led2, LOW);  // entering Wifi config mode
    // Uncomment and run this once, if you want to erase all the stored wifi credentials:
    //wifiManager.resetSettings();
    
    // if you want a custom ip for the wifi manager portal
    //wifiManager.setAPConfig(IPAddress(10,0,1,10), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
    Serial.println ("If this is the first time, we will start Wifi configuration portal.");
    Serial.println ("In that case, point your browser to 192.168.4.1");
  
     // make it reset and retry,or go to sleep
     // wifiManager.setTimeout(120);  //in seconds
      
    // fetches ssid and passwd from eeprom and tries to connect;
    // if it cannot connect, it starts an access point 
    // and goes into a *blocking* loop awaiting configuration
    wifiManager.autoConnect(SOFT_AP_SSID);
    // -- OR --
    // If you want to password protect your AP:
    // wifiManager.autoConnect(SOFT_AP_SSID, SOFT_AP_PASSWD)
    // -- OR --
    // or use this for auto generated name ESP + ChipID
    //wifiManager.autoConnect();
  
    digitalWrite(led2, HIGH);  // exiting wifi config mode
    // if you get here, you have connected to the WiFi
    Serial.print("Connected to your Wifi network: ");
    Serial.println(WiFi.SSID());
    Serial.println("Local IP address: ");
    Serial.println(WiFi.localIP());     
}

void start_wifi_manager_portal() {
    digitalWrite(led2, LOW); // indicates start of config mode
    // WiFiManager local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;
    
    digitalWrite(led2, LOW);  // entering Wifi config mode
    // Uncomment and run this once, if you want to erase all the stored wifi credentials:
    //wifiManager.resetSettings();
    
    // if you want a custom ip for the wifi manager portal
    //wifiManager.setAPConfig(IPAddress(10,0,1,10), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
    
    // make it reset and retry, or go to sleep
    // wifiManager.setTimeout(120);  //in seconds     
     
    Serial.println ("On your request, Wifi configuration portal is starting....");
    Serial.println ("Point your browser to 192.168.4.1");
     
    if (!wifiManager.startConfigPortal(SOFT_AP_SSID)) {
      Serial.println("failed to connect to Wifi and timed out");
      delay(5000);
      //reset and try again, or maybe put it to deep sleep
      ESP.reset();
      delay(5000);
    }
    // -- OR --
    // If you want to password protect your AP:
    // wifiManager.startConfigPortal(SOFT_AP_SSID, SOFT_AP_PASSWD)
    // -- OR --
    // or use this for auto generated name ESP + ChipID
    //wifiManager.startConfigPortal();
        
    digitalWrite(led2, HIGH);  // exiting wifi config mode
    // if you get here, you have connected to the WiFi
    Serial.print("Successfully connected to your Wifi network: ");
    Serial.println(WiFi.SSID());
    Serial.println("Local IP address: ");
    Serial.println(WiFi.localIP());    
    digitalWrite(led2, HIGH); // indicates end of config mode    
}

void blink() {
    for (int i=0; i<5; i++) {
      digitalWrite(led1, LOW);
      delay(200);
      digitalWrite(led1, HIGH);
      delay(200);
    }
}
