// utilities.h
#ifndef UTILITIES_H
#define UTILITIES_H

#include "common.h"

extern void safe_strncpy (char *dest, const char *src, int length=MAX_LONG_STRING_LENGTH); 
extern void safe_strncat (char *dest, const char *src, int length=MAX_LONG_STRING_LENGTH);
extern void print_heap(); 

#endif
