// settings.h
#ifndef SETTINGS_H
#define SETTINGS_H

#define  FIRMWARE_VERSION       2            // increment the firmware version for every revision
#define  CERTIFICATE_VERSION    2            // increment when you want to change the certificates of config.txt file on the Flash

#define  BAUD_RATE              115200       // for serial port
#define  APP_ID                 "bootloader"    // this helps update the bootloader itself to a new version, if available
#define  CERT_VERSION_FILE      "certversion.txt"   // found on the cloud server; controls downlod of certificates
#define  CONFIG_FILE_NAME       "/config.txt"   // found on SPIFF; overrides settings.h and keys.h (The leading slash is essential !)
#define  CONFIG_FILE_SIZE       412        // bytes; the raw text file on the flash
#define  JSON_CONFIG_FILE_SIZE  612        // bytes; including json overhead: https://arduinojson.org/v6/assistant/
#define  NUM_CERTIFICATE_FILES  4          // 3 TLS certificates and one config.txt file

#endif
