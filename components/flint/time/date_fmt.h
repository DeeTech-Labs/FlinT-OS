#pragma once

#include <stddef.h>
#include <stdint.h>
#include <time.h>

// Numeric date layout (order + year width). Separator is independent.
enum SettingsDateFormat : uint8_t {
  SETTINGS_DATE_DMY2 = 0,  // 31.12.26
  SETTINGS_DATE_DMY4 = 1,  // 31.12.2026 (default)
  SETTINGS_DATE_MDY2 = 2,  // 12.31.26
  SETTINGS_DATE_MDY4 = 3,  // 12.31.2026
  SETTINGS_DATE_YMD4 = 4,  // 2026.12.31 (ISO-style order)
  SETTINGS_DATE_FORMAT_COUNT = 5,
};

enum SettingsDateSep : uint8_t {
  SETTINGS_DATE_SEP_DOT = 0,    // .
  SETTINGS_DATE_SEP_SLASH = 1,  // /
  SETTINGS_DATE_SEP_DASH = 2,   // -
  SETTINGS_DATE_SEP_COUNT = 3,
};

char flintDateSepChar(uint8_t sep);

// Format numeric date only (no weekday). out e.g. "31.12.2026".
void flintFormatDate(char* out, size_t outN, const struct tm& ti, uint8_t dateFormat,
                     uint8_t dateSep);

// Weekday + numeric date, or waiting-for-time copy when !hasTime.
void flintFormatDateLine(char* out, size_t outN, bool hasTime, const struct tm& ti,
                         uint8_t dateFormat, uint8_t dateSep);

// Fixed sample label for settings UI (unambiguous day/month): uses 31 Dec 2026.
void flintDateFormatSample(char* out, size_t outN, uint8_t dateFormat, uint8_t dateSep);
