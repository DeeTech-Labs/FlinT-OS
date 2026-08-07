#include "day_mode/day_mode_hour.h"
#include "day_mode/day_mode_profile.h"
#include "host_check.h"

#include <cstdio>

static void expectMode(int hour, DayMode want) {
  const DayMode got = dayModeFromHour(hour);
  if (got != want) {
    std::fprintf(stderr, "dayModeFromHour(%d): got %u want %u\n", hour,
                 static_cast<unsigned>(got), static_cast<unsigned>(want));
    hostFail("dayModeFromHour mismatch");
  }
}

int main() {
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
    hostRequire(dayModeFromHour(h) != DAY_MODE_WAITING, "hour must not map to WAITING");
  }

  const DayModeProfile& waiting = dayModeProfile(DAY_MODE_WAITING);
  hostRequire(!waiting.showSeconds, "waiting auto-seconds off");
  hostRequire(!waiting.showModeLabel, "waiting mode label off");
  hostRequire(waiting.washHeight > 0, "waiting wash height");

  hostRequire(dayModeProfile(DAY_MODE_DAY).showSeconds, "day auto-seconds on");
  hostRequire(dayModeProfile(DAY_MODE_DAY).showModeLabel, "day mode label on");

  std::puts("ok test_day_mode");
  return 0;
}
