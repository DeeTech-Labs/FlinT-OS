#include "day_mode/day_mode_hour.h"
#include "day_mode/day_mode_profile.h"

#include <cassert>
#include <cstdio>

static void expectMode(int hour, DayMode want) {
  const DayMode got = dayModeFromHour(hour);
  if (got != want) {
    std::fprintf(stderr, "dayModeFromHour(%d): got %u want %u\n", hour,
                 static_cast<unsigned>(got), static_cast<unsigned>(want));
    assert(false);
  }
}

int main() {
  // Boundaries from kDayMode* in day_mode_hour.h
  expectMode(0, DAY_MODE_NIGHT);
  expectMode(4, DAY_MODE_NIGHT);
  expectMode(5, DAY_MODE_MORNING);
  expectMode(10, DAY_MODE_MORNING);
  expectMode(11, DAY_MODE_DAY);
  expectMode(12, DAY_MODE_DAY);
  expectMode(16, DAY_MODE_DAY);
  expectMode(17, DAY_MODE_EVENING);
  expectMode(21, DAY_MODE_EVENING);
  expectMode(22, DAY_MODE_NIGHT);
  expectMode(23, DAY_MODE_NIGHT);

  // WAITING is never derived from an hour — uiBuildContext sets it only when !hasTime.
  for (int h = 0; h < 24; h++) {
    assert(dayModeFromHour(h) != DAY_MODE_WAITING);
  }

  const DayModeProfile& waiting = dayModeProfile(DAY_MODE_WAITING);
  assert(!waiting.showSeconds);
  assert(!waiting.showModeLabel);
  assert(waiting.washHeight > 0);

  // Day profile still enables auto-seconds (contrast with waiting).
  assert(dayModeProfile(DAY_MODE_DAY).showSeconds);
  assert(dayModeProfile(DAY_MODE_DAY).showModeLabel);

  std::puts("ok test_day_mode");
  return 0;
}
