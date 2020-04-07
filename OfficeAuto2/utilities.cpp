// utilities.cpp

#include "common.h"
//////#include "utilities.h"

// TODO: develop this into a utility class
// utility method for string copying with bounds check
// It just truncates source strings that are too long **

// Need for this file: if the destination string is not large enough to hold the contents,
// then the behaviour is undefined in the case of plain vanilla strncpy and strncat.
// Also, if strncpy has to truncate the source, then there is no space for the null
// character, so it is left unterminated !

void safe_strncpy (char *dest, const char *src, int length=MAX_LONG_STRING_LENGTH) 
{
    if (strlen(src) > (length-1)) 
        SERIAL_PRINTLN ("***** String length is too long to copy !! TRUNCATING.... ******");
    strncpy (dest, src, length);
    dest[length-1] = '\0';  // if it overflows, destination string becomes unterminated ! 
//    #ifdef VERBOSE_MODE
//        SERIAL_PRINT ("string copied: ");
//        SERIAL_PRINTLN (dest);
//    #endif
}
 
void safe_strncat (char *dest, const char *src, int length=MAX_LONG_STRING_LENGTH) 
{
    if (strlen(src) > (length-1)) 
        SERIAL_PRINTLN ("***** String length is too long to concatenate !! TRUNCATING.... ******");
    strncat (dest, src, length);
    dest[length-1] = '\0';  // if it overflows, destination string becomes unterminated ! 
//    #ifdef VERBOSE_MODE
//        SERIAL_PRINT ("string copied: ");
//        SERIAL_PRINTLN (dest);
//    #endif
}

void print_heap() {
    SERIAL_PRINT("Free Heap: "); 
    SERIAL_PRINTLN(ESP.getFreeHeap()); //Low heap can cause problems  
}
