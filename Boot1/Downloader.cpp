// Downloader.cpp
// Downloads certificate files from the cloud/local server and saves it on the SPIFF of 8266

#include "Downloader.h"

void Downloader::init (Config* configptr) {
    this->pC = configptr;
}

int Downloader::download_files() {
    if (WiFi.status() != WL_CONNECTED) {
        SERIAL_PRINTLN ("\nNo Wifi, no http!");
        return 1; // NO_WIFI;
    }
    WiFiClient wifi_client;
    HTTPClient http;
    int update_check_result = check_certificate_version (http, wifi_client);
    if (update_check_result != 0) // CODE_OK
        return update_check_result;  // bubble it up
    if (!SPIFFS.begin()) {
        Serial.println("--- Failed to mount file system. ---");
        return 2; // SPIFF_FAILED
    }
    list_files();
    int num_files = pC->get_num_files();
    for (int i=0; i<num_files; i++) {
        int result = save_file (http, wifi_client, i);   // TODO: try secondary URL if this fails
        SERIAL_PRINT("File download result code: ");
        SERIAL_PRINTLN(result);
    }
    SERIAL_PRINTLN("Config file contents: "); 
    print_file(pC->file_names[0]); // NOTE: config.txt must be the first file in the list
    list_files();
    SPIFFS.end();  
    return 0;  // CODE_OK
}
    
int Downloader::check_certificate_version (HTTPClient& http, WiFiClient& wifi_client) {    
    char* url = (char*) pC->get_primary_certificate_version_url(); 
    SERIAL_PRINTLN("Connecting to URL: ");
    SERIAL_PRINTLN (url);
    if (!http.begin(wifi_client, url)) {  // this may redirect to HTTPS
        SERIAL_PRINTLN ("HTTP client failed to connect !");
        // begin() failed, so no end() is required
        return 3;  // CONNECTION_FAILED 
    }
    // get the headers
    SERIAL_PRINTLN("HTTP GETting...");
    int response_code = http.GET();
    SERIAL_PRINT ("HTTP response code: ");
    SERIAL_PRINTLN (response_code);
    if (response_code <= 0) {  // negative values: library's error codes
        SERIAL_PRINT ("HTTP GET failed: ");
        SERIAL_PRINTLN(http.errorToString(response_code).c_str()); // the strings are defined only for negative codes
        http.end();
        return 4;  // HTTP_FAILED
    }
    // file found at server, or the library automatically redirected to new location
    if(response_code >= 200 && response_code <= 308) { // code 304 = content unmodified (TODO: revisit)
        String str_version = http.getString();
        SERIAL_PRINT("Current certificate version: ");
        SERIAL_PRINTLN(pC->current_certificate_version);
        SERIAL_PRINT("Available certificate version: ");
        SERIAL_PRINTLN(str_version);
        int newVersion = str_version.toInt();
        
        http.end();
        
        if(newVersion > pC->current_certificate_version) {
            SERIAL_PRINTLN ("A new certificate is available");
            return 0;   // CODE_OK
         }  else  {
            SERIAL_PRINTLN("This device already has the latest certificate.");   
            return 5;  // NO_UPDATES     
         }
    }    
    if (response_code > 308 && response_code < 400) {
        SERIAL_PRINT("Possible redirection to: ");
        SERIAL_PRINTLN(http.getLocation());
        http.end();
        return 6;   // NO_FILE  todo: revisit       
    }
    SERIAL_PRINTLN ("--- HTTP GET: File Not found/rejected ---");
    http.end();
    return 7; // NO_ACCESS
}

int Downloader::save_file (HTTPClient& http, WiFiClient& wifi_client, int file_index) {
    //if (WiFi.status() != WL_CONNECTED) {
     //   SERIAL_PRINTLN ("\nNo Wifi, no http!");
     //   return 1; // NO_WIFI; 
    //}
    SERIAL_PRINTLN("\nConnecting to HTTP server: ");
    const char* url = pC->get_primary_certificate_url(file_index);
    SERIAL_PRINTLN (url);
    if (!http.begin(wifi_client, url)) {   
        SERIAL_PRINTLN ("HTTP client failed to connect !");
        // begin() failed, so no end() is required
        return 3;  // CONNECTION_FAILED 
    }
    // get the headers
    SERIAL_PRINT ("Downloading file...");
    int response_code = http.GET();
    SERIAL_PRINT ("HTTP response code: ");
    SERIAL_PRINTLN (response_code);
    if (response_code <= 0) {
        SERIAL_PRINT ("HTTP GET failed: ");
        SERIAL_PRINTLN(http.errorToString(response_code).c_str()); // the strings are defined only for negative codes
        http.end();
        return 4;  // HTTP_FAILED
    }
    // file found at server:o(r the library automatically redirected to new location?)
    //// if(response_code >= 200 && response_code <= 308) { // code 304 = content unmodified (TODO: revisit)
    if (response_code == HTTP_CODE_OK) {
        int spiff_result = write_to_spiff (http, file_index); // the http client is passed to the SPIFF stream
        http.end();
        return spiff_result;        
    }    
    SERIAL_PRINTLN ("--- HTTP GET failed. ---");
    http.end();
    return 6;   // NO_ACCESS  todo: revisit
}

int Downloader::write_to_spiff (HTTPClient& http, int file_index) {
    ///String target_file_name = String("/")+ pC->file_names[file_index];
    File f = SPIFFS.open(pC->file_names[file_index], "w");
    if (!f.isFile()) {
      SERIAL_PRINTLN ("Failed to open file for writing.");
      return 11; // FILE_OPEN_ERROR
    }
    // the following call returns bytes written (negative values are error codes)
    int writer_result = http.writeToStream(&f);
    f.close();
    if (writer_result > 0) {
      SERIAL_PRINT ("Bytes written to SPIFF: ");
      SERIAL_PRINTLN (writer_result);
      f.flush(); // TODO: revisit
    }
    else  {
      SERIAL_PRINT ("Failed to write to file stream. Code: ");
      SERIAL_PRINTLN (writer_result);
      return 12;   // FILE_WRITE_ERROR   
    }
    // success: result code is 0
    return 0;  // SPIFF_OK
}

void Downloader::list_files() {
    //SPIFFS.begin();
    SERIAL_PRINTLN ("Listing file sizes in your root folder..");
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
        SERIAL_PRINT(dir.fileName());
        SERIAL_PRINT("    ");
        File f = dir.openFile("r");
        SERIAL_PRINTLN(f.size());
        f.close();  // this is needed. see:
    }
    //dir.close(); // this call does not exist. 
    //SPIFFS.end(); // TODO: revisit
}

void Downloader::print_file (const char* file_name) {
    SERIAL_PRINT ("Opening SPIFF file for reading: ");
    SERIAL_PRINTLN (file_name);    
    if (!SPIFFS.exists(file_name)) {
        SERIAL_PRINT ("The file is missing ! ");
        return;
    }
    File f = SPIFFS.open(file_name, "r");
    if (!f.isFile()) {
      SERIAL_PRINTLN ("Failed to open file for reading.");
      return;
    }
    SERIAL_PRINTLN ("File opened. Contents:\n");
    while(f.available()) {
      String line = f.readStringUntil('\n');
      SERIAL_PRINTLN(line);
    }    
    f.close();
}
 
