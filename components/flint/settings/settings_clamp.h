#pragma once

#include <stdint.h>

#include "settings/settings.h"

// Pure clamp helpers for prefs enums (host-testable; used by settings setters).

inline uint8_t settingsClampHourFormat(uint8_t fmt) {
  return (fmt == SETTINGS_HOUR_12) ? SETTINGS_HOUR_12 : SETTINGS_HOUR_24;
}

inline uint8_t settingsClampDateFormat(uint8_t fmt) {
  return (fmt < SETTINGS_DATE_FORMAT_COUNT) ? fmt
                                            : static_cast<uint8_t>(SETTINGS_DATE_DMY4);
}

inline uint8_t settingsClampDateSep(uint8_t sep) {
  return (sep < SETTINGS_DATE_SEP_COUNT) ? sep
                                         : static_cast<uint8_t>(SETTINGS_DATE_SEP_DOT);
}

inline uint8_t settingsClampLang(uint8_t lang) {
  return (lang == SETTINGS_LANG_EN) ? SETTINGS_LANG_EN : SETTINGS_LANG_RU;
}

inline uint8_t settingsClampShowSeconds(uint8_t mode) {
  if (mode == SETTINGS_SEC_ON || mode == SETTINGS_SEC_OFF || mode == SETTINGS_SEC_AUTO) {
    return mode;
  }
  return SETTINGS_SEC_AUTO;
}
