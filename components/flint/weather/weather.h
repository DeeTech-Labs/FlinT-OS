#pragma once

#include <stdint.h>

#include "faces/FaceWeather.h"

const WeatherState weatherGet();  // snapshot copy (thread-safe)
const char* weatherCodeRu(int code);      // localized (legacy name)
const char* weatherCodeLabel(int code);   // localized
void weatherStartService();       // FreeRTOS background fetch
void weatherRequestRefresh();     // fetch ASAP (coords/tz changed)
bool weatherIsOk();
uint32_t weatherLastFetchMs();
