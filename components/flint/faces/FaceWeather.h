#pragma once

#include <stdint.h>

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
