#pragma once

#include <stdint.h>

// Snapshot-only weather fields for FaceContext / SDK.
// Faces must not call weatherGet() — shell fills FaceContext.

struct WeatherState {
  bool ok;
  bool loading;
  float tempC;
  int code;
  uint32_t updatedMs;
};
