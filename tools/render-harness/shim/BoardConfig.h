#pragma once

// Host BoardConfig: only the X4 fields the render path reads. Values mirror
// freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h (XTEINK_X4 uses
// the default ViewableInsets {9, 3, 3, 3}).

#include <cstdint>

namespace BoardConfig {

enum class DisplayController : uint8_t { SSD1677 };

struct ViewableInsets {
  uint8_t top = 9;
  uint8_t right = 3;
  uint8_t bottom = 3;
  uint8_t left = 3;
};

struct BoardProfile {
  ViewableInsets viewableInsets = {};
};

inline BoardProfile ACTIVE = {};

constexpr bool isEegoA4() { return false; }

}  // namespace BoardConfig
