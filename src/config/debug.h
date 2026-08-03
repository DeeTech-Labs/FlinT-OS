#pragma once

#include <Arduino.h>
#include "config.h"

// Serial monitor: 115200 baud
// Levels: 0=off, 1=info, 2=verbose (HTTP body snippets, UI redraws)

#if DEBUG_LEVEL >= 1
  #define LOGI(tag, fmt, ...) \
    do { Serial.printf("[%8lu][%s] " fmt "\n", millis(), tag, ##__VA_ARGS__); } while (0)
#else
  #define LOGI(tag, fmt, ...) do {} while (0)
#endif

#if DEBUG_LEVEL >= 2
  #define LOGD(tag, fmt, ...) \
    do { Serial.printf("[%8lu][%s] " fmt "\n", millis(), tag, ##__VA_ARGS__); } while (0)
#else
  #define LOGD(tag, fmt, ...) do {} while (0)
#endif

#define LOGE(tag, fmt, ...) \
  do { Serial.printf("[%8lu][%s][ERR] " fmt "\n", millis(), tag, ##__VA_ARGS__); } while (0)
