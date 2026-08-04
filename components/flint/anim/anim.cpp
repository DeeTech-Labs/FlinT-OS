#include "anim/anim.h"

#include "display/ui_tokens.h"
#include "util/flint_sys.h"

struct AnimState {
  AnimKind kind = ANIM_NONE;
  uint32_t t0 = 0;
  uint32_t durationMs = 0;
  bool active = false;
};

static AnimState gAnim;

uint8_t animEaseOutCubic(uint8_t t) {
  // easeOutCubic on 0..255 fixed-point
  const uint32_t u = 255u - t;
  const uint32_t u3 = (u * u * u) / (255u * 255u);
  return static_cast<uint8_t>(255u - u3);
}

uint8_t animLinearProgress(uint32_t elapsedMs, uint32_t durationMs) {
  if (durationMs == 0 || elapsedMs >= durationMs) {
    return 255;
  }
  return static_cast<uint8_t>((elapsedMs * 255UL) / durationMs);
}

void animBegin(AnimKind kind) {
  if (kind == ANIM_NONE) {
    gAnim.active = false;
    gAnim.kind = ANIM_NONE;
    return;
  }
  gAnim.kind = kind;
  gAnim.t0 = flintMillis();
  gAnim.durationMs = (kind == ANIM_OVERLAY_IN) ? UiTok::kAnimOverlayInMs : 0;
  gAnim.active = gAnim.durationMs > 0;
}

bool animIsActive() {
  return gAnim.active;
}

uint8_t animEasedProgress() {
  if (!gAnim.active) {
    return 255;
  }
  const uint32_t elapsed = flintMillis() - gAnim.t0;
  const uint8_t linear = animLinearProgress(elapsed, gAnim.durationMs);
  return animEaseOutCubic(linear);
}

bool animTick() {
  if (!gAnim.active) {
    return false;
  }
  if ((flintMillis() - gAnim.t0) >= gAnim.durationMs) {
    gAnim.active = false;
    gAnim.kind = ANIM_NONE;
    return false;
  }
  return true;
}
