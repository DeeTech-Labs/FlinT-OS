#pragma once

#include <stdint.h>
#include "display/ui_palette.h"

enum DayMode : uint8_t {
  DAY_MODE_MORNING = 0,
  DAY_MODE_DAY,
  DAY_MODE_EVENING,
  DAY_MODE_NIGHT
};

struct DayModeProfile {
  DayMode mode;
  const char* nameRu;     // «утро» / «день» / …
  bool showSeconds;       // ночь: только ЧЧ:ММ
  bool showWeather;       // бейдж погоды
  bool showModeLabel;     // подпись режима сверху
  uint8_t washHeight;     // высота нижней «вуали»
};

DayMode dayModeFromHour(int hour);
const DayModeProfile& dayModeProfile(DayMode mode);
const char* dayModeNameRu(DayMode mode);

// Палитра под режим + лёгкая подстройка от погоды
UiPalette dayModeBuildPalette(DayMode mode, int weatherCode, bool weatherOk);
