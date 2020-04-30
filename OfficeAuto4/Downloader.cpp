// Downloader.cpp
// Downloads certificate files from the cloud/local server and saves it on the SPIFF of 8266

#include "Downloader.h"

void Downloader::init (Config* configptr) {
    this->pC = configptr;
}

int Downloader::download_files() {
    if (WiFi.status() != WL_CONNECTED) {
        SERIAL_PRINTLN(F("\nNo Wifi, no http!"));
        return NO_WIFI;   
    }

    SERIAL_PRINTLN(F("Downloading certificate from primary URLs..."));
    use_backup_urls = false;
    //////// HTTPClient http; <- this is wrong; never reuse a HTTPClient object for the backup server:
    int result = check_certificate_version ();  // let check_certificate_version() create its own local HTTPClient object
    if (result != CODE_OK) { // 0               
        use_backup_urls = true;
        SERIAL_PRINTLN(F("Primary certificate URL failed; trying backup URLs..."));
        result = check_certificate_version (); // never reuse the client for a different web server !
        if (result != CODE_OK) // 0
            return result;  // bubble it up
    }
    if (!SPIFFS.begin()) {
        Serial.println("--- Failed to mount file system. ---");
        return SPIFF_FAILED; 
    }
    list_files();
    // you can reuse the same HTTP client for multiple file downloads, provided they are on the same server *
    WiFiClient wifi_client; 
    HTTPClient http;
    int num_files = pC->get_num_files();
    for (int i=0; i<num_files; i++) {
        int result = save_file (http, wifi_client, i);   
        SERIAL_PRINT(F("File download result code: "));
        SERIAL_PRINTLN(pC->get_error_message(result));
    }
    SERIAL_PRINTLN(F("Config file contents: ")); 
    print_file(pC->file_names[0]); // NOTE: config.txt must be the first file in the list
    list_files();
    SPIFFS.end();  
    return CODE_OK;  // 0
}

// NOTE: never reuse the same HTTPClient object for a different web server!    
int Downloader::check_certificate_version () { // (HTTPClient& http, WiFiClient& wifi_client) {    
    char* url;
    if (use_backup_urls)
        url = (char*) pC->get_secondary_certificate_version_url(); 
    else
        url = (char*) pC->get_primary_certificate_version_url(); 
    SERIAL_PRINTLN(F("Connecting to URL: "));
    SERIAL_PRINTLN (url);
    WiFiClient wifi_client;
    HTTPClient http;
    if (!http.begin(wifi_client, url)) {   
        SERIAL_PRINTLN(F("--- Malformed URL ---"));
        // begin() failed, so no end() is required
        return BAD_URL;  
    }
    // get the headers
    SERIAL_PRINTLN(F("HTTP GETting..."));
    int response_code = http.GET();
    SERIAL_PRINT(F("HTTP response code: "));
    SERIAL_PRINTLN (response_code);
    if (response_code <= 0) {  // negative values: library's error codes
        SERIAL_PRINT(F("HTTP GET failed: "));
        SERIAL_PRINTLN(http.errorToString(response_code).c_str()); // the strings are defined only for negative codes
        http.end();
        return HTTP_FAILED;  
    }
    // file found at server, or the library automatically redirected to new location
    if(response_code == 200) {  // HTTP_CODE_OK 
        String str_version = http.getString();         
        SERIAL_PRINT(F("Current certificate version: "));
        SERIAL_PRINTLN(pC->current_certificate_version);
        SERIAL_PRINT(F("Available certificate version: "));
        SERIAL_PRINTLN(str_version);
        int newVersion = str_version.toInt();
        
        http.end();
        
        if(newVersion > pC->current_certificate_version) {
            SERIAL_PRINTLN(F("A new certificate is available"));
            return CODE_OK;   // 0
         }  else  {
            SERIAL_PRINTLN(F("This device already has the latest certificate."));   
            return NO_UPDATES;    
         }
    }    
    // code 301: content moved; 304 = content unmodified (TODO: revisit)
    if (response_code >= 300 && response_code < 400) {
        SERIAL_PRINT(F("Failed. Possible redirection : ")); // TODO: revisit
        SERIAL_PRINTLN(http.getLocation());
        SERIAL_PRINTLN(http.getString()); 
        http.end();
        return NOT_FOUND;   //  todo: revisit       
    }
    SERIAL_PRINTLN(F("--- HTTP GET: File Not found/rejected ---"));
    http.end();
    return NO_ACCESS; 
}

