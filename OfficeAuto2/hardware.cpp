// hardware.cpp

#include "hardware.h"
#include "CommandHandler.h"  //forward declaration in hardware.h: class CommandHandler; 

const int dht_pin = D7; // DHT22 is at GPIO13
DHT dht(dht_pin, DHT22);   // this is actually a macro, so it refuses to go inside the class !
    
Hardware::Hardware () {
}

void Hardware::init (Config *configptr, Timer *timerptr, CommandHandler* cmdptr) {
  this->pC = configptr;
  this->pT = timerptr;
  this->pCmd = cmdptr;
  init_serial();
  init_hardware();
}

// The output is ternary: day,night,unknown
short Hardware::is_night_time() {
    int lite = 0;
    for (int i=0; i<5; i++) {
        lite += analogRead(ldr);
        delay(5);
    } 
    lite = (int) (lite/5);  
    lite = MAX_LIGHT - lite;
    if (lite > (MAX_LIGHT-NOISE_THRESHOLD)) // light sensor failure threshold=10
        return TIME_UNKNOWN;
    if (lite < pC->night_light_threshold)    
        return TIME_NIGHT;
    if (lite > pC->day_light_threshold)         
        return TIME_DAY;
    return TIME_UNKNOWN;   // in the buffer zone for Schmidt trigger
}

const char* Hardware::getStatus() {
    snprintf (relay_status_str, 3, "%1d%1d", relay_status[1], relay_status[0]); // NOTE: 3 is hard coded
    return ((const char *)relay_status_str);
}

const data* Hardware::getData() {
    return ((const data *)&sensor_data);
}

bool Hardware::getPir() {
    return digitalRead(pir);
}

bool Hardware::getRadar() {
    return digitalRead(radar);
}

bool Hardware::showPirStatus() {
    pir_status =  digitalRead(pir);
    digitalWrite(led1, !pir_status);
    return (pir_status);
}

bool Hardware::showRadarStatus() {
    radar_status =  digitalRead(radar);
    digitalWrite(led2, !radar_status);
    return (radar_status);
}

void Hardware::reboot_esp() {
    SERIAL_PRINTLN("\n *** Intof IoT Device will reboot now... ***");
    delay(2000);
    ESP.restart();    
}
//-------------------------------------------------------------------------
// * this is an infinite loop *
void Hardware::infinite_loop() {
    pinMode(D0, OUTPUT);  // hard coded: Node MCU built in LEDs
    pinMode(D4, OUTPUT);
    static bool temp = 0;
    while (true)  {  // NOTE: infinite loop outside main loop *
      digitalWrite(D0, temp);   
      digitalWrite(D4, !temp); 
      temp = !temp;
      delay(1000);
    }
}
//---------------  application specific --------------------------------

// this function is typically called once in a minute; it reads the sensors, and
// increments a count. When the count reaches, say, 5 then it computes the average
// and asks the Hardware to send the data or operate the lights as needed.
// returns true if it is time to send a status report to server, false otherwise

bool Hardware::read_sensors() {
    read_dht_ldr(); // low level read
    sensor_reading_count++;  // increment for 0 to 4
    // and when the count reaches 5, trigger send_data and return true. Till then, return false.
    if (sensor_reading_count < pC->status_report_frequency) 
        return false;  // not yet time to send data or to operate the lights
    
    // ..now the time is ripe to send data or operate the lights   
    SERIAL_PRINTLN("{Hardware] preparing to send sensor data..");
    print_data();
    if (sensor_reading_count > 0)
        light = (int)light/sensor_reading_count;  // assumption, light was read correctly all the times
    sensor_reading_count = 0; // reset it for the next time (but only after using it to average light!)
    
    if (valid_reading_count > 0) {
        temperature = temperature/valid_reading_count;
        humidity = humidity/valid_reading_count;
    }
    valid_reading_count = 0; // prepare for next time

    safe_strncpy (sensor_data.relay_status, getStatus(), MAX_RELAYS); // dest,src,size
    sensor_data.temperature = temperature;  // this->temperature
    sensor_data.humidity = humidity;        // this->humidity
    sensor_data.hindex = dht.computeHeatIndex(temperature, humidity, false); // here the default is Farenheit !  
    if (sensor_data.hindex < 0)
        sensor_data.hindex = 0.0f;
    sensor_data.light = MAX_LIGHT-light;
    sensor_data.pir_hits = 0;  // TODO: implement these
    sensor_data.radar_hits = 0;    
    temperature = 0.0f;
    humidity = 0.0f;
    light = 0;
    // TODO: reset pir_hits and radar_hits also !     
    return true;  // true = the main class should determine day or night and save it
} 

