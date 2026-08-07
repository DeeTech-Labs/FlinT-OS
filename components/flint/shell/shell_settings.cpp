#include "shell/shell_settings.h"

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

enum SettingsRow : uint8_t {
  SET_ROW_FACE = 0,
  SET_ROW_CLOCK_TIME,
  SET_ROW_CLOCK_DATE,
  SET_ROW_SECONDS,
  SET_ROW_HOUR,
  SET_ROW_DATE,
  SET_ROW_DATE_SEP,
  SET_ROW_LANG,
  SET_ROW_TZ,
  SET_ROW_COUNT,
};

enum SettingsDirty : uint8_t {
  SET_OVL_NONE = 0,
  SET_OVL_FULL = 1,
  SET_OVL_ROWS = 2,
};

static int gRow = 0;
static int gPrevRow = 0;
static int gStart = 0;
static SettingsDirty gDirty = SET_OVL_FULL;

void shellSettingsEnter() {
  gRow = SET_ROW_SECONDS;
  gPrevRow = gRow;
  gStart = 0;
  gDirty = SET_OVL_FULL;
}

void shellSettingsNavPrev() {
  gPrevRow = gRow;
  gRow = (gRow + SET_ROW_COUNT - 1) % SET_ROW_COUNT;
  gDirty = SET_OVL_ROWS;
}

void shellSettingsNavNext() {
  gPrevRow = gRow;
  gRow = (gRow + 1) % SET_ROW_COUNT;
  gDirty = SET_OVL_ROWS;
}

bool shellSettingsApply() {
  return gRow == SET_ROW_FACE;
}

bool shellSettingsNeedsFull() {
  return gDirty == SET_OVL_FULL;
}

bool shellSettingsNeedsRows() {
  return gDirty == SET_OVL_ROWS;
}

static void markSettingsLayoutChanged() {
  flintEventEmit(FLINT_EVT_SETTINGS);
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

static void formatSettingsLine(char* out, size_t outN, int row) {
  const FlintSettings& s = settingsGet();
  switch (row) {
    case SET_ROW_FACE:
      snprintf(out, outN, "%s   %.16s", flintTr(FlintStr::Face), s.activeFace);
      break;
    case SET_ROW_CLOCK_TIME: {
      char val[16];
      formatClockTimeValue(val, sizeof(val));
      snprintf(out, outN, "%s   %s", flintTr(FlintStr::Time), val);
      break;
    }
    case SET_ROW_CLOCK_DATE: {
      char val[16];
      formatClockDateValue(val, sizeof(val));
      snprintf(out, outN, "%s  %s", flintTr(FlintStr::SetDate), val);
      break;
    }
    case SET_ROW_SECONDS:
      snprintf(out, outN, "%s  %s", flintTr(FlintStr::Seconds), showSecLabel(s.showSeconds));
      break;
    case SET_ROW_HOUR:
      snprintf(out, outN, "%s     %s", flintTr(FlintStr::Hour), hourLabel(s.hourFormat));
      break;
    case SET_ROW_DATE: {
      char sample[16];
      flintDateFormatSample(sample, sizeof(sample), s.dateFormat, s.dateSep);
      snprintf(out, outN, "%s   %s", flintTr(FlintStr::Date), sample);
      break;
    }
    case SET_ROW_DATE_SEP: {
      char sep[4];
      snprintf(sep, sizeof(sep), "%c", flintDateSepChar(s.dateSep));
      snprintf(out, outN, "%s    %s", flintTr(FlintStr::DateSep), sep);
      break;
    }
    case SET_ROW_LANG:
      snprintf(out, outN, "%s     %s", flintTr(FlintStr::Lang), langLabel(s.lang));
      break;
    case SET_ROW_TZ: {
      const TzEntry* e = tzDbFind(s.tzId);
      snprintf(out, outN, "%s    %.14s", flintTr(FlintStr::Timezone), e->label);
      break;
    }
    default:
      out[0] = '\0';
      break;
  }
}

static int windowStart() {
  const int vis = UiTok::kShellVisibleRows;
  int start = gRow - vis / 2;
  if (start < 0) {
    start = 0;
  }
  if (start > SET_ROW_COUNT - vis) {
    start = SET_ROW_COUNT - vis;
  }
  if (start < 0) {
    start = 0;
  }
  return start;
}

static void paintScroll(const FaceContext& ctx) {
  const int trackH = UiTok::kShellVisibleRows * UiTok::kShellRowH;
  const int trackY = UiTok::kShellRowsTop;
  int thumbH = SET_ROW_COUNT > 0 ? (trackH * UiTok::kShellVisibleRows) / SET_ROW_COUNT : trackH;
  if (thumbH > trackH) {
    thumbH = trackH;
  }
  const int maxStart = SET_ROW_COUNT - UiTok::kShellVisibleRows;
  const int thumbY =
      trackY + (maxStart > 0 ? (gStart * (trackH - thumbH)) / maxStart : 0);
  chromeScrollTrack(ctx, trackY, trackH, thumbY, thumbH);
}

static void paintRow(const FaceContext& ctx, int row, int visIndex, uint8_t fade) {
  char line[48];
  formatSettingsLine(line, sizeof(line), row);
  chromeListRow(ctx, UiTok::shellRowFillY(visIndex), UiTok::kShellRowH, line, row == gRow, fade);
}

void shellSettingsPaint(uint8_t fade) {
  if (!displayHasCanvas()) {
    return;
  }
  const FaceContext ctx = uiBuildContext();
  chromeFillBackdrop(ctx);
  chromeHeader(ctx, flintTr(FlintStr::Settings), fade);
  chromeFooter(ctx, flintTr(FlintStr::PNPlusEnterEsc), fade);
  chromeCounter(ctx, gRow + 1, SET_ROW_COUNT, fade);
  gStart = windowStart();
  for (int i = 0; i < UiTok::kShellVisibleRows && gStart + i < SET_ROW_COUNT; i++) {
    paintRow(ctx, gStart + i, i, fade);
  }
  paintScroll(ctx);
  displayPresent();
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
    const DisplayRect counterBand{static_cast<int16_t>(UiTok::contentRight(ctx.screenW) - 48),
                                  static_cast<int16_t>(UiTok::contentTop() - 2), 52, 22};
    displaySetClipRect(counterBand.x, counterBand.y, counterBand.w, counterBand.h);
    chromeCounter(ctx, gRow + 1, SET_ROW_COUNT, 255);
    rects[n++] = counterBand;
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
  switch (gRow) {
    case SET_ROW_CLOCK_TIME:
      adjustClockTime(dir);
      break;
    case SET_ROW_CLOCK_DATE:
      adjustClockDate(dir);
      break;
    case SET_ROW_SECONDS:
      cycleShowSeconds();
      break;
    case SET_ROW_HOUR:
      cycleHourFormat();
      break;
    case SET_ROW_DATE:
      cycleDateFormat(dir);
      break;
    case SET_ROW_DATE_SEP:
      cycleDateSep(dir);
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
