#pragma once

#include <stdint.h>
#include "display/ui_palette.h"

enum DayMode : uint8_t {
  DAY_MODE_MORNING = 0,
  DAY_MODE_DAY,
  DAY_MODE_EVENING,
  DAY_MODE_NIGHT
};

struct DayModeProfile {
  bool showSeconds;
  bool showModeLabel;
  uint8_t washHeight;
};

DayMode dayModeFromHour(int hour);
const DayModeProfile& dayModeProfile(DayMode mode);
const char* dayModeNameRu(DayMode mode);  // legacy alias → flintTrDayMode
const char* dayModeName(DayMode mode);    // localized

UiPalette dayModeBuildPalette(DayMode mode);

// Bottom gradient wash as a few stepped fillRect bands (force redraw hot path).
void dayModeDrawWash(int screenW, int screenH, uint16_t bgTop, uint16_t bgBottom,
                     uint8_t washHeight);
