#include "shell/shell_settings.h"
#include "shell/settings_pages.h"
#include "usb/pc_link.h"

#include "display/display.h"
#include "display/ui_tokens.h"
#include "event/event.h"
#include "i18n/i18n.h"
#include "settings/settings.h"
#include "time/clock_time.h"
#include "time/date_fmt.h"
#include "time/tz_db.h"
#include "ui/chrome.h"
#include "ui/ui.h"

#include <stdio.h>

enum SettingsDirty : uint8_t {
  SET_OVL_NONE = 0,
  SET_OVL_FULL = 1,
  SET_OVL_ROWS = 2,
};

static SettingsPage gPage = SET_HOME;
static int gRow = 0;
static bool gLastConnected = false;
static bool gLastTimeValid = false;
static bool gLastPcSynced = false;
static bool gLastClockValid = false;
static struct tm gLastClock{};
static int gPrevRow = 0;
static int gStart = 0;
static SettingsDirty gDirty = SET_OVL_FULL;

void shellSettingsEnter() {
  gPage = SET_HOME;
  gRow = 0;
  gPrevRow = gRow;
  gStart = 0;
  gDirty = SET_OVL_FULL;
}

void shellSettingsNavPrev() {
  if (gDirty == SET_OVL_NONE) gPrevRow = gRow;
  gRow = (gRow + settingsPageRowCount(gPage) - 1) % settingsPageRowCount(gPage);
  if (gDirty != SET_OVL_FULL) gDirty = SET_OVL_ROWS;
}

void shellSettingsNavNext() {
  if (gDirty == SET_OVL_NONE) gPrevRow = gRow;
  gRow = (gRow + 1) % settingsPageRowCount(gPage);
  if (gDirty != SET_OVL_FULL) gDirty = SET_OVL_ROWS;
}

bool shellSettingsApply() {
  if (gPage == SET_HOME) {
    gPage = static_cast<SettingsPage>(gRow + 1);
    gRow = gPrevRow = gStart = 0;
    gDirty = SET_OVL_FULL;
    return false;
  }
  return gPage == SET_DISPLAY && settingsPageRow(gPage, gRow) == SET_ROW_FACE;
}

bool shellSettingsBack() {
  if (gPage == SET_HOME) return true;
  gRow = static_cast<int>(gPage) - 1;
  gPage = SET_HOME;
  gPrevRow = gRow;
  gStart = 0;
  gDirty = SET_OVL_FULL;
  return false;
}

static const char* pageTitle(SettingsPage page) {
  switch (page) {
    case SET_DISPLAY: return flintTr(FlintStr::Display);
    case SET_CLOCK: return flintTr(FlintStr::DateAndTime);
    case SET_CONNECTION: return flintTr(FlintStr::Connection);
    default: return flintTr(FlintStr::Settings);
  }
}

bool shellSettingsNeedsFull() {
  if (gPage == SET_CLOCK) {
    struct tm now{};
    const bool valid = clockTimeGetLocal(&now);
    if (valid != gLastClockValid ||
        (valid && (now.tm_year != gLastClock.tm_year || now.tm_yday != gLastClock.tm_yday ||
                   now.tm_hour != gLastClock.tm_hour || now.tm_min != gLastClock.tm_min))) {
      gDirty = SET_OVL_FULL;
    }
  }
  if (gPage == SET_CONNECTION &&
      (pcLinkConnected() != gLastConnected || clockTimeIsValid() != gLastTimeValid ||
       clockTimeIsPcSynced() != gLastPcSynced)) {
    gDirty = SET_OVL_FULL;
  }
  return gDirty == SET_OVL_FULL;
}

bool shellSettingsNeedsRows() {
  return gDirty == SET_OVL_ROWS;
}

static void markSettingsLayoutChanged() {
  flintEventEmit(FLINT_EVT_SETTINGS);
  // Clock preferences also change dependent time/date rows.
  if (gPage == SET_CLOCK) gDirty = SET_OVL_FULL;
}

static void markRowsUnlessFull() {
  if (gDirty != SET_OVL_FULL) {
    gDirty = SET_OVL_ROWS;
  }
}

static const char* showSecLabel(uint8_t mode) {
  switch (mode) {
    case SETTINGS_SEC_OFF:
      return flintTr(FlintStr::Off);
    case SETTINGS_SEC_ON:
      return flintTr(FlintStr::On);
    default:
      return flintTr(FlintStr::Auto);
  }
}

