#pragma once

#include <stdint.h>

enum SettingsRow : uint8_t {
  SET_ROW_FACE, SET_ROW_CLOCK_TIME, SET_ROW_CLOCK_DATE, SET_ROW_SECONDS,
  SET_ROW_HOUR, SET_ROW_DATE, SET_ROW_DATE_SEP, SET_ROW_LANG, SET_ROW_TZ,
  SET_ROW_USB, SET_ROW_CLOCK_STATUS,
};

enum SettingsPage : uint8_t { SET_HOME, SET_DISPLAY, SET_CLOCK, SET_CONNECTION };

inline int settingsPageRowCount(SettingsPage page) {
  switch (page) {
    case SET_HOME: return 3;
    case SET_DISPLAY: return 3;
    case SET_CLOCK: return 6;
    case SET_CONNECTION: return 2;
  }
  return 0;
}

inline SettingsRow settingsPageRow(SettingsPage page, int index) {
  static constexpr SettingsRow display[] = {SET_ROW_FACE, SET_ROW_SECONDS, SET_ROW_LANG};
  static constexpr SettingsRow clock[] = {SET_ROW_CLOCK_TIME, SET_ROW_CLOCK_DATE,
    SET_ROW_HOUR, SET_ROW_DATE, SET_ROW_DATE_SEP, SET_ROW_TZ};
  static constexpr SettingsRow connection[] = {SET_ROW_USB, SET_ROW_CLOCK_STATUS};
  if (index < 0 || index >= settingsPageRowCount(page)) return SET_ROW_FACE;
  switch (page) {
    case SET_DISPLAY: return display[index];
    case SET_CLOCK: return clock[index];
    case SET_CONNECTION: return connection[index];
    default: return SET_ROW_FACE;
  }
}
