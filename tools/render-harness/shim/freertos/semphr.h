#pragma once

// std::mutex-backed FreeRTOS mutex subset (Epub.cpp's static GIF mutex).
#include <mutex>

#include "FreeRTOS.h"

struct StaticSemaphore_t {
  std::mutex mutex;
};
using SemaphoreHandle_t = StaticSemaphore_t*;

inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* storage) { return storage; }
inline BaseType_t xSemaphoreTake(SemaphoreHandle_t handle, TickType_t) {
  handle->mutex.lock();
  return pdTRUE;
}
inline BaseType_t xSemaphoreGive(SemaphoreHandle_t handle) {
  handle->mutex.unlock();
  return pdTRUE;
}
