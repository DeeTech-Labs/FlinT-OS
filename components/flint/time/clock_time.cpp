#include "clock_time.h"
#include "config/config.h"
#include "config/debug.h"
#include "settings/settings.h"
#include "time/tz_db.h"
#include "util/flint_sys.h"

#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include "esp_sntp.h"
#include "nvs.h"
#include "nvs_flash.h"

static bool timeValid = false;
static bool timeNtpSynced = false;
static bool ntpStarted = false;
static time_t baseEpoch = 0;
static uint32_t baseMillis = 0;
static uint32_t lastNtpSyncMs = 0;
static uint32_t lastPersistMs = 0;

static constexpr time_t kMinEpoch = 1700000000;  // ~2023-11

static time_t clockNowEpoch() {
  if (!timeValid) {
    return 0;
  }
  return baseEpoch + (time_t)((flintMillis() - baseMillis) / 1000UL);
}

static void clockSetBase(time_t epoch, bool fromNtp) {
  baseEpoch = epoch;
  baseMillis = flintMillis();
  timeValid = true;
  if (fromNtp) {
    timeNtpSynced = true;
    lastNtpSyncMs = flintMillis();
  }
}

static void clockSaveEpoch(time_t epoch) {
  if (epoch < kMinEpoch) {
    return;
  }
  nvs_handle_t h;
  if (nvs_open("flint", NVS_READWRITE, &h) != ESP_OK) {
    return;
  }
  // Store as two u32 for portability (time_t may be 64-bit).
  nvs_set_u32(h, "epoch_lo", static_cast<uint32_t>(epoch & 0xffffffffu));
  nvs_set_u32(h, "epoch_hi", static_cast<uint32_t>((static_cast<uint64_t>(epoch) >> 32) & 0xffffffffu));
  nvs_commit(h);
  nvs_close(h);
  lastPersistMs = flintMillis();
}

static bool clockLoadEpoch(time_t* out) {
  nvs_handle_t h;
  if (nvs_open("flint", NVS_READONLY, &h) != ESP_OK) {
    return false;
  }
  uint32_t lo = 0, hi = 0;
  const esp_err_t e1 = nvs_get_u32(h, "epoch_lo", &lo);
  const esp_err_t e2 = nvs_get_u32(h, "epoch_hi", &hi);
  nvs_close(h);
  if (e1 != ESP_OK) {
    return false;
  }
  if (e2 != ESP_OK) {
    hi = 0;
  }
  const time_t epoch = static_cast<time_t>((static_cast<uint64_t>(hi) << 32) | lo);
  if (epoch < kMinEpoch) {
    return false;
  }
  *out = epoch;
  return true;
}

bool clockTimeIsValid() {
  return timeValid;
}

bool clockTimeIsNtpSynced() {
  return timeNtpSynced;
}

uint32_t clockTimeLastSyncMs() {
  return lastNtpSyncMs;
}

bool clockTimeGetLocal(struct tm* out) {
  if (!timeValid || !out) {
    return false;
  }
  time_t now = clockNowEpoch();
  struct tm* ti = localtime(&now);
  if (!ti) {
    return false;
  }
  *out = *ti;
  return true;
}

void clockTimeSoftBegin() {
  clockTimeApplyTz(settingsGet().tzId);

  time_t epoch = 0;
  if (!clockLoadEpoch(&epoch)) {
    LOGI("ntp", "no soft time in NVS");
    return;
  }
  clockSetBase(epoch, false);
  struct tm ti{};
  localtime_r(&epoch, &ti);
  LOGI("ntp", "soft time from NVS %04d-%02d-%02d %02d:%02d:%02d (until NTP)",
       ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec);
}

void clockTimePersist() {
  if (!timeValid) {
    return;
  }
  const uint32_t now = flintMillis();
  // Throttle NVS writes: every 60s when running, always ok after NTP first sync via poll.
  if (lastPersistMs != 0 && (now - lastPersistMs) < 60000UL) {
    return;
  }
  clockSaveEpoch(clockNowEpoch());
}

void clockTimeBeginNtp() {
  clockTimeApplyTz(settingsGet().tzId);
  LOGI("ntp", "begin server=%s tz=%s", NTP_SERVER, settingsGet().tzId);

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
  if (now < kMinEpoch) {
    return timeValid;
  }

  struct tm timeinfo{};
  localtime_r(&now, &timeinfo);

  const bool firstNtp = !timeNtpSynced;
  clockSetBase(now, true);
  clockSaveEpoch(now);

  if (firstNtp) {
    LOGI("ntp", "synced %04d-%02d-%02d %02d:%02d:%02d",
         timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
  return true;
}
