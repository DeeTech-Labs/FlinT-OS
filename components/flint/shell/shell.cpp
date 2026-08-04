#include "shell/shell.h"

#include "config/debug.h"
#include "display/display.h"
#include "event/event.h"
#include "faces/FaceRegistry.h"
#include "i18n/i18n.h"
#include "sdk/resources.h"
#include "settings/settings.h"
#include "time/tz_db.h"
#include "ui/ui.h"
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
  SET_ROW_LANG,
  SET_ROW_TZ,
  SET_ROW_LAT,
  SET_ROW_LON,
  SET_ROW_COUNT,
};

enum OverlayDirty : uint8_t {
  OVL_NONE = 0,
  OVL_FULL = 1,
  OVL_ROWS = 2,   // selection / value band only
};

static constexpr int kSetVisible = 7;
static constexpr int kSetRowH = 22;
static constexpr int kSetRowY0 = 46;
static constexpr int kPickRowH = 24;
static constexpr int kPickRowY0 = 56;

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
}

static void enterSettings() {
  gState = SHELL_SETTINGS;
  gSettingsRow = SET_ROW_SECONDS;
  gPrevSettingsRow = gSettingsRow;
  gSettingsStart = 0;
  gOverlayDirty = OVL_FULL;
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

static void formatSettingsLine(char* out, size_t outN, int row, bool selected) {
  const FlintSettings& s = settingsGet();
  const char* prefix = selected ? "> " : "  ";
  switch (row) {
    case SET_ROW_FACE:
      snprintf(out, outN, "%s%s   %.16s", prefix, flintTr(FlintStr::Face), s.activeFace);
      break;
    case SET_ROW_SECONDS:
      snprintf(out, outN, "%s%s  %s", prefix, flintTr(FlintStr::Seconds), showSecLabel(s.showSeconds));
      break;
    case SET_ROW_HOUR:
      snprintf(out, outN, "%s%s     %s", prefix, flintTr(FlintStr::Hour), hourLabel(s.hourFormat));
      break;
    case SET_ROW_LANG:
      snprintf(out, outN, "%s%s     %s", prefix, flintTr(FlintStr::Lang), langLabel(s.lang));
      break;
    case SET_ROW_TZ: {
      const TzEntry* e = tzDbFind(s.tzId);
      snprintf(out, outN, "%s%s    %.14s", prefix, flintTr(FlintStr::Timezone), e->label);
      break;
    }
    case SET_ROW_LAT:
      snprintf(out, outN, "%s%s   %+.2f", prefix, flintTr(FlintStr::Lat),
               static_cast<double>(s.wxLat));
      break;
    case SET_ROW_LON:
      snprintf(out, outN, "%s%s   %+.2f", prefix, flintTr(FlintStr::Lon),
               static_cast<double>(s.wxLon));
      break;
    default:
      out[0] = '\0';
      break;
  }
}

static int settingsWindowStart() {
  int start = gSettingsRow - kSetVisible / 2;
  if (start < 0) start = 0;
  if (start > SET_ROW_COUNT - kSetVisible) start = SET_ROW_COUNT - kSetVisible;
  if (start < 0) start = 0;
  return start;
}

static void paintSettingsChrome(const FaceContext& ctx) {
  auto& gfx = displayGfx();
  gfx.fillScreen(ctx.palette.bgTop);
  displayDrawText(gfx, flintTr(FlintStr::Settings), 12, 24, ctx.palette.accent, ctx.palette.bgTop,
                  flintFont(FlintFontId::Body));
  displayDrawText(gfx, flintTr(FlintStr::PNPlusEnterEsc), 12, ctx.screenH - 14,
                  ctx.palette.textSecondary, ctx.palette.bgTop,
                  flintFont(FlintFontId::Body));
}

static void paintSettingsScrollCue(const FaceContext& ctx) {
  auto& gfx = displayGfx();
  char cue[16];
  snprintf(cue, sizeof(cue), "%d/%d", gSettingsRow + 1, SET_ROW_COUNT);
  displaySetFont(gfx, flintFont(FlintFontId::Body));
  const int tw = displayTextWidth(gfx, cue);
  gfx.fillRect(ctx.screenW - tw - 16, 8, tw + 12, 18, ctx.palette.bgTop);
  displayDrawText(gfx, cue, ctx.screenW - tw - 12, 22, ctx.palette.textSecondary, ctx.palette.bgTop,
                  flintFont(FlintFontId::Body));

  // Thin thumb on right edge
  const int trackH = kSetVisible * kSetRowH;
  const int thumbH = SET_ROW_COUNT > 0 ? (trackH * kSetVisible) / SET_ROW_COUNT : trackH;
  const int maxStart = SET_ROW_COUNT - kSetVisible;
  const int thumbY = kSetRowY0 + (maxStart > 0 ? (gSettingsStart * (trackH - thumbH)) / maxStart : 0);
  gfx.fillRect(ctx.screenW - 4, kSetRowY0, 3, trackH, ctx.palette.panel);
  gfx.fillRect(ctx.screenW - 4, thumbY, 3, thumbH > 4 ? thumbH : 4, ctx.palette.accent);
}

static void paintSettingsRow(const FaceContext& ctx, int row, int visIndex) {
  auto& gfx = displayGfx();
  char line[48];
  formatSettingsLine(line, sizeof(line), row, row == gSettingsRow);
  const uint16_t fg = (row == gSettingsRow) ? ctx.palette.textPrimary : ctx.palette.textSecondary;
  const int y = kSetRowY0 + visIndex * kSetRowH;
  gfx.fillRect(0, y - 14, ctx.screenW - 6, kSetRowH, ctx.palette.bgTop);
  displayDrawText(gfx, line, 12, y, fg, ctx.palette.bgTop, flintFont(FlintFontId::Body));
}

static void drawSettingsFull() {
  if (!displayHasCanvas()) {
    return;
  }
  const FaceContext ctx = uiBuildContext();
  paintSettingsChrome(ctx);
  gSettingsStart = settingsWindowStart();
  for (int i = 0; i < kSetVisible && gSettingsStart + i < SET_ROW_COUNT; i++) {
    paintSettingsRow(ctx, gSettingsStart + i, i);
  }
  paintSettingsScrollCue(ctx);
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
    drawSettingsFull();
    return;
  }

  DisplayRect rects[4];
  int n = 0;

  auto addRow = [&](int row) {
    const int vis = row - gSettingsStart;
    if (vis < 0 || vis >= kSetVisible) {
      return;
    }
    paintSettingsRow(ctx, row, vis);
    rects[n++] = DisplayRect{0, static_cast<int16_t>(kSetRowY0 + vis * kSetRowH - 14),
                             static_cast<int16_t>(ctx.screenW - 6),
                             static_cast<int16_t>(kSetRowH)};
  };

  if (gPrevSettingsRow != gSettingsRow) {
    addRow(gPrevSettingsRow);
    addRow(gSettingsRow);
  } else {
    addRow(gSettingsRow);
  }

  paintSettingsScrollCue(ctx);
  rects[n++] = DisplayRect{static_cast<int16_t>(ctx.screenW - 48), 8, 48, 20};
  rects[n++] = DisplayRect{static_cast<int16_t>(ctx.screenW - 6),
                           static_cast<int16_t>(kSetRowY0), 6,
                           static_cast<int16_t>(kSetVisible * kSetRowH)};

  displayPresentRects(rects, n);
  gPrevSettingsRow = gSettingsRow;
  gOverlayDirty = OVL_NONE;
}

