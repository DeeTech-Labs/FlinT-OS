#pragma once

#include <stdint.h>

enum AnimKind : uint8_t {
  ANIM_NONE = 0,
  ANIM_FADE,
};

// Lightweight fade wash between shell screens (no second framebuffer).
void animBegin(AnimKind kind);
bool animIsActive();
// Returns true while animation still owns the frame (caller should skip normal draw).
bool animTick();
