#include "shell/shell.h"

#include "config/debug.h"
#include "display/display.h"
#include "faces/FaceRegistry.h"
#include "sdk/resources.h"
#include "settings/settings.h"
#include "ui/ui.h"
#include "ui/ui_dirty.h"

#include <stdio.h>
#include <string.h>

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_err.h"

// Shell owns on-screen navigation. FACE redraw is event-driven via ui_dirty.
// Console input: USB Serial/JTAG driver (default console VFS does not drain RX —
// idf_monitor then times out on keypress).

enum ShellState : uint8_t {
  SHELL_FACE = 0,
  SHELL_PICKER,
  SHELL_SETTINGS,
};

enum SettingsRow : uint8_t {
  SET_ROW_FACE = 0,
  SET_ROW_SECONDS,
  SET_ROW_LANG,
  SET_ROW_LAT,
  SET_ROW_LON,
  SET_ROW_COUNT,
};

static ShellState gState = SHELL_FACE;
static int gPickerIndex = 0;
static int gSettingsRow = 0;
static bool gOverlayDirty = true;
static bool gConsoleReady = false;

static void shellLogHelp() {
  LOGI("shell", "keys: Enter=picker  s=settings  p/n=prev/next  +/- adjust  q/Esc=back  h=help");
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
  uiMarkDirty(UI_DIRTY_FORCE);
  gOverlayDirty = false;
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
  gOverlayDirty = true;
}

static void enterSettings() {
  gState = SHELL_SETTINGS;
  gSettingsRow = SET_ROW_SECONDS;
  gOverlayDirty = true;
}

static const char* showSecLabel(uint8_t mode) {
  switch (mode) {
    case SETTINGS_SEC_OFF: return "off";
    case SETTINGS_SEC_ON: return "on";
    default: return "auto";
  }
}

static const char* langLabel(uint8_t lang) {
  return lang == SETTINGS_LANG_EN ? "en" : "ru";
}

static void drawPicker() {
  auto& gfx = displayGfx();
  const FaceContext ctx = uiBuildContext();
  gfx.fillScreen(ctx.palette.bgTop);

  displayDrawText(gfx, "Faces", 12, 28, ctx.palette.accent, ctx.palette.bgTop,
                  flintFont(FlintFontId::Body));

  const int n = FaceRegistry::count();
  for (int i = 0; i < n; i++) {
    const FaceInfo* info = FaceRegistry::infoAt(i);
    if (!info) {
      continue;
    }
    char line[64];
    snprintf(line, sizeof(line), "%s%.20s  (%.16s)",
             (i == gPickerIndex) ? "> " : "  ",
             info->name ? info->name : "?",
             info->id ? info->id : "?");
    const uint16_t fg = (i == gPickerIndex) ? ctx.palette.textPrimary : ctx.palette.textSecondary;
    displayDrawText(gfx, line, 12, 56 + i * 24, fg, ctx.palette.bgTop,
                    flintFont(FlintFontId::Body));
  }

  displayDrawText(gfx, "p/n  Enter  Esc", 12, ctx.screenH - 16,
                  ctx.palette.textSecondary, ctx.palette.bgTop,
                  flintFont(FlintFontId::Body));
  displayPresent();
  gOverlayDirty = false;
}

static void formatSettingsLine(char* out, size_t outN, int row, bool selected) {
  const FlintSettings& s = settingsGet();
  const char* prefix = selected ? "> " : "  ";
  switch (row) {
    case SET_ROW_FACE:
      snprintf(out, outN, "%sface   %.24s", prefix, s.activeFace);
      break;
    case SET_ROW_SECONDS:
      snprintf(out, outN, "%sseconds  %s", prefix, showSecLabel(s.showSeconds));
      break;
    case SET_ROW_LANG:
      snprintf(out, outN, "%slang     %s", prefix, langLabel(s.lang));
      break;
    case SET_ROW_LAT:
      snprintf(out, outN, "%slat   %+.4f", prefix, static_cast<double>(s.wxLat));
      break;
    case SET_ROW_LON:
      snprintf(out, outN, "%slon   %+.4f", prefix, static_cast<double>(s.wxLon));
      break;
    default:
      out[0] = '\0';
      break;
  }
}