static void paintPickerRow(const FaceContext& ctx, int index) {
  auto& gfx = displayGfx();
  const FaceInfo* info = FaceRegistry::infoAt(index);
  if (!info) {
    return;
  }
  char line[64];
  snprintf(line, sizeof(line), "%s%.20s  (%.16s)",
           (index == gPickerIndex) ? "> " : "  ",
           info->name ? info->name : "?",
           info->id ? info->id : "?");
  const uint16_t fg = (index == gPickerIndex) ? ctx.palette.textPrimary : ctx.palette.textSecondary;
  const int y = kPickRowY0 + index * kPickRowH;
  gfx.fillRect(0, y - 16, ctx.screenW, kPickRowH, ctx.palette.bgTop);
  displayDrawText(gfx, line, 12, y, fg, ctx.palette.bgTop, flintFont(FlintFontId::Body));
}

static void drawPickerFull() {
  if (!displayHasCanvas()) {
    return;
  }
  auto& gfx = displayGfx();
  const FaceContext ctx = uiBuildContext();
  gfx.fillScreen(ctx.palette.bgTop);
  displayDrawText(gfx, flintTr(FlintStr::Faces), 12, 28, ctx.palette.accent, ctx.palette.bgTop,
                  flintFont(FlintFontId::Body));
  const int n = FaceRegistry::count();
  for (int i = 0; i < n; i++) {
    paintPickerRow(ctx, i);
  }
  displayDrawText(gfx, flintTr(FlintStr::PNEnterEsc), 12, ctx.screenH - 16,
                  ctx.palette.textSecondary, ctx.palette.bgTop,
                  flintFont(FlintFontId::Body));
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
    paintPickerRow(ctx, index);
    rects[n++] = DisplayRect{0, static_cast<int16_t>(kPickRowY0 + index * kPickRowH - 16),
                             static_cast<int16_t>(ctx.screenW),
                             static_cast<int16_t>(kPickRowH)};
  };
  if (gPrevPickerIndex != gPickerIndex) {
    add(gPrevPickerIndex);
  }
  add(gPickerIndex);
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
  gOverlayDirty = OVL_FULL;  // labels changed
}

static void cycleHourFormat() {
  const FlintSettings& s = settingsGet();
  settingsSetHourFormat(s.hourFormat == SETTINGS_HOUR_24 ? SETTINGS_HOUR_12 : SETTINGS_HOUR_24);
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
  // Enter = action only (open picker for Face). Values change via +/-.
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
    case SHELL_FACE:
      uiFaceTick();
      break;
    case SHELL_PICKER:
      if (gOverlayDirty == OVL_FULL) {
        drawPickerFull();
      } else if (gOverlayDirty == OVL_ROWS) {
        drawPickerPartial();
      }
      break;
    case SHELL_SETTINGS:
      if (gOverlayDirty == OVL_FULL) {
        drawSettingsFull();
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
