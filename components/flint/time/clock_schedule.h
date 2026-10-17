#pragma once

#include <stdint.h>

// Sleep until the next displayed second. Round up to avoid waking just before
// the boundary and polling again. elapsedUs stays 64-bit across long uptimes.
inline uint32_t clockNextSecondDelayMs(int64_t elapsedUs) {
  if (elapsedUs < 0) return 1000;
  const int64_t remaining = 1000000LL - elapsedUs % 1000000LL;
  return static_cast<uint32_t>((remaining + 999) / 1000);
}
