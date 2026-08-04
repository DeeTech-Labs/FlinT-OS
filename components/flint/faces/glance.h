#pragma once

#include <stdint.h>

#include "faces/FaceContext.h"

enum GlanceKind : uint8_t {
  GLANCE_WEATHER = 0,
  GLANCE_WIFI,
  GLANCE_DATE,
  GLANCE_DAILY_RANGE,
};

struct GlanceRect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

// Draw a complication into the current display target. Returns true if painted.
bool flintGlanceDraw(const FaceContext& ctx, GlanceKind kind, GlanceRect r);
