#include "ui/ui_dirty.h"
#include "ui/ui_dirty_rules.h"
#include "host_check.h"

#include <cstdio>

int main() {
  hostRequire(uiDirtyIsMotionOnly(UI_DIRTY_MOTION), "motion-only");
  hostRequire(!uiDirtyIsMotionOnly(0), "empty not motion-only");
  hostRequire(!uiDirtyIsMotionOnly(UI_DIRTY_MOTION | UI_DIRTY_SECOND), "motion+second");

  hostRequire(uiDirtyFilterSeconds(UI_DIRTY_SECOND | UI_DIRTY_MINUTE, false) == UI_DIRTY_MINUTE,
              "drop SECOND when !showSeconds");
  hostRequire(uiDirtyFilterSeconds(UI_DIRTY_SECOND, true) == UI_DIRTY_SECOND,
              "keep SECOND when showSeconds");

  hostRequire(uiDirtyWantsForceRedraw(UI_DIRTY_FORCE), "FORCE");
  hostRequire(uiDirtyWantsForceRedraw(UI_DIRTY_DAY_MODE), "DAY_MODE");
  hostRequire(uiDirtyWantsForceRedraw(UI_DIRTY_TIME_VALID), "TIME_VALID");
  hostRequire(!uiDirtyWantsForceRedraw(UI_DIRTY_SETTINGS), "SETTINGS soft");
  hostRequire(!uiDirtyWantsForceRedraw(UI_DIRTY_MINUTE | UI_DIRTY_USB), "minute/usb soft");

  std::puts("ok test_ui_dirty");
  return 0;
}
