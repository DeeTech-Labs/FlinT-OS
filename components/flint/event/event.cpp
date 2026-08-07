#include "event/event.h"

#include "config/debug.h"
#include "ui/ui_dirty.h"

static constexpr int kMaxSubs = 8;

struct Sub {
  FlintEventHandler handler;
  void* user;
};

static Sub gSubs[kMaxSubs]{};
static int gSubCount = 0;
static bool gInited = false;

static void uiDirtyBridge(FlintEvent ev, void* /*user*/) {
  switch (ev) {
    case FLINT_EVT_SECOND:
      uiMarkDirty(UI_DIRTY_SECOND);
      break;
    case FLINT_EVT_MINUTE:
      uiMarkDirty(UI_DIRTY_MINUTE);
      break;
    case FLINT_EVT_DAY_MODE:
      uiMarkDirty(UI_DIRTY_DAY_MODE);
      break;
    case FLINT_EVT_TIME_VALID:
      uiMarkDirty(UI_DIRTY_TIME_VALID | UI_DIRTY_FORCE);
      break;
    case FLINT_EVT_WIFI:
      uiMarkDirty(UI_DIRTY_WIFI);
      break;
    case FLINT_EVT_SETTINGS:
      uiMarkDirty(UI_DIRTY_SETTINGS);
      break;
    case FLINT_EVT_FORCE_REDRAW:
      uiMarkDirty(UI_DIRTY_FORCE);
      break;
  }
}

void flintEventInit() {
  if (gInited) {
    return;
  }
  gInited = true;
  gSubCount = 0;
  flintEventSubscribe(uiDirtyBridge, nullptr);
}

bool flintEventSubscribe(FlintEventHandler h, void* user) {
  if (!h) {
    LOGE("event", "subscribe rejected: null handler");
    return false;
  }
  if (gSubCount >= kMaxSubs) {
    LOGE("event", "subscribe rejected: full (%d/%d)", gSubCount, kMaxSubs);
    return false;
  }
  gSubs[gSubCount].handler = h;
  gSubs[gSubCount].user = user;
  gSubCount++;
  return true;
}

void flintEventEmit(FlintEvent ev) {
  // Copy count so a handler cannot grow the table mid-dispatch.
  const int n = gSubCount;
  for (int i = 0; i < n; i++) {
    if (gSubs[i].handler) {
      gSubs[i].handler(ev, gSubs[i].user);
    }
  }
}