static const char* langLabel(uint8_t lang) {
  return lang == SETTINGS_LANG_EN ? "en_US" : "ru_RU";
}

static const char* hourLabel(uint8_t fmt) {
  return fmt == SETTINGS_HOUR_12 ? flintTr(FlintStr::N12h) : flintTr(FlintStr::N24h);
}

static void formatClockTimeValue(char* out, size_t outN) {
  struct tm ti{};
  if (!clockTimeGetLocal(&ti)) {
    snprintf(out, outN, "--:--");
    return;
  }
  if (settingsUse12Hour()) {
    int h = ti.tm_hour % 12;
    if (h == 0) {
      h = 12;
    }
    snprintf(out, outN, "%d:%02d %s", h, ti.tm_min,
             ti.tm_hour >= 12 ? flintTr(FlintStr::PM) : flintTr(FlintStr::AM));
  } else {
    snprintf(out, outN, "%02d:%02d", ti.tm_hour, ti.tm_min);
  }
}

static void formatClockDateValue(char* out, size_t outN) {
  struct tm ti{};
  if (!clockTimeGetLocal(&ti)) {
    snprintf(out, outN, "--");
    return;
  }
  flintFormatDate(out, outN, ti, settingsDateFormat(), settingsDateSep());
}

static void formatSettingsValue(const FaceContext& ctx, SettingsRow row,
                                const char** label, char* value, size_t size) {
  const FlintSettings& s = settingsGet();
  value[0] = '\0';
  switch (row) {
    case SET_ROW_FACE:
      *label = flintTr(FlintStr::Face);
      snprintf(value, size, "%s", s.activeFace);
      break;
    case SET_ROW_CLOCK_TIME:
      *label = flintTr(FlintStr::Time);
      formatClockTimeValue(value, size);
      break;
    case SET_ROW_CLOCK_DATE:
      *label = flintTr(FlintStr::SetDate);
      formatClockDateValue(value, size);
      break;
    case SET_ROW_SECONDS:
      *label = flintTr(FlintStr::Seconds);
      snprintf(value, size, "%s", showSecLabel(s.showSeconds));
      break;
    case SET_ROW_HOUR:
      *label = flintTr(FlintStr::Hour);
      snprintf(value, size, "%s", hourLabel(s.hourFormat));
      break;
    case SET_ROW_DATE:
      *label = flintTr(FlintStr::Date);
      flintDateFormatSample(value, size, s.dateFormat, s.dateSep);
      break;
    case SET_ROW_DATE_SEP:
      *label = flintTr(FlintStr::DateSep);
      if (s.dateFormat == SETTINGS_DATE_NATURAL) snprintf(value, size, "—");
      else snprintf(value, size, "%c", flintDateSepChar(s.dateSep));
      break;
    case SET_ROW_LANG:
      *label = flintTr(FlintStr::Lang);
      snprintf(value, size, "%s", langLabel(s.lang));
      break;
    case SET_ROW_TZ:
      *label = flintTr(FlintStr::Timezone);
      snprintf(value, size, "%s", tzDbFind(s.tzId)->label);
      break;
    case SET_ROW_USB:
      *label = "USB";
      snprintf(value, size, "%s", flintTr(ctx.usbConnected ? FlintStr::Connected : FlintStr::Offline));
      break;
    case SET_ROW_CLOCK_STATUS:
      *label = flintTr(FlintStr::ClockStatus);
      snprintf(value, size, "%s", flintTr(clockTimeIsPcSynced() ? FlintStr::SyncedWithPC :
                             ctx.hasTime ? FlintStr::Ready : FlintStr::NotSet));
      break;
  }
}

static int windowStart() {
  const int vis = UiTok::kShellVisibleRows;
  int start = gRow - vis / 2;
  if (start < 0) {
    start = 0;
  }
  if (start > settingsPageRowCount(gPage) - vis) {
    start = settingsPageRowCount(gPage) - vis;
  }
  if (start < 0) {
    start = 0;
  }
  return start;
}

