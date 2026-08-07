#include "time/date_fmt.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>

static void expectEq(const char* got, const char* want, const char* what) {
  if (std::strcmp(got, want) != 0) {
    std::fprintf(stderr, "%s: got \"%s\" want \"%s\"\n", what, got, want);
    assert(false);
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
  assert(flintDateSepChar(SETTINGS_DATE_SEP_DOT) == '.');
  assert(flintDateSepChar(SETTINGS_DATE_SEP_SLASH) == '/');
  assert(flintDateSepChar(SETTINGS_DATE_SEP_DASH) == '-');

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

  std::puts("ok test_date_fmt");
  return 0;
}
