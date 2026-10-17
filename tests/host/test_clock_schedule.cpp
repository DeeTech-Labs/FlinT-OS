#include "time/clock_schedule.h"
#include "host_check.h"
#include <cstdio>

int main() {
  hostRequire(clockNextSecondDelayMs(0) == 1000, "whole second sleeps one second");
  hostRequire(clockNextSecondDelayMs(500000) == 500, "half second deadline");
  hostRequire(clockNextSecondDelayMs(999000) == 1, "last millisecond");
  hostRequire(clockNextSecondDelayMs(999999) == 1, "round submillisecond remainder up");
  hostRequire(clockNextSecondDelayMs(1000001) == 1000, "avoid early wake and busy polling");
  hostRequire(clockNextSecondDelayMs(60LL * 86400 * 1000000 + 125000) == 875,
              "deadline stays correct beyond 32-bit millis rollover");
  hostRequire(clockNextSecondDelayMs(-1) == 1000, "safe initial deadline");
  std::puts("ok test_clock_schedule");
}
