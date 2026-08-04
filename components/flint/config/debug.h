#pragma once

#include <stdio.h>
#include "config.h"
#include "util/flint_sys.h"

// Levels: 0=off, 1=info, 2=verbose
// Uses stdout (USB Serial/JTAG on ESP32-C3 when configured).

#if DEBUG_LEVEL >= 1
  #define LOGI(tag, fmt, ...) \
    do { \
      printf("[%8lu][%s] " fmt "\n", \
             (unsigned long)flintMillis(), tag, ##__VA_ARGS__); \
    } while (0)
#else
  #define LOGI(tag, fmt, ...) do {} while (0)
#endif

#if DEBUG_LEVEL >= 2
  #define LOGD(tag, fmt, ...) \
    do { \
      printf("[%8lu][%s] " fmt "\n", \
             (unsigned long)flintMillis(), tag, ##__VA_ARGS__); \
    } while (0)
#else
  #define LOGD(tag, fmt, ...) do {} while (0)
#endif

#define LOGE(tag, fmt, ...) \
  do { \
    printf("[%8lu][%s][ERR] " fmt "\n", \
           (unsigned long)flintMillis(), tag, ##__VA_ARGS__); \
  } while (0)
