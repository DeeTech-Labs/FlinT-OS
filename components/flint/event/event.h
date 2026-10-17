#pragma once

#include <stdint.h>

// Sync event bus. Producers emit; UI dirty bridge maps to UiDirty bits.
// Prefs changes: emit FLINT_EVT_SETTINGS only (soft). Do not pair with
// FORCE_REDRAW — faces handle SETTINGS in onTick. See docs/ARCHITECTURE.md.

enum FlintEvent : uint16_t {
  FLINT_EVT_SECOND = 0,
  FLINT_EVT_MINUTE,
  FLINT_EVT_DAY_MODE,
  FLINT_EVT_TIME_VALID,
  FLINT_EVT_USB,
  FLINT_EVT_SETTINGS,       // → UI_DIRTY_SETTINGS (soft)
  FLINT_EVT_FORCE_REDRAW,   // → UI_DIRTY_FORCE (full)
};

typedef void (*FlintEventHandler)(FlintEvent ev, void* user);

void flintEventInit();
bool flintEventSubscribe(FlintEventHandler h, void* user);
void flintEventEmit(FlintEvent ev);
