#include "app.h"
#include "src/config/config.h"
#include "src/config/debug.h"
#include "src/display/display.h"
#include "src/net/wifi_net.h"
#include "src/time/clock_time.h"
#include "src/weather/weather.h"
#include "src/day_mode/day_mode.h"
#include "src/ui/ui.h"

#include <WiFi.h>
#include <stdio.h>

void appSetup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println(F("======== FlinT OS boot ========"));
  LOGI("app", "DEBUG_LEVEL=%d", DEBUG_LEVEL);

  displayBegin();
  LOGI("disp", "screen %dx%d", SCR_W, SCR_H);
  uiBegin();
  uiForceRedraw();
  uiTick();

  wifiNetBegin();
  weatherStartService();

  LOGI("app", "setup done (async wifi/ntp/weather)");
}

static void maintainServices() {
  wifiNetMaintain();
  if (!wifiNetIsConnected()) {
    return;
  }

  static uint32_t lastNtpBeginMs = 0;
  const bool needResync = clockTimeIsValid()
                          && (millis() - clockTimeLastSyncMs()) > NTP_RESYNC_MS;
  const bool needFirst = !clockTimeIsValid();

  if ((needFirst || needResync) && (millis() - lastNtpBeginMs > 10000UL)) {
    clockTimeBeginNtp();
    lastNtpBeginMs = millis();
  }

  const bool wasValid = clockTimeIsValid();
  if (clockTimePollNtp() && !wasValid) {
    uiForceRedraw();
  }
}

static void debugHeartbeat() {
#if DEBUG_LEVEL >= 1
  static uint32_t lastBeat = 0;
  if (millis() - lastBeat < DEBUG_HEARTBEAT_MS) return;
  lastBeat = millis();

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
  LOGI("hb", "wifi=%d time=%s mode=%s wx=%s temp=%.1f heap=%u",
       (int)wifiNetIsConnected(), tbuf, modeName,
       w.ok ? weatherCodeRu(w.code) : (w.loading ? "loading" : "none"),
       w.ok ? w.tempC : 0.0f,
       (unsigned)ESP.getFreeHeap());
#endif
}

void appLoop() {
  maintainServices();
  uiTick();
  debugHeartbeat();
  delay(50);
}
