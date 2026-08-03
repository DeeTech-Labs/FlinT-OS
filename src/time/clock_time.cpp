#include "clock_time.h"
#include "src/config/config.h"
#include "src/config/debug.h"

#include <Arduino.h>
#include <time.h>

static bool timeValid = false;
static bool ntpStarted = false;
static time_t baseEpoch = 0;
static uint32_t baseMillis = 0;
static uint32_t lastNtpSyncMs = 0;

bool clockTimeIsValid() {
  return timeValid;
}

uint32_t clockTimeLastSyncMs() {
  return lastNtpSyncMs;
}

bool clockTimeGetLocal(struct tm* out) {
  if (!timeValid || !out) {
    return false;
  }
  time_t now = baseEpoch + (time_t)((millis() - baseMillis) / 1000UL);
  struct tm* ti = localtime(&now);
  if (!ti) {
    return false;
  }
  *out = *ti;
  return true;
}

void clockTimeBeginNtp() {
  LOGI("ntp", "begin server=%s offset=%ld", NTP_SERVER, (long)GMT_OFFSET_SEC);
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  ntpStarted = true;
}

bool clockTimePollNtp() {
  if (!ntpStarted) {
    return timeValid;
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 0)) {
    return timeValid;
  }

  time_t now = 0;
  time(&now);
  if (now <= 1700000000) {
    return timeValid;
  }

  const bool first = !timeValid;
  baseEpoch = now;
  baseMillis = millis();
  lastNtpSyncMs = millis();
  timeValid = true;

  if (first) {
    LOGI("ntp", "ok %04d-%02d-%02d %02d:%02d:%02d",
         timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  } else {
    LOGD("ntp", "resync ok");
  }
  return true;
}
