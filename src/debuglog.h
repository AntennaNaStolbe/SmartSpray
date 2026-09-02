#ifndef DEBUGLOG_H
#define DEBUGLOG_H

#include <Arduino.h>

// Ring buffer of recent debug lines for web viewing.
// DEBUG_PRINT* macros duplicate output to both Serial and this buffer.

void logPrint(const char* s);      // append without newline
void logPrintln(const char* s);    // append line (with newline)
void logPrintf(const char* fmt, ...);  // formatted output + newline (like printf)
void logClear();                   // clear buffer
String logGet();                   // return recent lines as a single string

#endif
