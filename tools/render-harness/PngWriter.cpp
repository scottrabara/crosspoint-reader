#include "PngWriter.h"

#include <zlib.h>

#include <cstdio>

namespace {

void putU32(std::vector<uint8_t>& out, const uint32_t v) {
  out.push_back(static_cast<uint8_t>(v >> 24));
  out.push_back(static_cast<uint8_t>(v >> 16));
  out.push_back(static_cast<uint8_t>(v >> 8));
  out.push_back(static_cast<uint8_t>(v));
}

void putChunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
  putU32(out, static_cast<uint32_t>(data.size()));
  const size_t typeStart = out.size();
  out.insert(out.end(), type, type + 4);
  out.insert(out.end(), data.begin(), data.end());
  const uLong crc = crc32(0L, out.data() + typeStart, static_cast<uInt>(out.size() - typeStart));
  putU32(out, static_cast<uint32_t>(crc));
}

}  // namespace

bool harness::writeGrayPng(const std::string& path, const int width, const int height,
                           const std::vector<uint8_t>& pixels) {
  if (width <= 0 || height <= 0 || pixels.size() != static_cast<size_t>(width) * height) return false;

  // Filter type 0 (None) on every row.
  std::vector<uint8_t> raw;
  raw.reserve(static_cast<size_t>(width + 1) * height);
  for (int y = 0; y < height; y++) {
    raw.push_back(0);
    raw.insert(raw.end(), pixels.begin() + static_cast<size_t>(y) * width,
               pixels.begin() + static_cast<size_t>(y + 1) * width);
  }
  uLongf compressedSize = compressBound(static_cast<uLong>(raw.size()));
  std::vector<uint8_t> compressed(compressedSize);
  if (compress2(compressed.data(), &compressedSize, raw.data(), static_cast<uLong>(raw.size()), 9) != Z_OK) {
    return false;
  }
  compressed.resize(compressedSize);

  std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  std::vector<uint8_t> ihdr;
  putU32(ihdr, static_cast<uint32_t>(width));
  putU32(ihdr, static_cast<uint32_t>(height));
  ihdr.push_back(8);  // bit depth
  ihdr.push_back(0);  // color type: grayscale
  ihdr.push_back(0);  // compression
  ihdr.push_back(0);  // filter
  ihdr.push_back(0);  // interlace
  putChunk(png, "IHDR", ihdr);
  putChunk(png, "IDAT", compressed);
  putChunk(png, "IEND", {});

  std::FILE* file = std::fopen(path.c_str(), "wb");
  if (!file) return false;
  const bool ok = std::fwrite(png.data(), 1, png.size(), file) == png.size();
  return std::fclose(file) == 0 && ok;
}
