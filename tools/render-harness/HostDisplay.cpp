#include <HalDisplay.h>

#include <algorithm>
#include <cstring>

HalDisplay display;

HalDisplay::HalDisplay() : frame(BUFFER_SIZE, 0xFF), lsb(BUFFER_SIZE, 0x00), msb(BUFFER_SIZE, 0x00) {}

void HalDisplay::begin(bool) { std::memset(frame.data(), 0xFF, frame.size()); }

HalDisplay::GrayscaleCapabilities HalDisplay::grayscaleCapabilities(const GrayscaleMode mode) const {
  // X4 SSD1677 overlay mode, but without strip uploads so the reader takes its
  // whole-plane path (storeBwBuffer + copyGrayscale*Buffers) we can capture.
  if (mode != GrayscaleMode::Overlay) return {};
  return {GrayscaleEncoding::OverlayMasks, GrayscaleBase::Separate, false, true, false};
}

void HalDisplay::clearScreen(const uint8_t color) const {
  if (!lent) std::memset(frame.data(), color, frame.size());
}

void HalDisplay::drawImage(const uint8_t* imageData, const uint16_t x, const uint16_t y, const uint16_t w,
                           const uint16_t h, bool) const {
  if (lent || !imageData) return;
  // Same packing as the panel driver: w is a multiple of 8, rows MSB first.
  const uint16_t rowBytes = w / 8;
  for (uint16_t row = 0; row < h && y + row < DISPLAY_HEIGHT; row++) {
    const uint32_t dst = static_cast<uint32_t>(y + row) * DISPLAY_WIDTH_BYTES + x / 8;
    const uint16_t copyBytes = std::min<uint16_t>(rowBytes, DISPLAY_WIDTH_BYTES - x / 8);
    std::memcpy(frame.data() + dst, imageData + static_cast<uint32_t>(row) * rowBytes, copyBytes);
  }
}

void HalDisplay::displayBuffer(RefreshMode, bool) {}

void HalDisplay::displayBufferAsync(RefreshMode) {}

uint8_t* HalDisplay::getFrameBuffer() const { return lent ? nullptr : frame.data(); }

uint8_t* HalDisplay::lendFrameBufferStorage(uint32_t* sizeOut) {
  if (lent) {
    if (sizeOut) *sizeOut = 0;
    return nullptr;
  }
  lent = true;
  if (sizeOut) *sizeOut = BUFFER_SIZE;
  return frame.data();
}

void HalDisplay::returnFrameBufferStorage() {
  if (!lent) return;
  lent = false;
  std::memset(frame.data(), 0xFF, frame.size());
}

void HalDisplay::displayGrayscaleBase(RefreshMode, bool) {}

bool HalDisplay::displayGrayscaleBase(const GrayscaleMode mode, RefreshMode, bool) {
  return mode == GrayscaleMode::Overlay;
}

void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer) {
  if (lsbBuffer) std::memcpy(lsb.data(), lsbBuffer, lsb.size());
}

void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* msbBuffer) {
  if (msbBuffer) std::memcpy(msb.data(), msbBuffer, msb.size());
}

void HalDisplay::writeGrayscalePlaneStrip(const bool lsbPlane, const uint8_t* rows, const uint16_t yStart,
                                          const uint16_t numRows) {
  auto& plane = lsbPlane ? lsb : msb;
  std::memcpy(plane.data() + static_cast<size_t>(yStart) * DISPLAY_WIDTH_BYTES, rows,
              static_cast<size_t>(numRows) * DISPLAY_WIDTH_BYTES);
}

void HalDisplay::clearGrayPlanes() {
  std::memset(lsb.data(), 0x00, lsb.size());
  std::memset(msb.data(), 0x00, msb.size());
}
