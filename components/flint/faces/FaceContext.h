#pragma once

#include <time.h>
#include "day_mode/day_mode.h"
#include "display/ui_palette.h"
#include "faces/FaceWeather.h"

// Read-only snapshot for faces. Faces must not mutate services or call net/http.

struct FaceContext {
  bool hasTime;
  struct tm localTime;

  DayMode dayMode;
  DayModeProfile dayProfile;
  UiPalette palette;

  // Published weather snap for this tick (never null after uiBuildContext).
  // Do not retain across ticks — pointer targets weather service storage.
  const WeatherState* weather;
  bool wifiConnected;

  // Resolved prefs (shell/settings) — faces must not read NVS themselves.
  bool showSeconds;
  bool hour12;  // false = 24h, true = 12h + AM/PM
  uint8_t dateFormat;  // SettingsDateFormat
  uint8_t dateSep;     // SettingsDateSep

  int screenW;
  int screenH;
};
