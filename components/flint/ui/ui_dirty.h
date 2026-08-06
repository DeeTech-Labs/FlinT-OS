#pragma once

#include <stdint.h>

// Dirty flags for face redraw. Services call uiMarkDirty(); shell takes them
// with uiTakeDirty() and only paints when needed (event-driven ticks).

enum UiDirty : uint32_t {
  UI_DIRTY_NONE       = 0,
  UI_DIRTY_FORCE      = 1u << 0,
  UI_DIRTY_SECOND     = 1u << 1,
  UI_DIRTY_MINUTE     = 1u << 2,
  UI_DIRTY_WIFI       = 1u << 3,
  UI_DIRTY_TIME_VALID = 1u << 4,
  UI_DIRTY_DAY_MODE   = 1u << 5,
  UI_DIRTY_SETTINGS   = 1u << 6,
  UI_DIRTY_MOTION     = 1u << 7,  // face micro-animations (colon pulse, …)
};

void uiMarkDirty(uint32_t mask);
uint32_t uiPeekDirty();
uint32_t uiTakeDirty();      // clear-and-return
bool uiHasDirty();
