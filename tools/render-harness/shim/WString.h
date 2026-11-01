#pragma once

// Minimal Arduino String: just enough for FsHelpers.h's `const String&` overloads.
// Like Arduino's, it has no std::string constructor, so std::string arguments
// resolve to the string_view overloads instead of being ambiguous.
#include <string>

class String {
 public:
  String() = default;
  String(const char* s) : value_(s ? s : "") {}

  const char* c_str() const { return value_.c_str(); }
  size_t length() const { return value_.length(); }

 private:
  std::string value_;
};
