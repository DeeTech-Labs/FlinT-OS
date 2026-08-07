#pragma once

#include <stdint.h>

#include "ui/ui_dirty.h"

// Pure dirty-mask rules used by uiFaceTick (host-testable).

inline bool uiDirtyIsMotionOnly(uint32_t dirty) {
  return dirty != 0 && (dirty & ~UI_DIRTY_MOTION) == 0;
}

inline uint32_t uiDirtyFilterSeconds(uint32_t dirty, bool showSeconds) {
  if (!showSeconds) {
    dirty &= ~UI_DIRTY_SECOND;
  }
  return dirty;
}

inline bool uiDirtyWantsForceRedraw(uint32_t dirty) {
  return (dirty & (UI_DIRTY_FORCE | UI_DIRTY_DAY_MODE | UI_DIRTY_TIME_VALID)) != 0;
}
