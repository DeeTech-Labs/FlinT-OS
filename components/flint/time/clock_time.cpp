#include "clock_time.h"
#include "config/config.h"
#include "config/debug.h"
#include "settings/cfg_fs.h"
#include "settings/settings.h"
#include "time/tz_db.h"
#include "util/flint_sys.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "esp_sntp.h"
#include "nvs.h"
#include "nvs_flash.h"

static bool timeValid = false;
static bool timeNtpSynced = false;
static bool ntpStarted = false;
static bool ntpApplyPending = false;
static time_t baseEpoch = 0;
// Keep elapsed clock time in 64 bits; millis wraps after about 49.7 days.
static int64_t baseMicros = 0;
static uint32_t lastNtpSyncMs = 0;
static uint32_t lastPersistMs = 0;

static constexpr time_t kMinEpoch = 1700000000;  // ~2023-11

static time_t clockNowEpoch() {
  if (!timeValid) {
    return 0;
  }
  return baseEpoch + (time_t)((esp_timer_get_time() - baseMicros) / 1000000LL);
}

static void clockSetBase(time_t epoch, bool fromNtp) {
  baseEpoch = epoch;
  baseMicros = esp_timer_get_time();
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
  if (!cfgFsReady()) {
    return;
  }

  char buf[96];
  const int n = snprintf(buf, sizeof(buf),
                         "# flint time.cfg v1\n"
                         "version=1\n"
                         "epoch=%llu\n",
                         static_cast<unsigned long long>(epoch));
  if (n < 0 || static_cast<size_t>(n) >= sizeof(buf)) {
    return;
  }
  if (!cfgFsWriteAtomic(kCfgTimePath, buf, static_cast<size_t>(n))) {
    LOGE("ntp", "time.cfg write failed");
    return;
  }
  lastPersistMs = flintMillis();
}

static bool clockParseEpochFile(const char* text, time_t* out) {
  char buf[128];
  strncpy(buf, text, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  char* save = nullptr;
  for (char* line = strtok_r(buf, "\n", &save); line; line = strtok_r(nullptr, "\n", &save)) {
    char* hash = strchr(line, '#');
    if (hash) {
      *hash = '\0';
    }
    char* start = line;
    while (*start == ' ' || *start == '\t') {
      ++start;
    }
    if (*start == '\0') {
      continue;
    }
    char* eq = strchr(start, '=');
    if (!eq) {
      continue;
    }
    *eq = '\0';
    char* key = start;
    char* val = eq + 1;
    while (*val == ' ' || *val == '\t') {
      ++val;
    }
    char* keyEnd = key + strlen(key);
    while (keyEnd > key && (keyEnd[-1] == ' ' || keyEnd[-1] == '\t')) {
      *--keyEnd = '\0';
    }
    if (strcmp(key, "epoch") != 0) {
      continue;
    }
    char* endptr = nullptr;
    const unsigned long long u = strtoull(val, &endptr, 10);
    if (endptr == val) {
      return false;
    }
    const time_t epoch = static_cast<time_t>(u);
    if (epoch < kMinEpoch) {
      return false;
    }
    *out = epoch;
    return true;
  }
  return false;
}

// One-shot migrate: old NVS epoch_lo/hi → time.cfg when file is missing.
static bool clockMigrateEpochFromNvs(time_t* out) {
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

static bool clockLoadEpoch(time_t* out) {
  if (cfgFsReady()) {
    char buf[128];
    if (cfgFsReadFile(kCfgTimePath, buf, sizeof(buf)) >= 0) {
      if (clockParseEpochFile(buf, out)) {
        return true;
      }
    }
  }

  if (clockMigrateEpochFromNvs(out)) {
    LOGI("ntp", "migrated soft time from NVS");
    clockSaveEpoch(*out);
    return true;
  }
  return false;
}

static void clockSeedEditableLocal(struct tm* out) {
  if (clockTimeGetLocal(out)) {
    return;
  }
  memset(out, 0, sizeof(*out));
  out->tm_year = 2026 - 1900;
  out->tm_mon = 0;
  out->tm_mday = 1;
  out->tm_hour = 12;
  out->tm_min = 0;
  out->tm_sec = 0;
  out->tm_isdst = -1;
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

bool clockTimeSetLocal(const struct tm* local) {
  if (!local) {
    return false;
  }
  struct tm editable = *local;
  editable.tm_isdst = -1;
  const time_t epoch = mktime(&editable);
  if (epoch == static_cast<time_t>(-1) || epoch < kMinEpoch) {
    return false;
  }
  clockSetBase(epoch, false);
  // Hold manual value until the next explicit NTP begin/apply cycle.
  ntpApplyPending = false;
  clockSaveEpoch(epoch);
  LOGI("ntp", "manual soft time %04d-%02d-%02d %02d:%02d:%02d",
       editable.tm_year + 1900, editable.tm_mon + 1, editable.tm_mday,
       editable.tm_hour, editable.tm_min, editable.tm_sec);
  return true;
}

bool clockTimeAdjustMinutes(int deltaMin) {
  struct tm ti{};
  clockSeedEditableLocal(&ti);
  ti.tm_min += deltaMin;
  return clockTimeSetLocal(&ti);
}

bool clockTimeAdjustDays(int deltaDays) {
  struct tm ti{};
  clockSeedEditableLocal(&ti);
  ti.tm_mday += deltaDays;
  return clockTimeSetLocal(&ti);
}

void clockTimeSoftBegin() {
  clockTimeApplyTz(settingsGet().tzId);

  time_t epoch = 0;
  if (!clockLoadEpoch(&epoch)) {
    LOGI("ntp", "no soft time in cfg");
    return;
  }
  clockSetBase(epoch, false);
  struct tm ti{};
  localtime_r(&epoch, &ti);
  LOGI("ntp", "soft time from cfg %04d-%02d-%02d %02d:%02d:%02d (until NTP)",
       ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec);
}

void clockTimePersist() {
  if (!timeValid) {
    return;
  }
  const uint32_t now = flintMillis();
  // Throttle cfg writes: every 60s when running; NTP first sync writes immediately.
  if (lastPersistMs != 0 && (now - lastPersistMs) < 60000UL) {
    return;
  }
  clockSaveEpoch(clockNowEpoch());
}

void clockTimeBeginNtp() {
  clockTimeApplyTz(settingsGet().tzId);
  LOGI("ntp", "begin server=%s tz=%s", NTP_SERVER, settingsGet().tzId);

  ntpApplyPending = true;
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

  if (!ntpApplyPending) {
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
  ntpApplyPending = false;

  if (firstNtp) {
    LOGI("ntp", "synced %04d-%02d-%02d %02d:%02d:%02d",
         timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  }
  return true;
}
