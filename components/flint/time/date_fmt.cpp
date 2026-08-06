#include "time/date_fmt.h"

#include "i18n/i18n.h"

#include <stdio.h>

char flintDateSepChar(uint8_t sep) {
  switch (sep) {
    case SETTINGS_DATE_SEP_SLASH:
      return '/';
    case SETTINGS_DATE_SEP_DASH:
      return '-';
    case SETTINGS_DATE_SEP_DOT:
    default:
      return '.';
  }
}

static void formatParts(char* out, size_t outN, int a, int b, int c, bool yearFirst4,
                        bool yearLast2, bool yearLast4, char sep) {
  if (yearFirst4) {
    snprintf(out, outN, "%04d%c%02d%c%02d", a, sep, b, sep, c);
  } else if (yearLast2) {
    snprintf(out, outN, "%02d%c%02d%c%02d", a, sep, b, sep, c);
  } else if (yearLast4) {
    snprintf(out, outN, "%02d%c%02d%c%04d", a, sep, b, sep, c);
  } else {
    out[0] = '\0';
  }
}

void flintFormatDate(char* out, size_t outN, const struct tm& ti, uint8_t dateFormat,
                     uint8_t dateSep) {
  if (!out || outN == 0) {
    return;
  }
  const char sep = flintDateSepChar(dateSep);
  const int day = ti.tm_mday;
  const int mon = ti.tm_mon + 1;
  const int year = ti.tm_year + 1900;
  const int yy = year % 100;

  switch (dateFormat) {
    case SETTINGS_DATE_DMY2:
      formatParts(out, outN, day, mon, yy, false, true, false, sep);
      break;
    case SETTINGS_DATE_MDY2:
      formatParts(out, outN, mon, day, yy, false, true, false, sep);
      break;
    case SETTINGS_DATE_MDY4:
      formatParts(out, outN, mon, day, year, false, false, true, sep);
      break;
    case SETTINGS_DATE_YMD4:
      formatParts(out, outN, year, mon, day, true, false, false, sep);
      break;
    case SETTINGS_DATE_DMY4:
    default:
      formatParts(out, outN, day, mon, year, false, false, true, sep);
      break;
  }
}

void flintFormatDateLine(char* out, size_t outN, bool hasTime, const struct tm& ti,
                         uint8_t dateFormat, uint8_t dateSep) {
  if (!out || outN == 0) {
    return;
  }
  if (!hasTime) {
    snprintf(out, outN, "%s", flintTr(FlintStr::WaitingForTime));
    return;
  }
  char num[16];
  flintFormatDate(num, sizeof(num), ti, dateFormat, dateSep);
  snprintf(out, outN, "%s  %s", flintTrWeekday(ti.tm_wday), num);
}

void flintDateFormatSample(char* out, size_t outN, uint8_t dateFormat, uint8_t dateSep) {
  struct tm sample{};
  sample.tm_mday = 31;
  sample.tm_mon = 11;  // December
  sample.tm_year = 2026 - 1900;
  flintFormatDate(out, outN, sample, dateFormat, dateSep);
}
