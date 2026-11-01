#pragma once

// Firmware LOG_* macros routed to stderr, enabled at runtime by --verbose.
#include <Arduino.h>

#include <cstdio>

#include <cstdarg>

namespace harness {
inline bool verboseLogging = false;

// Not a printf-attributed function on purpose: firmware log formats are
// written for the ESP32, where uint32_t is unsigned long ("%lu"). On x86 the
// same format would warn at every call site, so the check is off here and
// stays on in the firmware build, which is where it means something.
inline void log(const char* level, const char* origin, const char* format, ...) {
  if (!verboseLogging) return;
  std::fprintf(stderr, "[%s] [%s] ", level, origin);
  va_list args;
  va_start(args, format);
  std::vfprintf(stderr, format, args);
  va_end(args);
  std::fputc('\n', stderr);
}
}  // namespace harness

#define LOG_ERR(origin, format, ...) harness::log("ERR", origin, format, ##__VA_ARGS__)
#define LOG_INF(origin, format, ...) harness::log("INF", origin, format, ##__VA_ARGS__)
#define LOG_DBG(origin, format, ...) harness::log("DBG", origin, format, ##__VA_ARGS__)
