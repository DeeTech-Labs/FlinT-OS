#pragma once

#include <stdint.h>

// Pure hour → day-mode mapping (no display / ESP deps — host-testable).

enum DayMode : uint8_t {
  DAY_MODE_MORNING = 0,
  DAY_MODE_DAY,
  DAY_MODE_EVENING,
  DAY_MODE_NIGHT,
  DAY_MODE_WAITING  // no valid local time — not derived from a synthetic hour
};

// Local-time hour bounds: [start .. next). Night wraps midnight.
// night: [NIGHT_START .. 24) U [0 .. MORNING_START)
inline constexpr int kDayModeMorningStart = 5;   // 05:00–10:59
inline constexpr int kDayModeDayStart = 11;      // 11:00–16:59
inline constexpr int kDayModeEveningStart = 17;  // 17:00–21:59
inline constexpr int kDayModeNightStart = 22;    // 22:00–04:59

DayMode dayModeFromHour(int hour);
