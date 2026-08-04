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

  WeatherState weather;
  bool wifiConnected;

  // Resolved prefs (shell/settings) — faces must not read NVS themselves.
  bool showSeconds;
  uint8_t lang;  // SettingsLang
  float weatherLat;
  float weatherLon;

  int screenW;
  int screenH;
};
