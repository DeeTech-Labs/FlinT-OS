#pragma once

#include <time.h>
#include "day_mode/day_mode.h"
#include "display/ui_palette.h"

// Read-only snapshot for faces. Faces must not mutate services or access hardware.

struct FaceContext {
  bool hasTime;
  struct tm localTime;

  DayMode dayMode;
  DayModeProfile dayProfile;
  UiPalette palette;

  bool usbConnected;

  // Resolved prefs (shell/settings → /cfg/ui.cfg) — faces must not read cfg/NVS.
  bool showSeconds;
  bool hour12;  // false = 24h, true = 12h + AM/PM
  uint8_t dateFormat;  // SettingsDateFormat
  uint8_t dateSep;     // SettingsDateSep

  int screenW;
  int screenH;

  // Dirty bitfield for this paint (from uiTakeDirty). 0 = unspecified.
  uint32_t dirty;
};
