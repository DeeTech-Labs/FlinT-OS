#pragma once

#include <stdint.h>

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
void settingsSetWeatherCoords(float lat, float lon);
void settingsSetTzId(const char* id);

// Resolved showSeconds for current day profile (auto → profile flag).
bool settingsResolveShowSeconds(bool dayProfileShowSeconds);
bool settingsUse12Hour();
