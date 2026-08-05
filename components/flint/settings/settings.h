#pragma once

#include <stdint.h>

#include "time/date_fmt.h"

// Persistent prefs (NVS namespace "flint"). Defaults come from config.h.

enum SettingsShowSeconds : uint8_t {
  SETTINGS_SEC_OFF = 0,
  SETTINGS_SEC_ON = 1,
  SETTINGS_SEC_AUTO = 255,  // follow day_mode profile
};

enum SettingsLang : uint8_t {
  SETTINGS_LANG_RU = 0,
  SETTINGS_LANG_EN = 1,
};

enum SettingsHourFormat : uint8_t {
  SETTINGS_HOUR_24 = 0,
  SETTINGS_HOUR_12 = 1,
};

struct FlintSettings {
  char activeFace[32];
  uint8_t showSeconds;  // SettingsShowSeconds
  uint8_t lang;         // SettingsLang
  uint8_t hourFormat;   // SettingsHourFormat
  uint8_t dateFormat;   // SettingsDateFormat
  uint8_t dateSep;      // SettingsDateSep
  float wxLat;
  float wxLon;
  char tzId[32];
};

void settingsLoad();
void settingsSave();
const FlintSettings& settingsGet();

bool settingsSetActiveFace(const char* id);
void settingsSetShowSeconds(uint8_t mode);
void settingsSetLang(uint8_t lang);
void settingsSetHourFormat(uint8_t fmt);
void settingsSetDateFormat(uint8_t fmt);
void settingsSetDateSep(uint8_t sep);
void settingsSetWeatherCoords(float lat, float lon);
void settingsSetTzId(const char* id);

// Resolved showSeconds for current day profile (auto → profile flag).
bool settingsResolveShowSeconds(bool dayProfileShowSeconds);
bool settingsUse12Hour();
uint8_t settingsDateFormat();
uint8_t settingsDateSep();