static void paintScroll(const FaceContext& ctx) {
  if (settingsPageRowCount(gPage) <= UiTok::kShellVisibleRows) return;
  const int trackH = UiTok::kShellVisibleRows * UiTok::kShellRowH;
  const int trackY = UiTok::kShellRowsTop;
  int thumbH = settingsPageRowCount(gPage) > 0 ? (trackH * UiTok::kShellVisibleRows) / settingsPageRowCount(gPage) : trackH;
  if (thumbH > trackH) {
    thumbH = trackH;
  }
  const int maxStart = settingsPageRowCount(gPage) - UiTok::kShellVisibleRows;
  const int thumbY =
      trackY + (maxStart > 0 ? (gStart * (trackH - thumbH)) / maxStart : 0);
  chromeScrollTrack(ctx, trackY, trackH, thumbY, thumbH);
}

static void paintRow(const FaceContext& ctx, int row, int visIndex, uint8_t fade) {
  const int y = UiTok::shellRowFillY(visIndex);
  if (gPage == SET_HOME) {
    chromeValueRow(ctx, y, UiTok::kShellRowH,
                    pageTitle(static_cast<SettingsPage>(row + 1)), "→", row == gRow, fade);
    return;
  }
  const char* label = "";
  char value[64];
  formatSettingsValue(ctx, settingsPageRow(gPage, row), &label, value, sizeof(value));
  chromeValueRow(ctx, y, UiTok::kShellRowH, label, value, row == gRow, fade);
}

void shellSettingsPaint(uint8_t fade) {
  if (!displayHasCanvas()) {
    return;
  }
  const FaceContext ctx = uiBuildContext();
  chromeFillBackdrop(ctx);
  chromeHeader(ctx, pageTitle(gPage), fade);
  const char* hint = gPage == SET_HOME ? flintTr(FlintStr::PNEnterEsc)
                   : gPage == SET_CONNECTION ? "Esc" : flintTr(FlintStr::PNPlusEnterEsc);
  chromeFooter(ctx, hint, fade);
  if (gPage == SET_CLOCK) {
    gLastClockValid = clockTimeGetLocal(&gLastClock);
  }
  gStart = windowStart();
  for (int i = 0; i < UiTok::kShellVisibleRows && gStart + i < settingsPageRowCount(gPage); i++) {
    paintRow(ctx, gStart + i, i, fade);
  }
  paintScroll(ctx);
  displayPresent();
  gLastConnected = ctx.usbConnected;
  gLastTimeValid = ctx.hasTime;
  gLastPcSynced = clockTimeIsPcSynced();
  gPrevRow = gRow;
  gDirty = SET_OVL_NONE;
}

void shellSettingsPaintPartial() {
  if (!displayHasCanvas()) {
    return;
  }
  const FaceContext ctx = uiBuildContext();
  const int newStart = windowStart();
  if (newStart != gStart) {
    gStart = newStart;
    shellSettingsPaint(255);
    return;
  }

  DisplayRect rects[4];
  int n = 0;

  auto addRow = [&](int row) {
    const int vis = row - gStart;
    if (vis < 0 || vis >= UiTok::kShellVisibleRows) {
      return;
    }
    const DisplayRect band =
        chromeListRowRect(ctx, UiTok::shellRowFillY(vis), UiTok::kShellRowH);
    displaySetClipRect(band.x, band.y, band.w, band.h);
    paintRow(ctx, row, vis, 255);
    rects[n++] = band;
  };

  if (gPrevRow != gRow) {
    addRow(gPrevRow);
    addRow(gRow);
  } else {
    addRow(gRow);
  }

  {
    const DisplayRect scrollBand{
        static_cast<int16_t>(UiTok::contentRight(ctx.screenW) - UiTok::kShellScrollW - 1),
        static_cast<int16_t>(UiTok::kShellRowsTop),
        static_cast<int16_t>(UiTok::kShellScrollW + 2),
        static_cast<int16_t>(UiTok::kShellVisibleRows * UiTok::kShellRowH)};
    displaySetClipRect(scrollBand.x, scrollBand.y, scrollBand.w, scrollBand.h);
    paintScroll(ctx);
    rects[n++] = scrollBand;
  }
  displayClearClipRect();

  displayPresentRects(rects, n);
  gLastConnected = ctx.usbConnected;
  gLastTimeValid = ctx.hasTime;
  gLastPcSynced = clockTimeIsPcSynced();
  gPrevRow = gRow;
  gDirty = SET_OVL_NONE;
}

