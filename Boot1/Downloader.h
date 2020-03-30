//Downloader.h
// Downloads a file from the internet and saves it on the SPIFF of 8266

#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include "common.h"
#include "config.h"

#include <FS.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266HTTPClient.h>
//#include <WiFiClientSecureBearSSL.h>  // part of Arduino 8266 library
 
 
class Downloader {
public:
    void init(Config* configptr);
    int download_files();
    void list_files(); 
    void print_file (const char* file_name);
private:
    Config* pC;
    //////HTTPClient http;
    int check_certificate_version (HTTPClient& http, WiFiClient& wifi_client);
    int save_file (HTTPClient& http, WiFiClient& wifi_client, int file_index);
    int write_to_spiff (HTTPClient& http, int file_index);  
};
 
#endif
