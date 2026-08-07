#include "settings/settings_clamp.h"
#include "host_check.h"

#include <cstdio>

int main() {
  hostRequire(settingsClampHourFormat(SETTINGS_HOUR_12) == SETTINGS_HOUR_12, "hour 12");
  hostRequire(settingsClampHourFormat(SETTINGS_HOUR_24) == SETTINGS_HOUR_24, "hour 24");
  hostRequire(settingsClampHourFormat(99) == SETTINGS_HOUR_24, "hour invalid→24");

  hostRequire(settingsClampDateFormat(SETTINGS_DATE_YMD4) == SETTINGS_DATE_YMD4, "date ok");
  hostRequire(settingsClampDateFormat(SETTINGS_DATE_FORMAT_COUNT) == SETTINGS_DATE_DMY4,
              "date overflow→DMY4");

  hostRequire(settingsClampDateSep(SETTINGS_DATE_SEP_DASH) == SETTINGS_DATE_SEP_DASH, "sep ok");
  hostRequire(settingsClampDateSep(SETTINGS_DATE_SEP_COUNT) == SETTINGS_DATE_SEP_DOT,
              "sep overflow→dot");

  hostRequire(settingsClampLang(SETTINGS_LANG_EN) == SETTINGS_LANG_EN, "lang en");
  hostRequire(settingsClampLang(42) == SETTINGS_LANG_RU, "lang invalid→ru");

  hostRequire(settingsClampShowSeconds(SETTINGS_SEC_ON) == SETTINGS_SEC_ON, "sec on");
  hostRequire(settingsClampShowSeconds(SETTINGS_SEC_AUTO) == SETTINGS_SEC_AUTO, "sec auto");
  hostRequire(settingsClampShowSeconds(7) == SETTINGS_SEC_AUTO, "sec invalid→auto");

  std::puts("ok test_settings_clamp");
  return 0;
}
