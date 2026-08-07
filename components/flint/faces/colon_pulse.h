#pragma once

#include "display/display.h"
#include "display/ui_palette.h"
#include "display/ui_tokens.h"
#include "util/flint_sys.h"

#include <stdint.h>

// Shared colon-pulse fade helpers (shell MOTION gate + digital face paint).

inline uint8_t digitalColonFadeNow() {
  const uint32_t phase = flintMillis() % UiTok::kColonPulsePeriodMs;
  const uint32_t half = UiTok::kColonPulsePeriodMs / 2;
  uint8_t t;
  if (phase < half) {
    t = static_cast<uint8_t>((phase * 255UL) / half);
  } else {
    t = static_cast<uint8_t>(((UiTok::kColonPulsePeriodMs - phase) * 255UL) / half);
  }
  return static_cast<uint8_t>(140 + (t * 115UL) / 255);
}

inline uint16_t digitalColonColor(const UiPalette& pal, uint8_t fade) {
  return colorLerp(pal.textSecondary, pal.accent, fade);
}

inline bool digitalColonFadeChanged(uint8_t fade, uint8_t lastFade) {
  const int d = static_cast<int>(fade) - static_cast<int>(lastFade);
  return d >= UiTok::kColonFadeQuant || d <= -UiTok::kColonFadeQuant;
}
