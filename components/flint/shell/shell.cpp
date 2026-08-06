#include "shell/shell.h"

#include "anim/anim.h"
#include "config/debug.h"
#include "display/display.h"
#include "display/ui_tokens.h"
#include "event/event.h"
#include "faces/FaceRegistry.h"
#include "i18n/i18n.h"
#include "sdk/resources.h"
#include "settings/settings.h"
#include "time/tz_db.h"
#include "time/date_fmt.h"
#include "ui/chrome.h"
#include "ui/ui.h"
#include "ui/ui_dirty.h"
#include "util/flint_sys.h"
#include "weather/weather.h"

#include <stdio.h>
#include <string.h>

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_err.h"

enum ShellState : uint8_t {
  SHELL_FACE = 0,
  SHELL_PICKER,
  SHELL_SETTINGS,
};

enum SettingsRow : uint8_t {
  SET_ROW_FACE = 0,
  SET_ROW_SECONDS,
  SET_ROW_HOUR,
  SET_ROW_DATE,
  SET_ROW_DATE_SEP,
  SET_ROW_LANG,
  SET_ROW_TZ,
  SET_ROW_LAT,
  SET_ROW_LON,
  SET_ROW_COUNT,
};

enum OverlayDirty : uint8_t {
  OVL_NONE = 0,
  OVL_FULL = 1,
  OVL_ROWS = 2,
};

static ShellState gState = SHELL_FACE;
static int gPickerIndex = 0;
static int gSettingsRow = 0;
static int gSettingsStart = 0;
static OverlayDirty gOverlayDirty = OVL_FULL;
static int gPrevPickerIndex = 0;
static int gPrevSettingsRow = 0;
static bool gConsoleReady = false;

static void shellLogHelp() {
  LOGI("shell", "keys: Enter=picker/action  s=settings  p/n=nav  +/- adjust  q/Esc=back  h=help");
}

static void shellConsoleInit() {
  if (gConsoleReady) {
    return;
  }

  setvbuf(stdin, NULL, _IONBF, 0);
  usb_serial_jtag_vfs_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
  usb_serial_jtag_vfs_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

  usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
  const esp_err_t err = usb_serial_jtag_driver_install(&cfg);
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    LOGE("shell", "usb_serial_jtag_driver_install: %s", esp_err_to_name(err));
    return;
  }

  usb_serial_jtag_vfs_use_driver();
  gConsoleReady = true;
}

static void enterFace() {
  gState = SHELL_FACE;
  animBegin(ANIM_NONE);
  uiFaceActivateCurrent();
  flintEventEmit(FLINT_EVT_FORCE_REDRAW);
  gOverlayDirty = OVL_NONE;
}

static void enterPicker() {
  gState = SHELL_PICKER;
  gPickerIndex = 0;
  const char* cur = FaceRegistry::activeId();
  for (int i = 0; i < FaceRegistry::count(); i++) {
    const FaceInfo* info = FaceRegistry::infoAt(i);
    if (info && cur && strcmp(info->id, cur) == 0) {
      gPickerIndex = i;
      break;
    }
  }
  gPrevPickerIndex = gPickerIndex;
  gOverlayDirty = OVL_FULL;
  animBegin(ANIM_OVERLAY_IN);
}

static void enterSettings() {
  gState = SHELL_SETTINGS;
  gSettingsRow = SET_ROW_SECONDS;
  gPrevSettingsRow = gSettingsRow;
  gSettingsStart = 0;
  gOverlayDirty = OVL_FULL;
  animBegin(ANIM_OVERLAY_IN);
}

static void goTo(ShellState next) {
  if (next == gState) {
    return;
  }
  switch (next) {
    case SHELL_FACE:
      enterFace();
      break;
    case SHELL_PICKER:
      enterPicker();
      break;
    case SHELL_SETTINGS:
      enterSettings();
      break;
  }
}

