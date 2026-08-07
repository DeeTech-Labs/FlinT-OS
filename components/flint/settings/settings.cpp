#include "settings/settings.h"
#include "config/config.h"
#include "config/debug.h"
#include "i18n/i18n.h"
#include "settings/cfg_fs.h"
#include "time/tz_db.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nvs.h"
#include "nvs_flash.h"

static FlintSettings gSettings{};
static bool gLoaded = false;

static void settingsApplyDefaults() {
  strncpy(gSettings.activeFace, ACTIVE_FACE_ID, sizeof(gSettings.activeFace) - 1);
  gSettings.activeFace[sizeof(gSettings.activeFace) - 1] = '\0';
  gSettings.showSeconds = SETTINGS_SEC_AUTO;
  gSettings.lang = SETTINGS_LANG_RU;
  gSettings.hourFormat = SETTINGS_HOUR_24;
  gSettings.dateFormat = SETTINGS_DATE_DMY4;
  gSettings.dateSep = SETTINGS_DATE_SEP_DOT;
  strncpy(gSettings.tzId, DEFAULT_TZ_ID, sizeof(gSettings.tzId) - 1);
  gSettings.tzId[sizeof(gSettings.tzId) - 1] = '\0';
}

static void settingsApplyAfterLoad() {
  flintI18nSetLang(gSettings.lang);
  clockTimeApplyTz(gSettings.tzId);
}

static bool settingsParseUiLine(char* line) {
  // Strip CR/LF and comments.
  char* hash = strchr(line, '#');
  if (hash) {
    *hash = '\0';
  }
  char* start = line;
  while (*start == ' ' || *start == '\t') {
    ++start;
  }
  char* end = start + strlen(start);
  while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n')) {
    *--end = '\0';
  }
  if (*start == '\0') {
    return true;
  }

  char* eq = strchr(start, '=');
  if (!eq) {
    return false;
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

  if (strcmp(key, "active_face") == 0) {
    strncpy(gSettings.activeFace, val, sizeof(gSettings.activeFace) - 1);
    gSettings.activeFace[sizeof(gSettings.activeFace) - 1] = '\0';
    return true;
  }
  if (strcmp(key, "tz_id") == 0) {
    strncpy(gSettings.tzId, val, sizeof(gSettings.tzId) - 1);
    gSettings.tzId[sizeof(gSettings.tzId) - 1] = '\0';
    return true;
  }

  char* endptr = nullptr;
  const unsigned long u = strtoul(val, &endptr, 10);
  if (endptr == val) {
    return false;
  }

  if (strcmp(key, "show_sec") == 0) {
    gSettings.showSeconds = static_cast<uint8_t>(u);
    return true;
  }
  if (strcmp(key, "lang") == 0) {
    gSettings.lang = static_cast<uint8_t>(u);
    return true;
  }
  if (strcmp(key, "hour_fmt") == 0) {
    gSettings.hourFormat = static_cast<uint8_t>(u);
    return true;
  }
  if (strcmp(key, "date_fmt") == 0) {
    if (u < SETTINGS_DATE_FORMAT_COUNT) {
      gSettings.dateFormat = static_cast<uint8_t>(u);
    }
    return true;
  }
  if (strcmp(key, "date_sep") == 0) {
    if (u < SETTINGS_DATE_SEP_COUNT) {
      gSettings.dateSep = static_cast<uint8_t>(u);
    }
    return true;
  }
  if (strcmp(key, "version") == 0) {
    return true;  // accepted, ignored beyond presence
  }
  return false;
}

static bool settingsLoadFromFile() {
  char buf[512];
  const int n = cfgFsReadFile(kCfgUiPath, buf, sizeof(buf));
  if (n < 0) {
    return false;
  }

  char* save = nullptr;
  for (char* line = strtok_r(buf, "\n", &save); line; line = strtok_r(nullptr, "\n", &save)) {
    settingsParseUiLine(line);
  }
  return true;
}

// One-shot migrate: old NVS "flint" UI keys → ui.cfg when file is missing.
static bool settingsMigrateFromNvs() {
  nvs_handle_t h;
  if (nvs_open("flint", NVS_READONLY, &h) != ESP_OK) {
    return false;
  }

  bool any = false;
  size_t len = sizeof(gSettings.activeFace);
  if (nvs_get_str(h, "active_face", gSettings.activeFace, &len) == ESP_OK) {
    any = true;
  }

  uint8_t u8 = 0;
  if (nvs_get_u8(h, "show_sec", &u8) == ESP_OK) {
    gSettings.showSeconds = u8;
    any = true;
  }
  if (nvs_get_u8(h, "lang", &u8) == ESP_OK) {
    gSettings.lang = u8;
    any = true;
  }
  if (nvs_get_u8(h, "hour_fmt", &u8) == ESP_OK) {
    gSettings.hourFormat = u8;
    any = true;
  }
  if (nvs_get_u8(h, "date_fmt", &u8) == ESP_OK && u8 < SETTINGS_DATE_FORMAT_COUNT) {
    gSettings.dateFormat = u8;
    any = true;
  }
  if (nvs_get_u8(h, "date_sep", &u8) == ESP_OK && u8 < SETTINGS_DATE_SEP_COUNT) {
    gSettings.dateSep = u8;
    any = true;
  }

  len = sizeof(gSettings.tzId);
  if (nvs_get_str(h, "tz_id", gSettings.tzId, &len) == ESP_OK) {
    any = true;
  }

  nvs_close(h);
  return any;
}

