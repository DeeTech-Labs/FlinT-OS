#pragma once

#include <stdint.h>

#include "faces/FaceContext.h"

enum GlanceKind : uint8_t {
  GLANCE_WIFI = 0,
  GLANCE_DATE,
};

struct GlanceRect {
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

// Draw a complication into the current display target. Returns true if painted.
// Optional `label` skips re-formatting for DATE (face already built it).
bool flintGlanceDraw(const FaceContext& ctx, GlanceKind kind, GlanceRect r,
                     const char* label = nullptr);
