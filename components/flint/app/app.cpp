#include "app.h"
#include "config/config.h"
#include "config/debug.h"
#include "display/display.h"
#include "net/wifi_net.h"
#include "time/clock_time.h"
#include "weather/weather.h"
#include "day_mode/day_mode.h"
#include "shell/shell.h"
#include "ui/ui.h"
#include "ui/ui_dirty.h"
#include "util/flint_sys.h"

#include <stdio.h>

void appSetup() {
  flintDelay(200);
  printf("\n======== FlinT OS boot ========\n");
  LOGI("app", "DEBUG_LEVEL=%d", DEBUG_LEVEL);

  displayBegin();
  LOGI("disp", "screen %dx%d canvas=%d", SCR_W, SCR_H, (int)displayHasCanvas());

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

  // Start NTP as soon as Wi‑Fi is up. Restart only if first sync stalls.
  static uint32_t lastNtpBeginMs = 0;
  const bool needFirst = !clockTimeIsValid();
  const bool needResync = clockTimeIsValid()
                          && (flintMillis() - clockTimeLastSyncMs()) > NTP_RESYNC_MS;

  if (needFirst || needResync) {
    const uint32_t now = flintMillis();
    const uint32_t stallMs = needFirst ? 5000UL : 10000UL;
    if (lastNtpBeginMs == 0 || (now - lastNtpBeginMs) >= stallMs) {
      clockTimeBeginNtp();
      lastNtpBeginMs = now ? now : 1;
    }
  }

  const bool wasValid = clockTimeIsValid();
  if (clockTimePollNtp() && !wasValid) {
    uiMarkDirty(UI_DIRTY_TIME_VALID | UI_DIRTY_FORCE);
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
    uiMarkDirty(UI_DIRTY_DAY_MODE);
    lastMode = mode;
  }
  if (ti.tm_min != lastMin) {
    uiMarkDirty(UI_DIRTY_MINUTE);
    lastMin = ti.tm_min;
  }
  if (ti.tm_sec != lastSec) {
    uiMarkDirty(UI_DIRTY_SECOND);
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

  WeatherState w = weatherGet();
  const char* modeName = "-";
  if (clockTimeGetLocal(&ti)) {
    modeName = dayModeNameRu(dayModeFromHour(ti.tm_hour));
  }
  LOGI("hb", "wifi=%d time=%s mode=%s wx=%s temp=%.1f heap=%u dirty=0x%x",
       (int)wifiNetIsConnected(), tbuf, modeName,
       w.ok ? weatherCodeRu(w.code) : (w.loading ? "loading" : "none"),
       w.ok ? w.tempC : 0.0f,
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

  // Idle longer when nothing to paint (event-driven faces).
  if (uiHasDirty()) {
    flintDelay(20);
  } else {
    flintDelay(uiWantsSeconds() ? 50 : 200);
  }
}