static const char* showSecLabel(uint8_t mode) {
  switch (mode) {
    case SETTINGS_SEC_OFF: return flintTr(FlintStr::Off);
    case SETTINGS_SEC_ON: return flintTr(FlintStr::On);
    default: return flintTr(FlintStr::Auto);
  }
}

static const char* langLabel(uint8_t lang) {
  return lang == SETTINGS_LANG_EN ? "en_US" : "ru_RU";
}

static const char* hourLabel(uint8_t fmt) {
  return fmt == SETTINGS_HOUR_12 ? flintTr(FlintStr::N12h) : flintTr(FlintStr::N24h);
}

static void dateFormatLabel(char* out, size_t outN, uint8_t fmt, uint8_t sep) {
  flintDateFormatSample(out, outN, fmt, sep);
}

static void dateSepLabel(char* out, size_t outN, uint8_t sep) {
  snprintf(out, outN, "%c", flintDateSepChar(sep));
}

static void formatSettingsLine(char* out, size_t outN, int row) {
  const FlintSettings& s = settingsGet();
  switch (row) {
    case SET_ROW_FACE:
      snprintf(out, outN, "%s   %.16s", flintTr(FlintStr::Face), s.activeFace);
      break;
    case SET_ROW_SECONDS:
      snprintf(out, outN, "%s  %s", flintTr(FlintStr::Seconds), showSecLabel(s.showSeconds));
      break;
    case SET_ROW_HOUR:
      snprintf(out, outN, "%s     %s", flintTr(FlintStr::Hour), hourLabel(s.hourFormat));
      break;
    case SET_ROW_DATE: {
      char sample[16];
      dateFormatLabel(sample, sizeof(sample), s.dateFormat, s.dateSep);
      snprintf(out, outN, "%s   %s", flintTr(FlintStr::Date), sample);
      break;
    }
    case SET_ROW_DATE_SEP: {
      char sep[4];
      dateSepLabel(sep, sizeof(sep), s.dateSep);
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
    case SET_ROW_LAT:
      snprintf(out, outN, "%s   %+.2f", flintTr(FlintStr::Lat),
               static_cast<double>(s.wxLat));
      break;
    case SET_ROW_LON:
      snprintf(out, outN, "%s   %+.2f", flintTr(FlintStr::Lon),
               static_cast<double>(s.wxLon));
      break;
    default:
      out[0] = '\0';
      break;
  }
}

static int settingsWindowStart() {
  const int vis = UiTok::kShellVisibleRows;
  int start = gSettingsRow - vis / 2;
  if (start < 0) start = 0;
  if (start > SET_ROW_COUNT - vis) start = SET_ROW_COUNT - vis;
  if (start < 0) start = 0;
  return start;
}

static void paintSettingsScroll(const FaceContext& ctx) {
  const int trackH = UiTok::kShellVisibleRows * UiTok::kShellRowH;
  const int trackY = UiTok::kShellRowsTop;
  int thumbH = SET_ROW_COUNT > 0 ? (trackH * UiTok::kShellVisibleRows) / SET_ROW_COUNT : trackH;
  if (thumbH > trackH) thumbH = trackH;
  const int maxStart = SET_ROW_COUNT - UiTok::kShellVisibleRows;
  const int thumbY =
      trackY + (maxStart > 0 ? (gSettingsStart * (trackH - thumbH)) / maxStart : 0);
  chromeScrollTrack(ctx, trackY, trackH, thumbY, thumbH);
}

static void paintSettingsRow(const FaceContext& ctx, int row, int visIndex, uint8_t fade) {
  char line[48];
  formatSettingsLine(line, sizeof(line), row);
  chromeListRow(ctx, UiTok::shellRowFillY(visIndex), UiTok::kShellRowH, line,
                row == gSettingsRow, fade);
}

static void drawSettingsFull(uint8_t fade) {
  if (!displayHasCanvas()) {
    return;
  }
  const FaceContext ctx = uiBuildContext();
  chromeFillBackdrop(ctx);
  chromeHeader(ctx, flintTr(FlintStr::Settings), fade);
  chromeFooter(ctx, flintTr(FlintStr::PNPlusEnterEsc), fade);
  chromeCounter(ctx, gSettingsRow + 1, SET_ROW_COUNT, fade);
  gSettingsStart = settingsWindowStart();
  for (int i = 0; i < UiTok::kShellVisibleRows && gSettingsStart + i < SET_ROW_COUNT; i++) {
    paintSettingsRow(ctx, gSettingsStart + i, i, fade);
  }
  paintSettingsScroll(ctx);
  displayPresent();
  gPrevSettingsRow = gSettingsRow;
  gOverlayDirty = OVL_NONE;
}

static void drawSettingsPartial() {
  if (!displayHasCanvas()) {
    return;
  }
  const FaceContext ctx = uiBuildContext();
  const int newStart = settingsWindowStart();
  if (newStart != gSettingsStart) {
    gSettingsStart = newStart;
    drawSettingsFull(255);
    return;
  }

  DisplayRect rects[4];
  int n = 0;

  auto addRow = [&](int row) {
    const int vis = row - gSettingsStart;
    if (vis < 0 || vis >= UiTok::kShellVisibleRows) {
      return;
    }
    const DisplayRect band = chromeListRowRect(ctx, UiTok::shellRowFillY(vis), UiTok::kShellRowH);
    displaySetClipRect(band.x, band.y, band.w, band.h);
    paintSettingsRow(ctx, row, vis, 255);
    rects[n++] = band;
  };

  if (gPrevSettingsRow != gSettingsRow) {
    addRow(gPrevSettingsRow);
    addRow(gSettingsRow);
  } else {
    addRow(gSettingsRow);
  }

  {
    const DisplayRect counterBand{static_cast<int16_t>(UiTok::contentRight(ctx.screenW) - 48),
                                  static_cast<int16_t>(UiTok::contentTop() - 2), 52, 22};
    displaySetClipRect(counterBand.x, counterBand.y, counterBand.w, counterBand.h);
    chromeCounter(ctx, gSettingsRow + 1, SET_ROW_COUNT, 255);
    rects[n++] = counterBand;
  }
  {
    const DisplayRect scrollBand{
        static_cast<int16_t>(UiTok::contentRight(ctx.screenW) - UiTok::kShellScrollW - 1),
        static_cast<int16_t>(UiTok::kShellRowsTop),
        static_cast<int16_t>(UiTok::kShellScrollW + 2),
        static_cast<int16_t>(UiTok::kShellVisibleRows * UiTok::kShellRowH)};
    displaySetClipRect(scrollBand.x, scrollBand.y, scrollBand.w, scrollBand.h);
    paintSettingsScroll(ctx);
    rects[n++] = scrollBand;
  }
  displayClearClipRect();

  displayPresentRects(rects, n);
  gPrevSettingsRow = gSettingsRow;
  gOverlayDirty = OVL_NONE;
}

static void paintPickerRow(const FaceContext& ctx, int index, uint8_t fade) {
  const FaceInfo* info = FaceRegistry::infoAt(index);
  if (!info) {
    return;
  }
  char line[64];
  snprintf(line, sizeof(line), "%.20s  (%.16s)", info->name ? info->name : "?",
           info->id ? info->id : "?");
  chromeListRow(ctx, UiTok::shellRowFillY(index), UiTok::kShellRowH, line,
                index == gPickerIndex, fade);
}

static void drawPickerFull(uint8_t fade) {
  if (!displayHasCanvas()) {
    return;
  }
  const FaceContext ctx = uiBuildContext();
  chromeFillBackdrop(ctx);
  chromeHeader(ctx, flintTr(FlintStr::Faces), fade);
  chromeFooter(ctx, flintTr(FlintStr::PNEnterEsc), fade);
  const int n = FaceRegistry::count();
  for (int i = 0; i < n; i++) {
    paintPickerRow(ctx, i, fade);
  }
  displayPresent();
  gPrevPickerIndex = gPickerIndex;
  gOverlayDirty = OVL_NONE;
}

static void drawPickerPartial() {
  if (!displayHasCanvas()) {
    return;
  }
  const FaceContext ctx = uiBuildContext();
  DisplayRect rects[2];
  int n = 0;
  auto add = [&](int index) {
    if (index < 0 || index >= FaceRegistry::count()) {
      return;
    }
    const DisplayRect band = chromeListRowRect(ctx, UiTok::shellRowFillY(index), UiTok::kShellRowH);
    displaySetClipRect(band.x, band.y, band.w, band.h);
    paintPickerRow(ctx, index, 255);
    rects[n++] = band;
  };
  if (gPrevPickerIndex != gPickerIndex) {
    add(gPrevPickerIndex);
  }
  add(gPickerIndex);
  displayClearClipRect();
  displayPresentRects(rects, n);
  gPrevPickerIndex = gPickerIndex;
  gOverlayDirty = OVL_NONE;
}

static void markSettingsLayoutChanged() {
  flintEventEmit(FLINT_EVT_SETTINGS);
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
  flintEventEmit(FLINT_EVT_FORCE_REDRAW);
}

static void cycleLang() {
  const FlintSettings& s = settingsGet();
  settingsSetLang(s.lang == SETTINGS_LANG_RU ? SETTINGS_LANG_EN : SETTINGS_LANG_RU);
  markSettingsLayoutChanged();
  flintEventEmit(FLINT_EVT_FORCE_REDRAW);
  gOverlayDirty = OVL_FULL;
}

static void cycleHourFormat() {
  const FlintSettings& s = settingsGet();
  settingsSetHourFormat(s.hourFormat == SETTINGS_HOUR_24 ? SETTINGS_HOUR_12 : SETTINGS_HOUR_24);
  markSettingsLayoutChanged();
  flintEventEmit(FLINT_EVT_FORCE_REDRAW);
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
  flintEventEmit(FLINT_EVT_FORCE_REDRAW);
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
  flintEventEmit(FLINT_EVT_FORCE_REDRAW);
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
    weatherRequestRefresh();
    markSettingsLayoutChanged();
    flintEventEmit(FLINT_EVT_FORCE_REDRAW);
    flintEventEmit(FLINT_EVT_WEATHER);
  }
}

