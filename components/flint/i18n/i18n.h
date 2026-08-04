#pragma once

#include <stdint.h>

#include "i18n/i18n_catalog.inc"

// Runtime localization (msgid catalogs compiled from .po).
// Authoring: components/flint/i18n/lang/{en_US,ru_RU}/flint.po + flint.pot
// Regen:     python3 tools/gen_i18n_catalog.py

enum FlintI18nLang : uint8_t {
  FLINT_I18N_RU = 0,  // ru_RU — matches SettingsLang
  FLINT_I18N_EN = 1,  // en_US
};

void flintI18nSetLang(uint8_t lang);
uint8_t flintI18nLang();
const char* flintI18nLocaleId();  // "ru_RU" / "en_US"

const char* flintTr(FlintStr id);

// Convenience for weekday 0=Sun .. 6=Sat
const char* flintTrWeekday(int wday);

// Day-mode / weather labels
const char* flintTrDayMode(uint8_t dayMode /* DayMode */);
const char* flintTrWeatherCode(int code);
