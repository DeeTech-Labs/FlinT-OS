#pragma once
#include <stdint.h>
struct StudioDefinition {
  uint32_t background = 0x101112;
  uint32_t foreground = 0xd4fa70;
  int timeY = 128;
  int dateY = 180;
  bool showDate = true;
  int iconX = 116, iconY = 16, iconScale = 3;
  bool showIcon = true, animateIcon = false;
  unsigned layerOrder = 0;
  uint8_t alternatePixels[32]{};
  uint8_t pixels[32]{}; // 16 x 16, row-major, most-significant bit first.
};
const StudioDefinition& studioDefinition();
bool studioAnimationFrame();