int Downloader::save_file (HTTPClient& http, WiFiClient& wifi_client, int file_index) {
    //if (WiFi.status() != WL_CONNECTED) {
     //   SERIAL_PRINTLN(F("\nNo Wifi, no http!"));
     //   return NO_WIFI;  
    //}
    char* url;
    if (use_backup_urls)    
        url = (char*)pC->get_secondary_certificate_url(file_index);
    else
        url = (char*)pC->get_primary_certificate_url(file_index);
    SERIAL_PRINTLN(F("\nConnecting to HTTP server: "));
    SERIAL_PRINTLN (url);
    if (!http.begin(wifi_client, url)) {   
        SERIAL_PRINTLN(F("--- Malformed URL ---"));
        // begin() failed, so no end() is required?
        return BAD_URL;   
    }
    // get the headers
    SERIAL_PRINT(F("Downloading file..."));
    int response_code = http.GET();
    SERIAL_PRINT(F("HTTP response code: "));
    SERIAL_PRINTLN (response_code);
    if (response_code <= 0) {
        SERIAL_PRINT(F("HTTP GET failed: "));
        SERIAL_PRINTLN(http.errorToString(response_code).c_str()); // the strings are defined only for negative codes
        http.end();
        return HTTP_FAILED;   
    }
    // file found at server:o(r the library automatically redirected to new location?)
    //// if(response_code >= 200 && response_code <= 308) { // code 304 = content unmodified (TODO: revisit)
    if (response_code == HTTP_CODE_OK) {
        int spiff_result = write_to_spiff (http, file_index); // the http client is passed to the SPIFF stream
        http.end();
        return spiff_result;        
    }    
    SERIAL_PRINTLN(F("--- HTTP GET failed. ---"));
    http.end();
    return NO_ACCESS;   //   todo: revisit
}

int Downloader::write_to_spiff (HTTPClient& http, int file_index) {
    ///String target_file_name = String("/")+ pC->file_names[file_index];
    File f = SPIFFS.open(pC->file_names[file_index], "w");
    if (!f.isFile()) {
      SERIAL_PRINTLN(F("Failed to open file for writing."));
      return FILE_OPEN_ERROR;  
    }
    // the following call returns bytes written (negative values are error codes)
    int writer_result = http.writeToStream(&f);
    f.close();
    if (writer_result > 0) {
      SERIAL_PRINT(F("Bytes written to SPIFF: "));
      SERIAL_PRINTLN (writer_result);
      f.flush(); // TODO: revisit
    }
    else  {
      SERIAL_PRINT(F("Failed to write to file stream. Code: "));
      SERIAL_PRINTLN (writer_result);
      return FILE_WRITE_ERROR;       
    }
    // success: result code is 0
    return CODE_OK; 
}

void Downloader::list_files() {
    //SPIFFS.begin();
    SERIAL_PRINTLN(F("Listing file sizes in your root folder.."));
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
        SERIAL_PRINT(dir.fileName());
        SERIAL_PRINT(F("    "));
        File f = dir.openFile("r");
        SERIAL_PRINTLN(f.size());
        f.close();  // this is needed. see:
    }
    //dir.close(); // this call does not exist. 
    //SPIFFS.end(); // TODO: revisit
}

void Downloader::print_file (const char* file_name) {
    SERIAL_PRINT(F("Opening SPIFF file for reading: "));
    SERIAL_PRINTLN (file_name);    
    if (!SPIFFS.exists(file_name)) {
        SERIAL_PRINT(F("The file is missing ! "));
        return;
    }
    File f = SPIFFS.open(file_name, "r");
    if (!f.isFile()) {
      SERIAL_PRINTLN(F("Failed to open file for reading."));
      return;
    }
    SERIAL_PRINTLN(F("File opened. Contents:\n"));
    while(f.available()) {
      String line = f.readStringUntil('\n');
      SERIAL_PRINTLN(line);
    }    
    f.close();
}
 
