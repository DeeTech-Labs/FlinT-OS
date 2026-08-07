#pragma once

#include "day_mode/day_mode_profile.h"
#include "display/ui_palette.h"

#include <stdint.h>

const char* dayModeNameRu(DayMode mode);  // legacy alias → flintTrDayMode
const char* dayModeName(DayMode mode);    // localized

UiPalette dayModeBuildPalette(DayMode mode);

// Bottom gradient wash as a few stepped fillRect bands (force redraw hot path).
void dayModeDrawWash(int screenW, int screenH, uint16_t bgTop, uint16_t bgBottom,
                     uint8_t washHeight);
