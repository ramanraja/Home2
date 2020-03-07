// utilities.cpp

#include "common.h"

// TODO: develop this into a utility class

// utility method for string copying with bounds check
// It just truncates source strings that are too long **

void safe_strncpy (char *dest, char *src, int length = MAX_LONG_STRING_LENGTH) 
{
    if (strlen(src) > (length-1)) 
        SERIAL_PRINTLN ("***** String length is too long to copy !! TRUNCATING.... ******");
    strncpy (dest, src, length);
    dest[length-1] = '\0';  // if overflows, it becomes unterminated ! 
    #ifdef VERBOSE_MODE
        SERIAL_PRINT ("string copied: ");
        SERIAL_PRINTLN (dest);
    #endif
}
 

 
