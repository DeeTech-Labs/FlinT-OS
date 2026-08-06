#pragma once

#include <stdint.h>

#include "faces/FaceWeather.h"

// Published snapshot (updated under mutex). Prefer for UI hot path — no struct copy.
const WeatherState& weatherSnapshot();
uint32_t weatherGeneration();  // bumps on each publish

// Full copy under short mutex try; falls back to published snap.
bool weatherCopy(WeatherState* out);

const char* weatherCodeRu(int code);      // localized (legacy name)
const char* weatherCodeLabel(int code);   // localized
void weatherStartService();       // FreeRTOS background fetch
void weatherRequestRefresh();     // fetch ASAP (coords/tz changed)
bool weatherIsOk();
uint32_t weatherLastFetchMs();
