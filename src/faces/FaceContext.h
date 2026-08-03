#pragma once

#include <time.h>
#include "src/day_mode/day_mode.h"
#include "src/display/ui_palette.h"
#include "src/weather/weather.h"

// Read-only snapshot for faces. Faces must not mutate services.
struct FaceContext {
  bool hasTime;
  struct tm localTime;

  DayMode dayMode;
  DayModeProfile dayProfile;
  UiPalette palette;

  WeatherState weather;
  bool wifiConnected;

  int screenW;
  int screenH;
};
