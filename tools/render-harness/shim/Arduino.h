#pragma once

// Host stand-in for the Arduino core: only the surface the reader layout and
// render path call. Time comes from steady_clock so host timings are real.

#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "WString.h"

inline uint32_t harnessElapsedUs() {
  static const auto start = std::chrono::steady_clock::now();
  return static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
}

inline unsigned long millis() { return harnessElapsedUs() / 1000; }
inline unsigned long micros() { return harnessElapsedUs(); }
inline void delay(uint32_t) {}
inline void yield() {}
inline void vTaskDelay(uint32_t) {}

#ifndef PROGMEM
#define PROGMEM
#endif

struct HarnessEsp {
  // Large values keep every heap-pressure fallback (CSS skip, cache shedding) off.
  uint32_t getFreeHeap() const { return 64u * 1024u * 1024u; }
  uint32_t getMaxAllocHeap() const { return 64u * 1024u * 1024u; }
  [[noreturn]] void restart() const { std::abort(); }
};

inline HarnessEsp ESP;