static void nudgeCoord(bool lat, float delta) {
  const FlintSettings& s = settingsGet();
  if (lat) {
    settingsSetWeatherCoords(s.wxLat + delta, s.wxLon);
  } else {
    settingsSetWeatherCoords(s.wxLat, s.wxLon + delta);
  }
  weatherRequestRefresh();
  flintEventEmit(FLINT_EVT_SETTINGS);
  flintEventEmit(FLINT_EVT_WEATHER);
}

static void adjustSelected(int dir) {
  switch (gSettingsRow) {
    case SET_ROW_LAT:
      nudgeCoord(true, dir > 0 ? 0.01f : -0.01f);
      break;
    case SET_ROW_LON:
      nudgeCoord(false, dir > 0 ? 0.01f : -0.01f);
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
  if (gOverlayDirty != OVL_FULL) {
    gOverlayDirty = OVL_ROWS;
  }
}

static void applySettingsRow() {
  if (gSettingsRow == SET_ROW_FACE) {
    goTo(SHELL_PICKER);
  }
}

void shellBegin() {
  shellConsoleInit();
  settingsLoad();
  if (!displayHasCanvas()) {
    LOGE("shell", "no canvas — shell idle");
    return;
  }
  uiFaceBegin(settingsGet().activeFace);
  enterFace();
  shellLogHelp();
  LOGI("shell", "state=FACE");
}

void shellTick() {
  if (!displayHasCanvas()) {
    return;
  }

  switch (gState) {
    case SHELL_FACE: {
      // Colon pulse only while seconds are visible — skip always-on motion dirty
      // when the face is minute-driven.
      static uint32_t lastMotionMs = 0;
      const uint32_t now = flintMillis();
      if (uiWantsSeconds() && (now - lastMotionMs >= UiTok::kColonPulseTickMs)) {
        uiMarkDirty(UI_DIRTY_MOTION);
        lastMotionMs = now;
      }
      uiFaceTick();
      break;
    }

    case SHELL_PICKER:
      if (animIsActive()) {
        drawPickerFull(animEasedProgress());
        animTick();
      } else if (gOverlayDirty == OVL_FULL) {
        drawPickerFull(255);
      } else if (gOverlayDirty == OVL_ROWS) {
        drawPickerPartial();
      }
      break;

    case SHELL_SETTINGS:
      if (animIsActive()) {
        drawSettingsFull(animEasedProgress());
        animTick();
      } else if (gOverlayDirty == OVL_FULL) {
        drawSettingsFull(255);
      } else if (gOverlayDirty == OVL_ROWS) {
        drawSettingsPartial();
      }
      break;
  }
}

void shellHandleKey(char c) {
  if (c == 'h' || c == 'H' || c == '?') {
    shellLogHelp();
    return;
  }

  if (c == '\n') {
    c = '\r';
  }

  if (!displayHasCanvas()) {
    return;
  }

  // Ignore nav during overlay enter animation.
  if (animIsActive() && gState != SHELL_FACE) {
    return;
  }

  switch (gState) {
    case SHELL_FACE:
      if (c == '\r') {
        goTo(SHELL_PICKER);
        LOGI("shell", "state=PICKER");
      } else if (c == 's' || c == 'S') {
        goTo(SHELL_SETTINGS);
        LOGI("shell", "state=SETTINGS");
      }
      break;

    case SHELL_PICKER:
      if (c == 'q' || c == 'Q' || c == 27) {
        goTo(SHELL_FACE);
        LOGI("shell", "state=FACE (cancel)");
      } else if (c == 'p' || c == 'P') {
        if (FaceRegistry::count() > 0) {
          gPrevPickerIndex = gPickerIndex;
          gPickerIndex = (gPickerIndex + FaceRegistry::count() - 1) % FaceRegistry::count();
          gOverlayDirty = OVL_ROWS;
        }
      } else if (c == 'n' || c == 'N') {
        if (FaceRegistry::count() > 0) {
          gPrevPickerIndex = gPickerIndex;
          gPickerIndex = (gPickerIndex + 1) % FaceRegistry::count();
          gOverlayDirty = OVL_ROWS;
        }
      } else if (c == '\r') {
        const FaceInfo* info = FaceRegistry::infoAt(gPickerIndex);
        if (info && FaceRegistry::setActiveId(info->id)) {
          settingsSetActiveFace(info->id);
          goTo(SHELL_FACE);
          LOGI("shell", "state=FACE active=%s", info->id);
        }
      }
      break;

    case SHELL_SETTINGS:
      if (c == 'q' || c == 'Q' || c == 27) {
        goTo(SHELL_FACE);
        LOGI("shell", "state=FACE");
      } else if (c == 'p' || c == 'P') {
        gPrevSettingsRow = gSettingsRow;
        gSettingsRow = (gSettingsRow + SET_ROW_COUNT - 1) % SET_ROW_COUNT;
        gOverlayDirty = OVL_ROWS;
      } else if (c == 'n' || c == 'N') {
        gPrevSettingsRow = gSettingsRow;
        gSettingsRow = (gSettingsRow + 1) % SET_ROW_COUNT;
        gOverlayDirty = OVL_ROWS;
      } else if (c == '+' || c == '=') {
        adjustSelected(+1);
      } else if (c == '-' || c == '_') {
        adjustSelected(-1);
      } else if (c == '\r') {
        applySettingsRow();
      }
      break;
  }
}

void shellPollSerial() {
  if (!gConsoleReady) {
    return;
  }

  uint8_t buf[16];
  const int n = usb_serial_jtag_read_bytes(buf, sizeof(buf), 0);
  for (int i = 0; i < n; i++) {
    shellHandleKey(static_cast<char>(buf[i]));
  }
}
