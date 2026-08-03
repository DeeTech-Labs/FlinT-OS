#pragma once

#include <stdint.h>

struct WeatherState {
  bool ok;
  bool loading;
  float tempC;
  int code;
  uint32_t updatedMs;
};

const WeatherState weatherGet();  // snapshot copy (thread-safe)
const char* weatherCodeRu(int code);
void weatherStartService();       // FreeRTOS background fetch
bool weatherIsOk();
uint32_t weatherLastFetchMs();