static void cycleShowSeconds() {
  const FlintSettings& s = settingsGet();
  uint8_t next = SETTINGS_SEC_AUTO;
  if (s.showSeconds == SETTINGS_SEC_AUTO) {
    next = SETTINGS_SEC_ON;
  } else if (s.showSeconds == SETTINGS_SEC_ON) {
    next = SETTINGS_SEC_OFF;
  }
  settingsSetShowSeconds(next);
  markSettingsLayoutChanged();
}

static void cycleLang() {
  const FlintSettings& s = settingsGet();
  settingsSetLang(s.lang == SETTINGS_LANG_RU ? SETTINGS_LANG_EN : SETTINGS_LANG_RU);
  markSettingsLayoutChanged();
  gDirty = SET_OVL_FULL;
}

static void cycleHourFormat() {
  const FlintSettings& s = settingsGet();
  settingsSetHourFormat(s.hourFormat == SETTINGS_HOUR_24 ? SETTINGS_HOUR_12 : SETTINGS_HOUR_24);
  markSettingsLayoutChanged();
}

static void cycleDateFormat(int dir) {
  const FlintSettings& s = settingsGet();
  int next = static_cast<int>(s.dateFormat) + (dir > 0 ? 1 : -1);
  if (next < 0) {
    next = SETTINGS_DATE_FORMAT_COUNT - 1;
  } else if (next >= SETTINGS_DATE_FORMAT_COUNT) {
    next = 0;
  }
  settingsSetDateFormat(static_cast<uint8_t>(next));
  markSettingsLayoutChanged();
}

static void cycleDateSep(int dir) {
  const FlintSettings& s = settingsGet();
  int next = static_cast<int>(s.dateSep) + (dir > 0 ? 1 : -1);
  if (next < 0) {
    next = SETTINGS_DATE_SEP_COUNT - 1;
  } else if (next >= SETTINGS_DATE_SEP_COUNT) {
    next = 0;
  }
  settingsSetDateSep(static_cast<uint8_t>(next));
  markSettingsLayoutChanged();
}

static void cycleTz(int dir) {
  int idx = tzDbIndexOf(settingsGet().tzId);
  if (idx < 0) {
    idx = 0;
  }
  const int n = tzDbCount();
  idx = (idx + (dir > 0 ? 1 : n - 1)) % n;
  const TzEntry* e = tzDbAt(idx);
  if (e) {
    settingsSetTzId(e->id);
    markSettingsLayoutChanged();
  }
}

static void adjustClockTime(int dir) {
  const bool wasValid = clockTimeIsValid();
  if (!clockTimeAdjustMinutes(dir > 0 ? 1 : -1)) {
    return;
  }
  if (!wasValid) {
    flintEventEmit(FLINT_EVT_TIME_VALID);
  }
  flintEventEmit(FLINT_EVT_MINUTE);
}

static void adjustClockDate(int dir) {
  const bool wasValid = clockTimeIsValid();
  if (!clockTimeAdjustDays(dir > 0 ? 1 : -1)) {
    return;
  }
  if (!wasValid) {
    flintEventEmit(FLINT_EVT_TIME_VALID);
  }
  flintEventEmit(FLINT_EVT_MINUTE);
}

void shellSettingsAdjust(int dir) {
  if (gPage == SET_HOME || gPage == SET_CONNECTION) return;
  switch (settingsPageRow(gPage, gRow)) {
    case SET_ROW_CLOCK_TIME:
      adjustClockTime(dir);
      gDirty = SET_OVL_FULL;
      break;
    case SET_ROW_CLOCK_DATE:
      adjustClockDate(dir);
      gDirty = SET_OVL_FULL;
      break;
    case SET_ROW_SECONDS:
      cycleShowSeconds();
      break;
    case SET_ROW_HOUR:
      cycleHourFormat();
      break;
    case SET_ROW_DATE:
      cycleDateFormat(dir);
      gDirty = SET_OVL_FULL;
      break;
    case SET_ROW_DATE_SEP:
      if (settingsGet().dateFormat != SETTINGS_DATE_NATURAL) cycleDateSep(dir);
      break;
    case SET_ROW_LANG:
      cycleLang();
      break;
    case SET_ROW_TZ:
      cycleTz(dir);
      break;
    case SET_ROW_FACE:
    default:
      break;
  }
  markRowsUnlessFull();
}
