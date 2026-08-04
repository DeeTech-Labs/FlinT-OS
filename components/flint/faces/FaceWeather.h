#pragma once

#include <stdint.h>

// Snapshot-only weather fields for FaceContext / SDK.
// Faces must not call weatherGet() — shell fills FaceContext.

struct WeatherHourly {
  int8_t hour;       // 0–23 local
  int16_t tempCx10;  // temp * 10
  int16_t code;
};

struct WeatherDaily {
  int16_t tMaxCx10;
  int16_t tMinCx10;
  int16_t code;
};

struct WeatherState {
  bool ok;
  bool loading;
  float tempC;
  int code;
  uint32_t updatedMs;

  bool dailyOk;
  WeatherDaily daily;
  uint8_t hourlyCount;
  WeatherHourly hourly[8];
};
