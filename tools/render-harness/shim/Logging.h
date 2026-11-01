#pragma once

// Firmware LOG_* macros routed to stderr, enabled at runtime by --verbose.
#include <Arduino.h>

#include <cstdio>

namespace harness {
inline bool verboseLogging = false;
}  // namespace harness

#define HARNESS_LOG(level, origin, format, ...)                                     \
  do {                                                                              \
    if (harness::verboseLogging) {                                                  \
      std::fprintf(stderr, "[%s] [%s] " format "\n", level, origin, ##__VA_ARGS__); \
    }                                                                               \
  } while (0)

#define LOG_ERR(origin, format, ...) HARNESS_LOG("ERR", origin, format, ##__VA_ARGS__)
#define LOG_INF(origin, format, ...) HARNESS_LOG("INF", origin, format, ##__VA_ARGS__)
#define LOG_DBG(origin, format, ...) HARNESS_LOG("DBG", origin, format, ##__VA_ARGS__)
