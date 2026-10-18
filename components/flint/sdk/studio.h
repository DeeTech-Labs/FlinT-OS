#pragma once
#include <stdint.h>
struct StudioDefinition {
  uint32_t background = 0x101112;
  uint32_t foreground = 0xd4fa70;
  int timeY = 128;
  int dateY = 180;
  bool showDate = true;
  uint8_t pixels[32]{}; // 16 x 16, row-major, most-significant bit first.
};
const StudioDefinition& studioDefinition();
