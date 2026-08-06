#include "ui.h"
#include "ui/ui_dirty.h"
#include "config/config.h"
#include "config/debug.h"
#include "display/display.h"
#include "time/clock_time.h"
#include "weather/weather.h"
#include "net/wifi_net.h"
#include "day_mode/day_mode.h"
#include "settings/settings.h"
#include "faces/FaceRegistry.h"
#include "faces/FaceContext.h"

static DayMode gLastMode = DAY_MODE_DAY;

FaceContext uiBuildContext() {
  FaceContext ctx{};
  ctx.hasTime = clockTimeGetLocal(&ctx.localTime);
  const int hour = ctx.hasTime ? ctx.localTime.tm_hour : 12;
  ctx.dayMode = dayModeFromHour(hour);
  ctx.dayProfile = dayModeProfile(ctx.dayMode);
  ctx.weather = &weatherSnapshot();
  ctx.palette = dayModeBuildPalette(ctx.dayMode, ctx.weather->ok ? ctx.weather->code : 0,
                                    ctx.weather->ok);
  ctx.wifiConnected = wifiNetIsConnected();

  ctx.showSeconds = settingsResolveShowSeconds(ctx.dayProfile.showSeconds);
  ctx.hour12 = settingsUse12Hour();
  ctx.dateFormat = settingsDateFormat();
  ctx.dateSep = settingsDateSep();

  ctx.screenW = SCR_W;
  ctx.screenH = SCR_H;
  return ctx;
}

bool uiWantsSeconds() {
  struct tm ti{};
  const bool has = clockTimeGetLocal(&ti);
  const int hour = has ? ti.tm_hour : 12;
  return settingsResolveShowSeconds(dayModeProfile(dayModeFromHour(hour)).showSeconds);
}

void uiFaceBegin(const char* preferredFaceId) {
  facesRegisterBuiltins();
  LOGI("ui", "faces registered=%d", FaceRegistry::count());

  const char* want = preferredFaceId && preferredFaceId[0] ? preferredFaceId : ACTIVE_FACE_ID;
  if (!FaceRegistry::setActiveId(want)) {
    if (FaceRegistry::count() > 0) {
      const FaceInfo* info = FaceRegistry::infoAt(0);
      if (info) {
        FaceRegistry::setActiveId(info->id);
      }
    }
  }

  uiMarkDirty(UI_DIRTY_FORCE);
}

void uiFaceActivateCurrent() {
  Face* face = FaceRegistry::active();
  if (!face) {
    return;
  }
  FaceContext ctx = uiBuildContext();
  face->onActivate(ctx);
  gLastMode = ctx.dayMode;
}

void uiForceRedraw() {
  uiMarkDirty(UI_DIRTY_FORCE);
}

void uiFaceTick() {
  if (!displayHasCanvas()) {
    return;
  }

  Face* face = FaceRegistry::active();
  if (!face) {
    return;
  }

  uint32_t dirty = uiTakeDirty();
  if (!dirty) {
    return;
  }

  FaceContext ctx = uiBuildContext();
  if (!ctx.showSeconds) {
    dirty &= ~UI_DIRTY_SECOND;
    if (!dirty) {
      return;
    }
  }

  const bool force = (dirty & (UI_DIRTY_FORCE | UI_DIRTY_DAY_MODE | UI_DIRTY_TIME_VALID)) != 0
                     || ctx.dayMode != gLastMode;
  // UI_DIRTY_SETTINGS is soft — onTick handles layout-affecting
  // changes when FORCE was emitted explicitly from shell.

  if (force) {
    if (ctx.dayMode != gLastMode) {
      LOGI("mode", "switch -> %s", dayModeName(ctx.dayMode));
    }
    face->onForceRedraw(ctx);
    gLastMode = ctx.dayMode;
    return;
  }

  face->onTick(ctx);
}
