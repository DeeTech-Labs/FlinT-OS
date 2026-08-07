#include "ui.h"
#include "ui/ui_dirty.h"
#include "config/config.h"
#include "config/debug.h"
#include "display/display.h"
#include "time/clock_time.h"
#include "net/wifi_net.h"
#include "day_mode/day_mode.h"
#include "settings/settings.h"
#include "faces/FaceRegistry.h"
#include "faces/FaceContext.h"

static DayMode gLastMode = DAY_MODE_WAITING;
static FaceContext gLastCtx{};
static bool gHaveLastCtx = false;

FaceContext uiBuildContext() {
  FaceContext ctx{};
  ctx.hasTime = clockTimeGetLocal(&ctx.localTime);
  if (ctx.hasTime) {
    ctx.dayMode = dayModeFromHour(ctx.localTime.tm_hour);
  } else {
    ctx.dayMode = DAY_MODE_WAITING;
  }
  ctx.dayProfile = dayModeProfile(ctx.dayMode);
  ctx.palette = dayModeBuildPalette(ctx.dayMode);
  ctx.wifiConnected = wifiNetIsConnected();
  ctx.wifiBars = wifiNetBars();

  ctx.showSeconds = settingsResolveShowSeconds(ctx.dayProfile.showSeconds);
  ctx.hour12 = settingsUse12Hour();
  ctx.dateFormat = settingsDateFormat();
  ctx.dateSep = settingsDateSep();

  ctx.screenW = SCR_W;
  ctx.screenH = SCR_H;
  ctx.dirty = 0;
  return ctx;
}

bool uiWantsSeconds() {
  if (gHaveLastCtx) {
    return gLastCtx.showSeconds;
  }
  struct tm ti{};
  if (!clockTimeGetLocal(&ti)) {
    return settingsResolveShowSeconds(dayModeProfile(DAY_MODE_WAITING).showSeconds);
  }
  return settingsResolveShowSeconds(dayModeProfile(dayModeFromHour(ti.tm_hour)).showSeconds);
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

  gHaveLastCtx = false;
  uiMarkDirty(UI_DIRTY_FORCE);
}

void uiFaceActivateCurrent() {
  Face* face = FaceRegistry::active();
  if (!face) {
    return;
  }
  FaceContext ctx = uiBuildContext();
  gLastCtx = ctx;
  gHaveLastCtx = true;
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

  // Colon pulse only: reuse last context (skip palette / clock / format rebuild).
  if ((dirty & ~UI_DIRTY_MOTION) == 0 && gHaveLastCtx) {
    gLastCtx.dirty = dirty;
    face->onTick(gLastCtx);
    return;
  }

  FaceContext ctx = uiBuildContext();
  ctx.dirty = dirty;
  if (!ctx.showSeconds) {
    dirty &= ~UI_DIRTY_SECOND;
    ctx.dirty = dirty;
    if (!dirty) {
      gLastCtx = ctx;
      gHaveLastCtx = true;
      return;
    }
  }

  // FORCE / DAY_MODE / TIME_VALID → onForceRedraw.
  // SETTINGS stays soft → onTick (faces rebuild layout if prefs changed).
  const bool force = (dirty & (UI_DIRTY_FORCE | UI_DIRTY_DAY_MODE | UI_DIRTY_TIME_VALID)) != 0
                     || ctx.dayMode != gLastMode;

  gLastCtx = ctx;
  gHaveLastCtx = true;

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
