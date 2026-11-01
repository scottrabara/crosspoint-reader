#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace harness {
// Write an 8-bit grayscale PNG (one byte per pixel, row-major). Uses zlib.
bool writeGrayPng(const std::string& path, int width, int height, const std::vector<uint8_t>& pixels);
}  // namespace harness
