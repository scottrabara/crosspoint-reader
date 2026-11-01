#pragma once

// Host HalDisplay: owns an 800x480 1-bit framebuffer (row-major, MSB first,
// bit clear = ink) and records the grayscale planes the renderer uploads.
// Refresh calls do nothing. Capabilities match the X4's SSD1677 overlay mode.

#include <cstdint>
#include <vector>

#include "../../../freeink-sdk/libs/display/FreeInkDisplay/include/GrayscaleCapabilities.h"

class HalDisplay {
 public:
  enum class Controller : uint8_t { SSD1677 };
  Controller getController() const { return Controller::SSD1677; }

  using GrayscaleMode = freeink::GrayscaleMode;
  using GrayscaleCapabilities = freeink::GrayscaleCapabilities;
  using GrayscaleBase = freeink::GrayscaleBase;
  using GrayscaleEncoding = freeink::GrayscaleEncoding;

  GrayscaleCapabilities grayscaleCapabilities(GrayscaleMode mode = GrayscaleMode::Overlay) const;

  enum RefreshMode { FULL_REFRESH, HALF_REFRESH, FAST_REFRESH };

  static constexpr uint16_t DISPLAY_WIDTH = 800;
  static constexpr uint16_t DISPLAY_HEIGHT = 480;
  static constexpr uint16_t DISPLAY_WIDTH_BYTES = DISPLAY_WIDTH / 8;
  static constexpr uint32_t BUFFER_SIZE = DISPLAY_WIDTH_BYTES * DISPLAY_HEIGHT;

  HalDisplay();

  void begin(bool seamless = false);
  void clearScreen(uint8_t color = 0xFF) const;
  void drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 bool fromProgmem = false) const;

  void displayBuffer(RefreshMode mode = RefreshMode::FAST_REFRESH, bool turnOffScreen = false);
  void displayBufferAsync(RefreshMode mode = RefreshMode::FAST_REFRESH);
  void waitRefreshComplete() {}
  bool supportsAsyncRefresh() const { return true; }
  bool supportsAsyncGrayscaleBase() const { return grayscaleCapabilities().asyncBase; }

  bool isInverted() const { return false; }

  uint8_t* getFrameBuffer() const;
  uint8_t* lendFrameBufferStorage(uint32_t* sizeOut);
  void returnFrameBufferStorage();

  void preconditionGrayscale() {}
  void preconditionGrayscale(uint16_t, uint16_t, uint16_t, uint16_t) {}
  void displayGrayscaleBase(RefreshMode fallback = HALF_REFRESH, bool turnOffScreen = false);
  bool displayGrayscaleBase(GrayscaleMode mode, RefreshMode fallback = HALF_REFRESH, bool turnOffScreen = false);

  void copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer);
  void copyGrayscaleMsbBuffers(const uint8_t* msbBuffer);
  void cleanupGrayscaleBuffers(const uint8_t*) {}
  void displayGrayBuffer(bool = false) {}
  void writeGrayscalePlaneStrip(bool lsbPlane, const uint8_t* rows, uint16_t yStart, uint16_t numRows);

  uint16_t getDisplayWidth() const { return DISPLAY_WIDTH; }
  uint16_t getDisplayHeight() const { return DISPLAY_HEIGHT; }
  uint16_t getDisplayWidthBytes() const { return DISPLAY_WIDTH_BYTES; }
  uint32_t getBufferSize() const { return BUFFER_SIZE; }

  // Harness access to the uploaded planes (1 = pixel takes the gray waveform).
  const std::vector<uint8_t>& lsbPlane() const { return lsb; }
  const std::vector<uint8_t>& msbPlane() const { return msb; }
  void clearGrayPlanes();

 private:
  mutable std::vector<uint8_t> frame;
  std::vector<uint8_t> lsb;
  std::vector<uint8_t> msb;
  bool lent = false;
};

extern HalDisplay display;