float t, h;
// adds the new readings to a corresponding buffer variable, so that average can be taken before sending data
void Hardware::read_dht_ldr() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)    
  t = dht.readTemperature();  // here the default is Celcius (but Farenheit is the default for heat index)
  h = dht.readHumidity();
  if (isnan(t) || isnan(h)) 
    SERIAL_PRINTLN("--- cannot read temperature sensor ---"); 
  else {
      temperature += t;
      humidity += h;
      valid_reading_count++;
  }
  light += analogRead(ldr); // assumption: light is always valid !
  //print_data();  // just for debugging
}  

void Hardware::print_data() {
  SERIAL_PRINT("[RAW] temp: "); SERIAL_PRINT(temperature); SERIAL_PRINT("\t");
  SERIAL_PRINT("humi: "); SERIAL_PRINT(humidity); SERIAL_PRINT("\t");
  SERIAL_PRINT("light: "); SERIAL_PRINTLN(light);    
}
//--------------- end application specific -----------------------------

// NOTE: the round PCB hardware can only transmit; the serial Rx line is taken away for an LED.
void Hardware::init_serial() {
  #ifdef ENABLE_DEBUG
      Serial.begin(BAUD_RATE); 
      #ifdef VERBOSE_MODE
        Serial.setDebugOutput(true);
      #endif
      Serial.setTimeout(250);
  #endif    
  SERIAL_PRINTLN("\n\n ***************** AWS IoT X-509 client starting.. ******************\n\n");  
  SERIAL_PRINT("FW version: ");
  SERIAL_PRINTLN(FIRMWARE_VERSION);
}

// internal function
void Hardware::init_hardware() {
    for (int i=0; i<NUM_RELAYS; i++) {
    pinMode(relay_pin[i], OUTPUT);
    digitalWrite(relay_pin[i],pC->OFF);  
  }  
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(pir, INPUT);
  pinMode(radar, INPUT);
  dht.begin();
  blink1();
}
  
// these two are invoked by algorithm based on light level or time of the day or movement
void Hardware::primary_light_on() {
  digitalWrite (relay_pin[pC->primary_relay], pC->ON);
  relay_status[pC->primary_relay] = 1;
  //SERIAL_PRINTLN("Primary light is ON"); // Portico: this will be called once in 5 minutes...
  //////pCmd->send_status();   // ... and Bath room: triggered by movements
}

void Hardware::primary_light_off() {
  digitalWrite (relay_pin[pC->primary_relay], pC->OFF);
  relay_status[pC->primary_relay] = 0;
  //SERIAL_PRINTLN("Primary light is OFF");   // this will be called once in 5 minutes!
  //////pCmd->send_status();   // do not flood the server !
}       
        
// these two are invoked by remote MQTT command
// either of the two relays can be operated
void Hardware::relay_on (short relay_number) {
  SERIAL_PRINT ("Remote command: ON; Relay : ");
  SERIAL_PRINTLN (relay_number);  
  digitalWrite (relay_pin[relay_number], pC->ON);
  relay_status[relay_number] = 1;
  pCmd->send_status();  // TODO: send the status as an argument? NO. even the main .ino can send data
}

void Hardware::relay_off (short relay_number) {
  SERIAL_PRINT ("Remote command: OFF; Relay : ");
  SERIAL_PRINTLN (relay_number);    
  digitalWrite (relay_pin[relay_number], pC->OFF);
  relay_status[relay_number] = 0;
  pCmd->send_status();  // TODO: send the status as an argument? NO. even the main .ino can send data
}
//-------------------------------------------------------------------------

void Hardware::blink1() {
  for (int i=0; i<4; i++) {
      digitalWrite (led1, LOW);  // active low 
      delay(250);
      digitalWrite (led1, HIGH);  // active low 
      delay(250);               
  }
}

void Hardware::blink2() {
  for (int i=0; i<6; i++) {
      digitalWrite (led1, LOW);  // active low, green LED, for PIR
      delay(100);
      digitalWrite (led1, HIGH);  // active low
      delay(100);               
  }
}

void Hardware::blink3() {
  for (int i=0; i<8; i++) {
      digitalWrite (led2, LOW);  // active low 
      delay(50);
      digitalWrite (led2, HIGH);  // active low 
      delay(50);               
  }
}

// ASSUMPTION: there are 2 LEDs, so the caller supplies 0 or 1 as the argument
// to the following functions
void Hardware::led_on (byte led_num) {
    digitalWrite(LEDS[led_num], LOW); // active low
}

void Hardware::led_off (byte led_num) {
    digitalWrite(LEDS[led_num], HIGH);   
}

void Hardware::set_led (byte led_num, bool is_on) {
    digitalWrite(LEDS[led_num], !is_on);  // active low 
}

void Hardware::blink_led (byte led_num, byte times) {
    if (led_num==1)
        pT->oscillate(led1, 300, HIGH, times);
    else if (led_num==2)
        pT->oscillate(led2, 300, HIGH, times);
    else // coding bug
        SERIAL_PRINTLN("\n------- Invalid LED number -------\n");        
}
