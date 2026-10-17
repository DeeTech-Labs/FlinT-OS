#include "i18n/i18n.h"

#include "day_mode/day_mode_hour.h"

static uint8_t gLang = FLINT_I18N_RU;

void flintI18nSetLang(uint8_t lang) {
  gLang = (lang == FLINT_I18N_EN) ? FLINT_I18N_EN : FLINT_I18N_RU;
}

uint8_t flintI18nLang() {
  return gLang;
}

const char* flintI18nLocaleId() {
  return gLang == FLINT_I18N_EN ? "en_US" : "ru_RU";
}

const char* flintTr(FlintStr id) {
  const unsigned i = static_cast<unsigned>(id);
  if (i >= static_cast<unsigned>(FlintStr::COUNT)) {
    return "?";
  }
  const char* const* table = (gLang == FLINT_I18N_EN) ? kFlintStrEn : kFlintStrRu;
  const char* s = table[i];
  return (s && s[0]) ? s : kFlintStrMsgid[i];
}

const char* flintTrWeekday(int wday) {
  static const FlintStr kDays[] = {
      FlintStr::Sun, FlintStr::Mon, FlintStr::Tue, FlintStr::Wed,
      FlintStr::Thu, FlintStr::Fri, FlintStr::Sat,
  };
  if (wday < 0 || wday > 6) {
    return "?";
  }
  return flintTr(kDays[wday]);
}

const char* flintTrDayMode(uint8_t dayMode) {
  switch (static_cast<DayMode>(dayMode)) {
    case DAY_MODE_MORNING: return flintTr(FlintStr::Morning);
    case DAY_MODE_DAY: return flintTr(FlintStr::Day);
    case DAY_MODE_EVENING: return flintTr(FlintStr::Evening);
    case DAY_MODE_NIGHT: return flintTr(FlintStr::Night);
    case DAY_MODE_WAITING:
    default: return flintTr(FlintStr::WaitingForTime);
  }
}
