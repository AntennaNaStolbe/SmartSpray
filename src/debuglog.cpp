#include "debuglog.h"
#include <stdarg.h>

// ---- Ring buffer of strings ----
// Size is a tradeoff: enough for the log window, doesn't eat ESP8266 heap.
// Kept small: it sits in .bss, and every byte here lowers the heap ceiling -
// which is critical for the GitHub HTTPS firmware download (needs ~16.7KB TLS
// recv buffer + 6.2KB BearSSL stack on a ~30KB free heap).
#define LOG_MAX_LINES 20     // how many recent lines to keep
#define LOG_MAX_LEN   80     // max length of a single line

static char logLines[LOG_MAX_LINES][LOG_MAX_LEN];
static int logWriteIdx = 0;   // where to write the next line
static int logLineCount = 0;  // how many lines accumulated (<= LOG_MAX_LINES)

// Current line being assembled (may be filled in parts via logPrint)
static char curLine[LOG_MAX_LEN];
static int curLen = 0;

static void pushLine() {
  strncpy(logLines[logWriteIdx], curLine, LOG_MAX_LEN - 1);
  logLines[logWriteIdx][LOG_MAX_LEN - 1] = '\0';
  if (logLineCount < LOG_MAX_LINES) logLineCount++;
  logWriteIdx = (logWriteIdx + 1) % LOG_MAX_LINES;
}

static void putChar(char c) {
  if (c == '\r') return;
  if (c == '\n') {
    pushLine();
    curLen = 0;
    curLine[0] = '\0';
  } else if (curLen < LOG_MAX_LEN - 1) {
    curLine[curLen++] = c;
    curLine[curLen] = '\0';
  } else {
    // line overflow — push current and start over
    pushLine();
    curLen = 0;
    putChar(c);
  }
}

// ==================== Public API ====================

void logPrint(const char* s) {
  Serial.print(s);
  for (; *s; ++s) putChar(*s);
}

void logPrintln(const char* s) {
  logPrint(s);
  logPrint("\n");
}

void logPrintf(const char* fmt, ...) {
  char buf[LOG_MAX_LEN];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  logPrint(buf);
}

void logClear() {
  logWriteIdx = 0;
  logLineCount = 0;
  curLen = 0;
  curLine[0] = '\0';
}

String logGet() {
  String out;
  int start = (logLineCount < LOG_MAX_LINES) ? 0 : logWriteIdx;
  for (int i = 0; i < logLineCount; i++) {
    int idx = (start + i) % LOG_MAX_LINES;
    out += logLines[idx];
    out += '\n';
  }
  return out;
}