void settingsLoad() {
  if (gLoaded) {
    return;
  }
  settingsApplyDefaults();

  if (!cfgFsReady()) {
    LOGI("set", "cfg unavailable — defaults face=%s", gSettings.activeFace);
    settingsApplyAfterLoad();
    gLoaded = true;
    return;
  }

  if (settingsLoadFromFile()) {
    LOGI("set", "loaded face=%s sec=%u lang=%u tz=%s",
         gSettings.activeFace, (unsigned)gSettings.showSeconds, (unsigned)gSettings.lang,
         gSettings.tzId);
  } else if (settingsMigrateFromNvs()) {
    LOGI("set", "migrated from NVS face=%s", gSettings.activeFace);
    settingsSave();
  } else {
    LOGI("set", "no ui.cfg — defaults face=%s", gSettings.activeFace);
  }

  settingsApplyAfterLoad();
  gLoaded = true;
}

void settingsSave() {
  if (!cfgFsReady()) {
    LOGE("set", "cfg not ready — save skipped");
    return;
  }

  char buf[384];
  const int n = snprintf(
      buf, sizeof(buf),
      "# flint ui.cfg v1\n"
      "version=1\n"
      "active_face=%s\n"
      "show_sec=%u\n"
      "lang=%u\n"
      "hour_fmt=%u\n"
      "date_fmt=%u\n"
      "date_sep=%u\n"
      "tz_id=%s\n",
      gSettings.activeFace,
      (unsigned)gSettings.showSeconds,
      (unsigned)gSettings.lang,
      (unsigned)gSettings.hourFormat,
      (unsigned)gSettings.dateFormat,
      (unsigned)gSettings.dateSep,
      gSettings.tzId);
  if (n < 0 || static_cast<size_t>(n) >= sizeof(buf)) {
    LOGE("set", "ui.cfg serialize overflow");
    return;
  }

  if (!cfgFsWriteAtomic(kCfgUiPath, buf, static_cast<size_t>(n))) {
    LOGE("set", "ui.cfg write failed");
    return;
  }
  LOGI("set", "saved face=%s", gSettings.activeFace);
}

const FlintSettings& settingsGet() {
  if (!gLoaded) {
    settingsLoad();
  }
  return gSettings;
}

bool settingsSetActiveFace(const char* id) {
  if (!id || !id[0]) {
    return false;
  }
  strncpy(gSettings.activeFace, id, sizeof(gSettings.activeFace) - 1);
  gSettings.activeFace[sizeof(gSettings.activeFace) - 1] = '\0';
  settingsSave();
  return true;
}

void settingsSetShowSeconds(uint8_t mode) {
  gSettings.showSeconds = mode;
  settingsSave();
}

void settingsSetLang(uint8_t lang) {
  gSettings.lang = (lang == SETTINGS_LANG_EN) ? SETTINGS_LANG_EN : SETTINGS_LANG_RU;
  flintI18nSetLang(gSettings.lang);
  settingsSave();
}

void settingsSetHourFormat(uint8_t fmt) {
  gSettings.hourFormat = (fmt == SETTINGS_HOUR_12) ? SETTINGS_HOUR_12 : SETTINGS_HOUR_24;
  settingsSave();
}

void settingsSetDateFormat(uint8_t fmt) {
  gSettings.dateFormat =
      (fmt < SETTINGS_DATE_FORMAT_COUNT) ? fmt : static_cast<uint8_t>(SETTINGS_DATE_DMY4);
  settingsSave();
}

void settingsSetDateSep(uint8_t sep) {
  gSettings.dateSep =
      (sep < SETTINGS_DATE_SEP_COUNT) ? sep : static_cast<uint8_t>(SETTINGS_DATE_SEP_DOT);
  settingsSave();
}

void settingsSetTzId(const char* id) {
  const TzEntry* e = tzDbFind(id);
  strncpy(gSettings.tzId, e->id, sizeof(gSettings.tzId) - 1);
  gSettings.tzId[sizeof(gSettings.tzId) - 1] = '\0';
  clockTimeApplyTz(gSettings.tzId);
  settingsSave();
}

bool settingsResolveShowSeconds(bool dayProfileShowSeconds) {
  const FlintSettings& s = settingsGet();
  if (s.showSeconds == SETTINGS_SEC_AUTO) {
    return dayProfileShowSeconds;
  }
  return s.showSeconds == SETTINGS_SEC_ON;
}

bool settingsUse12Hour() {
  return settingsGet().hourFormat == SETTINGS_HOUR_12;
}

uint8_t settingsDateFormat() {
  return settingsGet().dateFormat;
}

uint8_t settingsDateSep() {
  return settingsGet().dateSep;
}
