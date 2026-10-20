#include "app.h"
#include "render/watch_display.h"
#include "companion/studio.h"
#include "faces/FaceRegistry.h"
#include <cstring>
#include "companion/apps.h"
#include "ble/ble_link.h"
#include "anim/anim.h"
#include "assets/asset_store.h"
#include "config/config.h"
#include "config/debug.h"
#include "day_mode/day_mode.h"
#include "display/display.h"
#include "event/event.h"
#include "usb/pc_link.h"
#include "settings/cfg_fs.h"
#include "shell/shell.h"
#include "time/clock_time.h"
#include "ui/splash.h"
#include "ui/ui.h"
#include "ui/ui_dirty.h"
#include "util/flint_sys.h"

#include <stdio.h>

void appSetup() {
  printf("\n======== FlinT OS boot ========\n");
  LOGI("app", "DEBUG_LEVEL=%d board=%s", DEBUG_LEVEL, FLINT_BOARD_ID);

  // Init panel ASAP (fills brand bg) — no delay before this, avoids white flash.
  displayBegin();
  LOGI("disp", "screen %dx%d canvas=%d", SCR_W, SCR_H, (int)displayHasCanvas());

  // Mount cfg LittleFS before settings/soft-clock load.
  if (!cfgFsBegin()) {
    LOGE("cfg", "unavailable — prefs/soft clock will use defaults until cfg mounts");
  }

  // Mount LittleFS asset pack before splash/faces.
  if (assetStoreBegin()) {
    LOGI("assets", "ready pack_ver=%u entries=%u", (unsigned)assetStorePackVersion(),
         (unsigned)assetStoreEntryCount());
  } else {
    LOGE("assets", "unavailable — icons/splash mark will be blank until storage is flashed");
  }

  flintEventInit();
  splashShow();

  // Show last-known time immediately; the PC can correct it over USB.
  clockTimeSoftBegin();
  if (clockTimeIsValid()) {
    flintEventEmit(FLINT_EVT_TIME_VALID);
  }

  studioBegin();
  companionAppsBegin();
  shellBegin();
  shellTick();

  pcLinkBegin();
  bleLinkBegin();

  LOGI("app", "setup done (USB time sync); serial: h=help");
}

static void scanTimeEvents(const struct tm* tiOpt) {
  static bool haveLast = false;
  static int lastSec = -1;
  static int lastMin = -1;
  static DayMode lastMode = DAY_MODE_DAY;

  struct tm tiLocal{};
  const struct tm* ti = tiOpt;
  if (!ti) {
    if (!clockTimeGetLocal(&tiLocal)) {
      haveLast = false;
      return;
    }
    ti = &tiLocal;
  }

  const DayMode mode = dayModeFromHour(ti->tm_hour);
  if (!haveLast) {
    lastSec = ti->tm_sec;
    lastMin = ti->tm_min;
    lastMode = mode;
    haveLast = true;
    return;
  }

  if (mode != lastMode) {
    flintEventEmit(FLINT_EVT_DAY_MODE);
    lastMode = mode;
  }
  if (ti->tm_min != lastMin) {
    flintEventEmit(FLINT_EVT_MINUTE);
    lastMin = ti->tm_min;
    clockTimePersist();
  }
  if (ti->tm_sec != lastSec) {
    flintEventEmit(FLINT_EVT_SECOND);
    lastSec = ti->tm_sec;
  }
}

static void debugHeartbeat(const struct tm* tiOpt) {
#if DEBUG_LEVEL >= 1
  static uint32_t lastBeat = 0;
  if (flintMillis() - lastBeat < DEBUG_HEARTBEAT_MS) return;
  lastBeat = flintMillis();

  struct tm tiLocal{};
  const struct tm* ti = tiOpt;
  if (!ti) {
    if (clockTimeGetLocal(&tiLocal)) {
      ti = &tiLocal;
    }
  }

  char tbuf[20] = "--:--:--";
  const char* modeName = "-";
  if (ti) {
    snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
    modeName = dayModeName(dayModeFromHour(ti->tm_hour));
  }

  LOGI("hb", "usb=%d time=%s%s mode=%s heap=%u dirty=0x%x",
       (int)pcLinkConnected(), tbuf,
       clockTimeIsValid() && !clockTimeIsPcSynced() ? "~" : "",
       modeName,
       (unsigned)flintFreeHeap(),
       (unsigned)uiPeekDirty());
#else
  (void)tiOpt;
#endif
}

void appLoop() {
  watchDisplayMaintain();
  pcLinkMaintain();
  bleLinkMaintain();
  shellPollSerial();
  const auto* activeFace = FaceRegistry::activeId();
  if (studioAnimate(flintMillis(), activeFace && !strcmp(activeFace, "studio"))) flintEventEmit(FLINT_EVT_FORCE_REDRAW);

  struct tm ti{};
  const bool hasTime = clockTimeGetLocal(&ti);
  scanTimeEvents(hasTime ? &ti : nullptr);
  shellTick();
  debugHeartbeat(hasTime ? &ti : nullptr);

  // USB input wakes the task immediately; LED/connection polling is bounded
  // to 50 ms. Overlay animations retain their 20 ms cadence.
  uint32_t tickMs = clockTimeNextTickMs();
  if (tickMs > 50) tickMs = 50;  // USB attach/detach and LED pattern cadence.
  shellWaitForInput(animIsActive() && tickMs > 20 ? 20 : tickMs);
}
