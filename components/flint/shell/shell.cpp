#include "shell/shell.h"

#include "anim/anim.h"
#include "config/debug.h"
#include "display/display.h"
#include "display/ui_tokens.h"
#include "event/event.h"
#include "faces/FaceRegistry.h"
#include "settings/settings.h"
#include "shell/shell_picker.h"
#include "shell/shell_settings.h"
#include "ui/ui.h"
#include "ui/ui_dirty.h"
#include "util/flint_sys.h"

#include <stdio.h>

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_err.h"

enum ShellState : uint8_t {
  SHELL_FACE = 0,
  SHELL_PICKER,
  SHELL_SETTINGS,
};

static ShellState gState = SHELL_FACE;
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
}

static void enterPicker() {
  gState = SHELL_PICKER;
  shellPickerEnter();
  animBegin(ANIM_OVERLAY_IN);
}

static void enterSettings() {
  gState = SHELL_SETTINGS;
  shellSettingsEnter();
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
        shellPickerPaint(animEasedProgress());
        animTick();
      } else if (shellPickerNeedsFull()) {
        shellPickerPaint(255);
      } else if (shellPickerNeedsRows()) {
        shellPickerPaintPartial();
      }
      break;

    case SHELL_SETTINGS:
      if (animIsActive()) {
        shellSettingsPaint(animEasedProgress());
        animTick();
      } else if (shellSettingsNeedsFull()) {
        shellSettingsPaint(255);
      } else if (shellSettingsNeedsRows()) {
        shellSettingsPaintPartial();
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
        shellPickerNavPrev();
      } else if (c == 'n' || c == 'N') {
        shellPickerNavNext();
      } else if (c == '\r') {
        if (shellPickerConfirm()) {
          const char* id = FaceRegistry::activeId();
          goTo(SHELL_FACE);
          LOGI("shell", "state=FACE active=%s", id ? id : "?");
        }
      }
      break;

    case SHELL_SETTINGS:
      if (c == 'q' || c == 'Q' || c == 27) {
        goTo(SHELL_FACE);
        LOGI("shell", "state=FACE");
      } else if (c == 'p' || c == 'P') {
        shellSettingsNavPrev();
      } else if (c == 'n' || c == 'N') {
        shellSettingsNavNext();
      } else if (c == '+' || c == '=') {
        shellSettingsAdjust(+1);
      } else if (c == '-' || c == '_') {
        shellSettingsAdjust(-1);
      } else if (c == '\r') {
        if (shellSettingsApply()) {
          goTo(SHELL_PICKER);
        }
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
