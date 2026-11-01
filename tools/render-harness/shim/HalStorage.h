#pragma once

// Host HalStorage over the real filesystem; paths are used as given (the
// harness hands Epub a host --cache-dir). Each HalFile is one O_RDWR-capable
// POSIX descriptor, so reads and writes on one handle share a single position
// like an SdFat FsFile (Section reads built pages back mid-build).

#include <fcntl.h>

#include <cstddef>
#include <cstdint>
#include <string>

#include "Print.h"
#include "WString.h"

using oflag_t = int;

class HalStorage;

class HalFile : public Print {
  friend class HalStorage;

 public:
  HalFile() = default;
  ~HalFile() override;
  HalFile(HalFile&& other) noexcept;
  HalFile& operator=(HalFile&& other) noexcept;
  HalFile(const HalFile&) = delete;
  HalFile& operator=(const HalFile&) = delete;

  void flush() {}
  size_t size();
  size_t fileSize() { return size(); }
  uint64_t fileSize64() { return size(); }
  bool seek(size_t pos);
  bool seek64(uint64_t pos) { return seek(static_cast<size_t>(pos)); }
  bool seekCur(int64_t offset);
  bool seekSet(size_t offset) { return seek(offset); }
  bool truncate(uint64_t length);
  int available() const;
  size_t position() const;
  int read(void* buf, size_t count);
  int read();
  size_t write(const uint8_t* buf, size_t count) override;
  size_t write(const void* buf, size_t count);
  size_t write(uint8_t b) override;
  bool isDirectory() const;
  bool close();
  bool isOpen() const { return fd >= 0; }
  operator bool() const { return isOpen(); }

 private:
  int fd = -1;
};

class HalStorage {
 public:
  static HalStorage& getInstance() { return instance; }

  HalFile open(const char* path, oflag_t oflag = O_RDONLY);
  bool mkdir(const char* path, bool pFlag = true);
  bool exists(const char* path);
  bool remove(const char* path);
  bool rename(const char* oldPath, const char* newPath);
  bool rmdir(const char* path);
  bool removeDir(const char* path);
  bool ensureDirectoryExists(const char* path) { return mkdir(path, true); }

  bool openFileForRead(const char* moduleName, const char* path, HalFile& file);
  bool openFileForRead(const char* moduleName, const std::string& path, HalFile& file) {
    return openFileForRead(moduleName, path.c_str(), file);
  }
  bool openFileForRead(const char* moduleName, const String& path, HalFile& file) {
    return openFileForRead(moduleName, path.c_str(), file);
  }
  bool openFileForWrite(const char* moduleName, const char* path, HalFile& file);
  bool openFileForWrite(const char* moduleName, const std::string& path, HalFile& file) {
    return openFileForWrite(moduleName, path.c_str(), file);
  }
  bool openFileForWrite(const char* moduleName, const String& path, HalFile& file) {
    return openFileForWrite(moduleName, path.c_str(), file);
  }

 private:
  static HalStorage instance;
};

#define Storage HalStorage::getInstance()
