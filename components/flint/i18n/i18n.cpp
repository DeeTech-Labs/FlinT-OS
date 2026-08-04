#include "i18n/i18n.h"

#include "day_mode/day_mode.h"

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
    case DAY_MODE_NIGHT:
    default: return flintTr(FlintStr::Night);
  }
}

const char* flintTrWeatherCode(int code) {
  if (code == 0) return flintTr(FlintStr::Clear);
  if (code <= 3) return flintTr(FlintStr::Cloudy);
  if (code == 45 || code == 48) return flintTr(FlintStr::Fog);
  if (code >= 51 && code <= 57) return flintTr(FlintStr::Drizzle);
  if (code >= 61 && code <= 67) return flintTr(FlintStr::Rain);
  if (code >= 71 && code <= 77) return flintTr(FlintStr::Snow);
  if (code >= 80 && code <= 82) return flintTr(FlintStr::Showers);
  if (code >= 85 && code <= 86) return flintTr(FlintStr::Snow);
  if (code >= 95) return flintTr(FlintStr::Thunder);
  return flintTr(FlintStr::Weather);
}
