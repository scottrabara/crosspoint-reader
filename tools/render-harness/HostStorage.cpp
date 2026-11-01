#include <HalStorage.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

HalStorage HalStorage::instance;

namespace {

bool isDir(const char* path) {
  struct stat st{};
  return ::stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool removeTree(const std::string& path) {
  DIR* dir = ::opendir(path.c_str());
  if (!dir) return false;
  bool ok = true;
  while (const dirent* entry = ::readdir(dir)) {
    if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0) continue;
    const std::string child = path + "/" + entry->d_name;
    ok &= isDir(child.c_str()) ? removeTree(child) : ::unlink(child.c_str()) == 0;
  }
  ::closedir(dir);
  return ::rmdir(path.c_str()) == 0 && ok;
}

}  // namespace

HalFile::~HalFile() { close(); }

HalFile::HalFile(HalFile&& other) noexcept : fd(other.fd) { other.fd = -1; }

HalFile& HalFile::operator=(HalFile&& other) noexcept {
  if (this != &other) {
    close();
    fd = other.fd;
    other.fd = -1;
  }
  return *this;
}

size_t HalFile::size() {
  struct stat st{};
  if (fd < 0 || ::fstat(fd, &st) != 0) return 0;
  return static_cast<size_t>(st.st_size);
}

bool HalFile::seek(const size_t pos) { return fd >= 0 && ::lseek(fd, static_cast<off_t>(pos), SEEK_SET) >= 0; }

bool HalFile::seekCur(const int64_t offset) {
  if (fd < 0) return false;
  const off_t cur = ::lseek(fd, 0, SEEK_CUR);
  if (cur < 0 || cur + offset < 0) return false;
  return ::lseek(fd, static_cast<off_t>(cur + offset), SEEK_SET) >= 0;
}

bool HalFile::truncate(const uint64_t length) { return fd >= 0 && ::ftruncate(fd, static_cast<off_t>(length)) == 0; }

int HalFile::available() const {
  if (fd < 0) return 0;
  struct stat st{};
  if (::fstat(fd, &st) != 0) return 0;
  const off_t cur = ::lseek(fd, 0, SEEK_CUR);
  return cur < 0 || cur >= st.st_size ? 0 : static_cast<int>(st.st_size - cur);
}

size_t HalFile::position() const {
  if (fd < 0) return 0;
  const off_t cur = ::lseek(fd, 0, SEEK_CUR);
  return cur < 0 ? 0 : static_cast<size_t>(cur);
}

int HalFile::read(void* buf, const size_t count) {
  if (fd < 0) return -1;
  size_t done = 0;
  auto* out = static_cast<uint8_t*>(buf);
  while (done < count) {
    const ssize_t n = ::read(fd, out + done, count - done);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) break;
    done += static_cast<size_t>(n);
  }
  return static_cast<int>(done);
}

int HalFile::read() {
  uint8_t b = 0;
  return read(&b, 1) == 1 ? b : -1;
}

size_t HalFile::write(const uint8_t* buf, const size_t count) {
  if (fd < 0) return 0;
  size_t done = 0;
  while (done < count) {
    const ssize_t n = ::write(fd, buf + done, count - done);
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) break;
    done += static_cast<size_t>(n);
  }
  return done;
}

size_t HalFile::write(const void* buf, const size_t count) { return write(static_cast<const uint8_t*>(buf), count); }

size_t HalFile::write(const uint8_t b) { return write(&b, 1); }

bool HalFile::isDirectory() const {
  struct stat st{};
  return fd >= 0 && ::fstat(fd, &st) == 0 && S_ISDIR(st.st_mode);
}

bool HalFile::close() {
  if (fd < 0) return false;
  const bool ok = ::close(fd) == 0;
  fd = -1;
  return ok;
}

HalFile HalStorage::open(const char* path, const oflag_t oflag) {
  HalFile file;
  file.fd = ::open(path, oflag, 0644);
  return file;
}

bool HalStorage::mkdir(const char* path, const bool pFlag) {
  if (isDir(path)) return true;
  if (pFlag) {
    std::string partial;
    const std::string full = path;
    for (size_t i = 1; i <= full.size(); i++) {
      if (i == full.size() || full[i] == '/') {
        partial = full.substr(0, i);
        if (!isDir(partial.c_str()) && ::mkdir(partial.c_str(), 0755) != 0 && errno != EEXIST) return false;
      }
    }
    return true;
  }
  return ::mkdir(path, 0755) == 0;
}

bool HalStorage::exists(const char* path) {
  struct stat st{};
  return ::stat(path, &st) == 0;
}

bool HalStorage::remove(const char* path) { return ::unlink(path) == 0; }

bool HalStorage::rename(const char* oldPath, const char* newPath) { return ::rename(oldPath, newPath) == 0; }

bool HalStorage::rmdir(const char* path) { return ::rmdir(path) == 0; }

bool HalStorage::removeDir(const char* path) { return removeTree(path); }

bool HalStorage::openFileForRead(const char*, const char* path, HalFile& file) {
  file = open(path, O_RDONLY);
  return file.isOpen() && !file.isDirectory();
}

bool HalStorage::openFileForWrite(const char*, const char* path, HalFile& file) {
  // Same flags as SDCardManager::openFileForWrite.
  file = open(path, O_RDWR | O_CREAT | O_TRUNC);
  return file.isOpen();
}
