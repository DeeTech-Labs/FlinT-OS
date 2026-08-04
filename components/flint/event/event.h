#pragma once

#include <stdint.h>

// Sync event bus. Producers emit; UI dirty bridge handles.

enum FlintEvent : uint16_t {
  FLINT_EVT_SECOND = 0,
  FLINT_EVT_MINUTE,
  FLINT_EVT_DAY_MODE,
  FLINT_EVT_TIME_VALID,
  FLINT_EVT_WEATHER,
  FLINT_EVT_WIFI,
  FLINT_EVT_SETTINGS,
  FLINT_EVT_FORCE_REDRAW,
};

typedef void (*FlintEventHandler)(FlintEvent ev, void* user);

void flintEventInit();
bool flintEventSubscribe(FlintEventHandler h, void* user);
void flintEventEmit(FlintEvent ev);
