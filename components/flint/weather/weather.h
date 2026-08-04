#pragma once

#include <stdint.h>

#include "faces/FaceWeather.h"

const WeatherState weatherGet();  // snapshot copy (thread-safe)
const char* weatherCodeRu(int code);
void weatherStartService();       // FreeRTOS background fetch
bool weatherIsOk();
uint32_t weatherLastFetchMs();
