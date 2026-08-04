#include "clock_time.h"
#include "config/config.h"
#include "config/debug.h"
#include "util/flint_sys.h"

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "esp_sntp.h"

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
  time_t now = baseEpoch + (time_t)((flintMillis() - baseMillis) / 1000UL);
  struct tm* ti = localtime(&now);
  if (!ti) {
    return false;
  }
  *out = *ti;
  return true;
}

void clockTimeBeginNtp() {
  LOGI("ntp", "begin server=%s tz=MSK-3", NTP_SERVER);

  // Europe/Moscow, no DST (UTC+3)
  setenv("TZ", "MSK-3", 1);
  tzset();

  if (!ntpStarted) {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    esp_sntp_setservername(0, NTP_SERVER);
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_setservername(2, "time.cloudflare.com");
    esp_sntp_init();
    ntpStarted = true;
  } else {
    esp_sntp_restart();
  }
}

bool clockTimePollNtp() {
  if (!ntpStarted) {
    return timeValid;
  }

  if (esp_sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
    return timeValid;
  }

  time_t now = 0;
  time(&now);
  if (now <= 1700000000) {
    return timeValid;
  }

  struct tm timeinfo{};
  localtime_r(&now, &timeinfo);

  const bool first = !timeValid;
  baseEpoch = now;
  baseMillis = flintMillis();
  lastNtpSyncMs = flintMillis();
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
