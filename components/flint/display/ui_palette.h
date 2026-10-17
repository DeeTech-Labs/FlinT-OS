#pragma once

#include <stdint.h>

#include "display/display.h"

// Semantic UI colors. Day-mode profiles fill these in day_mode.*.

struct UiPalette {
  uint16_t bgTop;
  uint16_t bgBottom;
  uint16_t accent;
  uint16_t glow;
  uint16_t textPrimary;
  uint16_t textSecondary;
  uint16_t panel;
  uint16_t panelBorder;
  uint16_t divider;
  uint16_t selectionBg;
  uint16_t selectionFg;
  uint16_t footerMuted;
};

// Named day-mode base colors (shared with splash — not a full palette).
namespace UiColor {
inline uint16_t dayBgTop() {
  return rgb565(8, 12, 20);
}
inline uint16_t dayAccent() {
  return rgb565(72, 224, 214);
}
inline uint16_t dayTextSecondary() {
  return rgb565(132, 158, 178);
}
}  // namespace UiColor
