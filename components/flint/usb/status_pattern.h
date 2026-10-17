#pragma once

#include <stdint.h>

// Pure LED timing, independent of GPIO polarity and scheduler frequency.
class PcStatusPattern {
public:
  void synced(uint32_t now) { syncStart_ = now; syncing_ = true; }

  bool on(bool connected, uint32_t now) {
    if (!connected) {
      syncing_ = false;
      return false;
    }
    if (syncing_) {
      const uint32_t elapsed = now - syncStart_;
      // Off lead-in, then exactly three 80 ms flashes, then a quiet gap.
      if (elapsed < 800) return elapsed < 480 && ((elapsed / 80) % 2 == 1);
      syncing_ = false;
      blinkStart_ = now;
    }
    return ((now - blinkStart_) / 500) % 2 == 0;
  }

private:
  uint32_t syncStart_ = 0;
  uint32_t blinkStart_ = 0;
  bool syncing_ = false;
};
