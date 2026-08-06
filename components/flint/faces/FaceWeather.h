#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

// Snapshot-only weather fields for FaceContext / SDK.
// Faces must not call weather APIs — shell fills FaceContext.weather.

struct WeatherDaily {
  int16_t tMaxCx10;
  int16_t tMinCx10;
  int16_t code;
};

struct WeatherState {
  bool ok;
  bool loading;
  int16_t tempCx10;  // current temp * 10 (°C)
  int16_t code;
  uint32_t updatedMs;

  bool dailyOk;
  WeatherDaily daily;
};

// Whole °C from Cx10 (truncate toward zero; matches prior %+ .0f for typical values).
inline int weatherTempCWhole(int16_t tempCx10) {
  return static_cast<int>(tempCx10 / 10);
}

// Current temp label: "+21" / ".." / "--".
inline void weatherFormatTempC(char* out, size_t outN, const WeatherState& w) {
  if (!out || outN == 0) {
    return;
  }
  if (w.ok) {
    snprintf(out, outN, "%+d", weatherTempCWhole(w.tempCx10));
  } else if (w.loading) {
    snprintf(out, outN, "..");
  } else {
    snprintf(out, outN, "--");
  }
}

// Daily min/max label: "+12/+19".
inline void weatherFormatDailyRange(char* out, size_t outN, const WeatherDaily& d) {
  if (!out || outN == 0) {
    return;
  }
  snprintf(out, outN, "%+d/%+d", weatherTempCWhole(d.tMinCx10), weatherTempCWhole(d.tMaxCx10));
}
