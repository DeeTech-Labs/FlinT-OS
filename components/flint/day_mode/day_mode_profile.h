#pragma once

#include "day_mode/day_mode_hour.h"

#include <stdint.h>

// Profile flags for day modes (no display deps — host-testable).

struct DayModeProfile {
  bool showSeconds;
  bool showModeLabel;
  uint8_t washHeight;
};

const DayModeProfile& dayModeProfile(DayMode mode);
