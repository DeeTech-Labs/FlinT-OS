#pragma once

#include <stdint.h>

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
  uint16_t wifiOn;
  uint16_t wifiOff;
  uint16_t divider;
  uint16_t selectionBg;
  uint16_t selectionFg;
  uint16_t footerMuted;
};