static void drawSettings() {
  auto& gfx = displayGfx();
  const FaceContext ctx = uiBuildContext();
  gfx.fillScreen(ctx.palette.bgTop);

  displayDrawText(gfx, "Settings", 12, 28, ctx.palette.accent, ctx.palette.bgTop,
                  flintFont(FlintFontId::Body));

  for (int i = 0; i < SET_ROW_COUNT; i++) {
    char row[48];
    formatSettingsLine(row, sizeof(row), i, i == gSettingsRow);
    const uint16_t fg = (i == gSettingsRow) ? ctx.palette.textPrimary : ctx.palette.textSecondary;
    displayDrawText(gfx, row, 12, 52 + i * 24, fg, ctx.palette.bgTop,
                    flintFont(FlintFontId::Body));
  }

  displayDrawText(gfx, "p/n  +/-  Enter  Esc", 12, ctx.screenH - 16,
                  ctx.palette.textSecondary, ctx.palette.bgTop,
                  flintFont(FlintFontId::Body));
  displayPresent();
  gOverlayDirty = false;
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
  uiMarkDirty(UI_DIRTY_SETTINGS);
}

static void cycleLang() {
  const FlintSettings& s = settingsGet();
  settingsSetLang(s.lang == SETTINGS_LANG_RU ? SETTINGS_LANG_EN : SETTINGS_LANG_RU);
  uiMarkDirty(UI_DIRTY_SETTINGS);
}

static void nudgeCoord(bool lat, float delta) {
  const FlintSettings& s = settingsGet();
  if (lat) {
    settingsSetWeatherCoords(s.wxLat + delta, s.wxLon);
  } else {
    settingsSetWeatherCoords(s.wxLat, s.wxLon + delta);
  }
  uiMarkDirty(UI_DIRTY_SETTINGS | UI_DIRTY_WEATHER);
}

static void adjustSelected(float delta) {
  switch (gSettingsRow) {
    case SET_ROW_LAT:
      nudgeCoord(true, delta);
      break;
    case SET_ROW_LON:
      nudgeCoord(false, delta);
      break;
    case SET_ROW_SECONDS:
      cycleShowSeconds();
      break;
    case SET_ROW_LANG:
      cycleLang();
      break;
    default:
      break;
  }
  gOverlayDirty = true;
}

static void applySettingsRow() {
  switch (gSettingsRow) {
    case SET_ROW_FACE:
      enterPicker();
      break;
    case SET_ROW_SECONDS:
      cycleShowSeconds();
      gOverlayDirty = true;
      break;
    case SET_ROW_LANG:
      cycleLang();
      gOverlayDirty = true;
      break;
    default:
      break;
  }
}

void shellBegin() {
  shellConsoleInit();
  settingsLoad();
  uiFaceBegin(settingsGet().activeFace);
  enterFace();
  shellLogHelp();
  LOGI("shell", "state=FACE");
}

void shellTick() {
  switch (gState) {
    case SHELL_FACE:
      uiFaceTick();
      break;
    case SHELL_PICKER:
      if (gOverlayDirty) {
        drawPicker();
      }
      break;
    case SHELL_SETTINGS:
      if (gOverlayDirty) {
        drawSettings();
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

  switch (gState) {
    case SHELL_FACE:
      if (c == '\r') {
        enterPicker();
        LOGI("shell", "state=PICKER");
      } else if (c == 's' || c == 'S') {
        enterSettings();
        LOGI("shell", "state=SETTINGS");
      }
      break;

    case SHELL_PICKER:
      if (c == 'q' || c == 'Q' || c == 27) {
        enterFace();
        LOGI("shell", "state=FACE (cancel)");
      } else if (c == 'p' || c == 'P') {
        if (FaceRegistry::count() > 0) {
          gPickerIndex = (gPickerIndex + FaceRegistry::count() - 1) % FaceRegistry::count();
          gOverlayDirty = true;
        }
      } else if (c == 'n' || c == 'N') {
        if (FaceRegistry::count() > 0) {
          gPickerIndex = (gPickerIndex + 1) % FaceRegistry::count();
          gOverlayDirty = true;
        }
      } else if (c == '\r') {
        const FaceInfo* info = FaceRegistry::infoAt(gPickerIndex);
        if (info && FaceRegistry::setActiveId(info->id)) {
          settingsSetActiveFace(info->id);
          enterFace();
          LOGI("shell", "state=FACE active=%s", info->id);
        }
      }
      break;

    case SHELL_SETTINGS:
      if (c == 'q' || c == 'Q' || c == 27) {
        enterFace();
        LOGI("shell", "state=FACE");
      } else if (c == 'p' || c == 'P') {
        gSettingsRow = (gSettingsRow + SET_ROW_COUNT - 1) % SET_ROW_COUNT;
        gOverlayDirty = true;
      } else if (c == 'n' || c == 'N') {
        gSettingsRow = (gSettingsRow + 1) % SET_ROW_COUNT;
        gOverlayDirty = true;
      } else if (c == '+' || c == '=') {
        adjustSelected(0.1f);
      } else if (c == '-' || c == '_') {
        adjustSelected(-0.1f);
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
