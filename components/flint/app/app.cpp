#include "app.h"
#include "config/config.h"
#include "config/debug.h"
#include "day_mode/day_mode.h"
#include "display/display.h"
#include "event/event.h"
#include "net/wifi_net.h"
#include "shell/shell.h"
#include "time/clock_time.h"
#include "ui/splash.h"
#include "ui/ui.h"
#include "ui/ui_dirty.h"
#include "util/flint_sys.h"
#include "weather/weather.h"

#include <stdio.h>

void appSetup() {
  printf("\n======== FlinT OS boot ========\n");
  LOGI("app", "DEBUG_LEVEL=%d board=%s", DEBUG_LEVEL, FLINT_BOARD_ID);

  // Init panel ASAP (fills brand bg) — no delay before this, avoids white flash.
  displayBegin();
  LOGI("disp", "screen %dx%d canvas=%d", SCR_W, SCR_H, (int)displayHasCanvas());

  flintEventInit();
  splashShow();  // ~3 s brand anim; blocks before face

  // Show last-known time immediately (NVS); NTP will correct after Wi‑Fi.
  clockTimeSoftBegin();
  if (clockTimeIsValid()) {
    flintEventEmit(FLINT_EVT_TIME_VALID);
  }

  shellBegin();
  shellTick();

  wifiNetBegin();
  weatherStartService();

  LOGI("app", "setup done (async wifi/ntp/weather); serial: h=help");
}

static void maintainServices() {
  wifiNetMaintain();
  if (!wifiNetIsConnected()) {
    return;
  }

  static uint32_t lastNtpBeginMs = 0;
  const bool needFirst = !clockTimeIsNtpSynced();
  const bool needResync = clockTimeIsNtpSynced()
                          && (flintMillis() - clockTimeLastSyncMs()) > NTP_RESYNC_MS;

  if (needFirst || needResync) {
    const uint32_t now = flintMillis();
    const uint32_t stallMs = needFirst ? 5000UL : 10000UL;
    if (lastNtpBeginMs == 0 || (now - lastNtpBeginMs) >= stallMs) {
      clockTimeBeginNtp();
      lastNtpBeginMs = now ? now : 1;
    }
  }

  const bool wasNtp = clockTimeIsNtpSynced();
  if (clockTimePollNtp() && !wasNtp && clockTimeIsNtpSynced()) {
    flintEventEmit(FLINT_EVT_TIME_VALID);
  }
}

static void scanTimeEvents() {
  static bool haveLast = false;
  static int lastSec = -1;
  static int lastMin = -1;
  static DayMode lastMode = DAY_MODE_DAY;

  struct tm ti{};
  if (!clockTimeGetLocal(&ti)) {
    haveLast = false;
    return;
  }

  const DayMode mode = dayModeFromHour(ti.tm_hour);
  if (!haveLast) {
    lastSec = ti.tm_sec;
    lastMin = ti.tm_min;
    lastMode = mode;
    haveLast = true;
    return;
  }

  if (mode != lastMode) {
    flintEventEmit(FLINT_EVT_DAY_MODE);
    lastMode = mode;
  }
  if (ti.tm_min != lastMin) {
    flintEventEmit(FLINT_EVT_MINUTE);
    lastMin = ti.tm_min;
    clockTimePersist();
  }
  if (ti.tm_sec != lastSec) {
    flintEventEmit(FLINT_EVT_SECOND);
    lastSec = ti.tm_sec;
  }
}

static void debugHeartbeat() {
#if DEBUG_LEVEL >= 1
  static uint32_t lastBeat = 0;
  if (flintMillis() - lastBeat < DEBUG_HEARTBEAT_MS) return;
  lastBeat = flintMillis();

  struct tm ti;
  char tbuf[20] = "--:--:--";
  if (clockTimeGetLocal(&ti)) {
    snprintf(tbuf, sizeof(tbuf), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
  }

  const WeatherState& w = weatherSnapshot();
  const char* modeName = "-";
  if (clockTimeGetLocal(&ti)) {
    modeName = dayModeNameRu(dayModeFromHour(ti.tm_hour));
  }
  LOGI("hb", "wifi=%d time=%s%s mode=%s wx=%s temp=%d.%d heap=%u dirty=0x%x",
       (int)wifiNetIsConnected(), tbuf,
       clockTimeIsValid() && !clockTimeIsNtpSynced() ? "~" : "",
       modeName,
       w.ok ? weatherCodeRu(w.code) : (w.loading ? "loading" : "none"),
       w.ok ? weatherTempCWhole(w.tempCx10) : 0,
       w.ok ? ((w.tempCx10 >= 0 ? w.tempCx10 : -w.tempCx10) % 10) : 0,
       (unsigned)flintFreeHeap(),
       (unsigned)uiPeekDirty());
#endif
}

void appLoop() {
  shellPollSerial();
  maintainServices();
  scanTimeEvents();
  shellTick();
  debugHeartbeat();

  if (uiHasDirty()) {
    flintDelay(20);
  } else {
    flintDelay(uiWantsSeconds() ? 50 : 200);
  }
}
