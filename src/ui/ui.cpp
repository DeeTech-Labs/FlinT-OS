#include "ui.h"
#include "src/config/config.h"
#include "src/display/display.h"
#include "src/time/clock_time.h"
#include "src/weather/weather.h"
#include "src/net/wifi_net.h"
#include "src/day_mode/day_mode.h"
#include "src/config/debug.h"
#include "src/faces/FaceRegistry.h"
#include "src/faces/FaceContext.h"

static bool gForce = true;
static DayMode gLastMode = DAY_MODE_DAY;

static FaceContext buildContext() {
  FaceContext ctx{};
  ctx.hasTime = clockTimeGetLocal(&ctx.localTime);
  const int hour = ctx.hasTime ? ctx.localTime.tm_hour : 12;
  ctx.dayMode = dayModeFromHour(hour);
  ctx.dayProfile = dayModeProfile(ctx.dayMode);
  ctx.weather = weatherGet();
  ctx.palette = dayModeBuildPalette(ctx.dayMode, ctx.weather.ok ? ctx.weather.code : 0,
                                    ctx.weather.ok);
  ctx.wifiConnected = wifiNetIsConnected();
  ctx.screenW = SCR_W;
  ctx.screenH = SCR_H;
  return ctx;
}

void uiBegin() {
  gForce = true;
  LOGI("ui", "faces registered=%d", FaceRegistry::count());

  if (!FaceRegistry::setActiveId(ACTIVE_FACE_ID)) {
    if (FaceRegistry::count() > 0) {
      const FaceInfo* info = FaceRegistry::infoAt(0);
      if (info) {
        FaceRegistry::setActiveId(info->id);
      }
    }
  }

  Face* face = FaceRegistry::active();
  if (face) {
    FaceContext ctx = buildContext();
    face->onActivate(ctx);
    gLastMode = ctx.dayMode;
  }
}

void uiForceRedraw() {
  gForce = true;
}

void uiTick() {
  Face* face = FaceRegistry::active();
  if (!face) {
    return;
  }

  FaceContext ctx = buildContext();

  if (gForce || ctx.dayMode != gLastMode) {
    if (ctx.dayMode != gLastMode) {
      LOGI("mode", "switch -> %s", ctx.dayProfile.nameRu);
    }
    face->onForceRedraw(ctx);
    gLastMode = ctx.dayMode;
    gForce = false;
    return;
  }

  face->onTick(ctx);
}
