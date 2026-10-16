#include "time/date_fmt.h"
#include "host_check.h"
#include "i18n/i18n.h"

#include <cstdio>
#include <cstring>
#include <ctime>

static void expectEq(const char* got, const char* want, const char* what) {
  if (std::strcmp(got, want) != 0) {
    std::fprintf(stderr, "%s: got \"%s\" want \"%s\"\n", what, got, want);
    hostFail("string mismatch");
  }
}

static struct tm sampleDec31() {
  struct tm ti{};
  ti.tm_mday = 31;
  ti.tm_mon = 11;
  ti.tm_year = 2026 - 1900;
  ti.tm_wday = 4;  // Thursday
  return ti;
}

int main() {
  flintI18nSetLang(FLINT_I18N_EN);
  hostRequire(flintDateSepChar(SETTINGS_DATE_SEP_DOT) == '.', "sep dot");
  hostRequire(flintDateSepChar(SETTINGS_DATE_SEP_SLASH) == '/', "sep slash");
  hostRequire(flintDateSepChar(SETTINGS_DATE_SEP_DASH) == '-', "sep dash");

  const struct tm ti = sampleDec31();
  char buf[32];

  flintFormatDate(buf, sizeof(buf), ti, SETTINGS_DATE_DMY4, SETTINGS_DATE_SEP_DOT);
  expectEq(buf, "31.12.2026", "DMY4");

  flintFormatDate(buf, sizeof(buf), ti, SETTINGS_DATE_DMY2, SETTINGS_DATE_SEP_SLASH);
  expectEq(buf, "31/12/26", "DMY2/");

  flintFormatDate(buf, sizeof(buf), ti, SETTINGS_DATE_MDY4, SETTINGS_DATE_SEP_DASH);
  expectEq(buf, "12-31-2026", "MDY4-");

  flintFormatDate(buf, sizeof(buf), ti, SETTINGS_DATE_YMD4, SETTINGS_DATE_SEP_DOT);
  expectEq(buf, "2026.12.31", "YMD4");

  flintDateFormatSample(buf, sizeof(buf), SETTINGS_DATE_DMY4, SETTINGS_DATE_SEP_DOT);
  expectEq(buf, "31.12.2026", "sample");

  flintFormatDateLine(buf, sizeof(buf), false, ti, SETTINGS_DATE_DMY4, SETTINGS_DATE_SEP_DOT);
  expectEq(buf, "Waiting for time", "!hasTime");

  flintFormatDateLine(buf, sizeof(buf), true, ti, SETTINGS_DATE_DMY4, SETTINGS_DATE_SEP_DOT);
  expectEq(buf, "Thu  31.12.2026", "hasTime line");

  flintFormatDateLine(buf, sizeof(buf), true, ti, SETTINGS_DATE_NATURAL, SETTINGS_DATE_SEP_DOT);
  expectEq(buf, "Thu, 31 December", "natural English date");
  flintI18nSetLang(FLINT_I18N_RU);
  flintFormatDateLine(buf, sizeof(buf), true, ti, SETTINGS_DATE_NATURAL, SETTINGS_DATE_SEP_SLASH);
  expectEq(buf, "Чт, 31 декабря", "natural Russian date ignores separator");
  struct tm october = ti;
  october.tm_mon = 9;
  october.tm_mday = 17;
  october.tm_wday = 6;
  flintFormatDateLine(buf, sizeof(buf), true, october, SETTINGS_DATE_NATURAL, SETTINGS_DATE_SEP_DOT);
  expectEq(buf, "Сб, 17 октября", "requested date presentation");
  october.tm_mon = 12;
  flintFormatDate(buf, sizeof(buf), october, SETTINGS_DATE_NATURAL, SETTINGS_DATE_SEP_DOT);
  expectEq(buf, "", "invalid month rejected");
  std::puts("ok test_date_fmt");
  return 0;
}
