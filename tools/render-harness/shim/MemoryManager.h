#pragma once

// Host MemoryManager: no memory pressure. Sinks are recorded but never run,
// ensureFree() always succeeds, and the free-byte queries report a large heap.

#include <cstddef>
#include <cstdint>
#include <functional>

namespace freeink {

enum class MemPool : uint8_t { Internal, Psram, Default };

struct CacheSink {
  const char* name = nullptr;
  uint8_t priority = 128;
  std::function<size_t(size_t bytesRequested)> evict;
};

class MemoryManager {
 public:
  static MemoryManager& instance() {
    static MemoryManager inst;
    return inst;
  }
  int registerSink(const CacheSink&) { return ++nextId_; }
  void unregisterSink(int) {}
  void unregisterSink(const char*) {}
  size_t freeBytes(MemPool = MemPool::Default) const { return 64u * 1024u * 1024u; }
  size_t largestFreeBlock(MemPool = MemPool::Default) const { return 64u * 1024u * 1024u; }
  size_t clearCaches(size_t = 0) { return 0; }
  bool ensureFree(size_t, MemPool = MemPool::Default) { return true; }

 private:
  MemoryManager() = default;
  int nextId_ = 0;
};

}  // namespace freeink
