#pragma once

#include <stdint.h>

enum AnimKind : uint8_t {
  ANIM_NONE = 0,
  ANIM_OVERLAY_IN,
};

// Shared easing / progress (0..255). Safe on a single framebuffer.
uint8_t animEaseOutCubic(uint8_t t);
uint8_t animLinearProgress(uint32_t elapsedMs, uint32_t durationMs);

void animBegin(AnimKind kind);
bool animIsActive();
// Current eased progress 0..255 (255 when inactive / finished).
uint8_t animEasedProgress();
// Advance clock. Returns true while animation still owns the frame.
bool animTick();
