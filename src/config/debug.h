#pragma once

#include <Arduino.h>
#include "config.h"

// Serial monitor: 115200 baud
// Levels: 0=off, 1=info, 2=verbose (HTTP body snippets, UI redraws)
// USB-CDC can block for seconds if the host isn't reading — skip when buffer is full.

static inline bool flintLogCanWrite() {
  // availableForWrite() is reliable on HardwareSerial / USB-CDC in ESP32 core
  return Serial.availableForWrite() > 64;
}

#if DEBUG_LEVEL >= 1
  #define LOGI(tag, fmt, ...) \
    do { \
      if (flintLogCanWrite()) { \
        Serial.printf("[%8lu][%s] " fmt "\n", millis(), tag, ##__VA_ARGS__); \
      } \
    } while (0)
#else
  #define LOGI(tag, fmt, ...) do {} while (0)
#endif

#if DEBUG_LEVEL >= 2
  #define LOGD(tag, fmt, ...) \
    do { \
      if (flintLogCanWrite()) { \
        Serial.printf("[%8lu][%s] " fmt "\n", millis(), tag, ##__VA_ARGS__); \
      } \
    } while (0)
#else
  #define LOGD(tag, fmt, ...) do {} while (0)
#endif

#define LOGE(tag, fmt, ...) \
  do { \
    if (flintLogCanWrite()) { \
      Serial.printf("[%8lu][%s][ERR] " fmt "\n", millis(), tag, ##__VA_ARGS__); \
    } \
  } while (0)
