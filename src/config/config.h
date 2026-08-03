#pragma once

#include <stdint.h>
#include "secrets.h"

// FlinT OS — shared configuration (safe to commit)
// Pins: ESP32-C3 + GMT028-05 (ST7789)
// Secrets (Wi‑Fi): src/config/secrets.h  ← local only

// Timezone (Орёл / Москва, UTC+3, без DST)
static const long GMT_OFFSET_SEC = 3 * 3600;
static const int DAYLIGHT_OFFSET_SEC = 0;
static const char* NTP_SERVER = "pool.ntp.org";

// Open-Meteo — Орёл, Россия
static const float WEATHER_LAT = 52.9651f;
static const float WEATHER_LON = 36.0694f;
static const char* WEATHER_TZ = "Europe%2FMoscow";

// Display pins
static const int TFT_CS = 10;
static const int TFT_DC = 9;
static const int TFT_RST = 8;
static const int TFT_MOSI = 7;
static const int TFT_SCLK = 6;

// Landscape 320x240. If upside-down, use 3.
static const uint8_t TFT_ROTATION = 1;

// Serial debug: 0=off, 1=info, 2=verbose
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL 1
#endif
static const uint32_t DEBUG_HEARTBEAT_MS = 10000UL;

// Service intervals
static const uint32_t NTP_RESYNC_MS = 24UL * 60UL * 60UL * 1000UL;
static const uint32_t WEATHER_REFRESH_MS = 15UL * 60UL * 1000UL;
static const uint32_t WIFI_RETRY_MS = 15000UL;
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000UL;

// Face / theme API (bump when Face ABI breaks)
static const uint16_t FLINT_OS_API_VERSION = 1;
static const char* ACTIVE_FACE_ID = "digital";

// Режимы суток (час локального времени, [start .. next) )
// ночь: [NIGHT_START .. 24) U [0 .. MORNING_START)
static const int DAY_MODE_MORNING_START = 5;   // 05:00–10:59 утро
static const int DAY_MODE_DAY_START = 11;      // 11:00–16:59 день
static const int DAY_MODE_EVENING_START = 17;  // 17:00–21:59 вечер
static const int DAY_MODE_NIGHT_START = 22;    // 22:00–04:59 ночь
