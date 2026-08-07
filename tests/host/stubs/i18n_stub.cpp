#include "i18n/i18n.h"

void flintI18nSetLang(uint8_t /*lang*/) {}

uint8_t flintI18nLang() {
  return FLINT_I18N_EN;
}

const char* flintI18nLocaleId() {
  return "en_US";
}

const char* flintTr(FlintStr id) {
  if (id == FlintStr::WaitingForTime) {
    return "Waiting for time";
  }
  return "?";
}

const char* flintTrWeekday(int wday) {
  static const char* kDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  if (wday < 0 || wday > 6) {
    return "?";
  }
  return kDays[wday];
}

const char* flintTrDayMode(uint8_t /*dayMode*/) {
  return "?";
}
