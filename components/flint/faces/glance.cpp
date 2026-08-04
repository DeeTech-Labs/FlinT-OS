#include "faces/glance.h"

#include "display/display.h"
#include "display/ui_tokens.h"
#include "i18n/i18n.h"
#include "sdk/resources.h"

#include <stdio.h>
#include <string.h>

static void drawWifiSignal(lgfx::LGFXBase& gfx, int cx, int cy, bool on, uint16_t fg,
                           uint16_t bg) {
  gfx.fillRect(cx - 8, cy - 8, 16, 16, bg);
  const uint16_t dim = colorLerp(bg, fg, on ? 255 : 90);
  // Three rising bars + tip dot (16×16 budget).
  gfx.fillRect(cx - 6, cy + 3, 3, 4, dim);
  gfx.fillRect(cx - 1, cy, 3, 7, dim);
  gfx.fillRect(cx + 4, cy - 3, 3, 10, dim);
  if (on) {
    gfx.fillCircle(cx + 5, cy - 5, 1, fg);
  }
}

bool flintGlanceDraw(const FaceContext& ctx, GlanceKind kind, GlanceRect r) {
  auto& gfx = displayGfx();
  const UiPalette& pal = ctx.palette;

  switch (kind) {
    case GLANCE_WEATHER: {
      if (!ctx.dayProfile.showWeather) {
        return false;
      }
      char temp[12];
      int iconCode = -1;
      const WeatherState& w = ctx.weather;
      if (w.ok) {
        snprintf(temp, sizeof(temp), "%+.0f", static_cast<double>(w.tempC));
        iconCode = w.code;
      } else if (w.loading) {
        snprintf(temp, sizeof(temp), "..");
      } else {
        snprintf(temp, sizeof(temp), "--");
      }

      gfx.fillRect(r.x, r.y, r.w, r.h, pal.bgTop);
      const int iconCx = r.x + 12;
      const int iconCy = r.y + r.h / 2;
      if (iconCode >= 0) {
        flintDrawIcon(gfx, flintWeatherIcon(iconCode), iconCx, iconCy, pal.accent, pal.bgTop);
      }
      displayDrawText(gfx, temp, r.x + 26, r.y + r.h - 3, pal.textPrimary, pal.bgTop,
                      flintFont(FlintFontId::Temp));
      return true;
    }

    case GLANCE_WIFI: {
      const int cx = r.x + r.w / 2;
      const int cy = r.y + r.h / 2;
      const uint16_t fg = ctx.wifiConnected ? pal.wifiOn : pal.wifiOff;
      drawWifiSignal(gfx, cx, cy, ctx.wifiConnected, fg, pal.bgTop);
      return true;
    }

    case GLANCE_DATE: {
      char dateStr[32];
      if (ctx.hasTime) {
        snprintf(dateStr, sizeof(dateStr), "%s  %02d.%02d.%04d",
                 flintTrWeekday(ctx.localTime.tm_wday), ctx.localTime.tm_mday,
                 ctx.localTime.tm_mon + 1, ctx.localTime.tm_year + 1900);
      } else {
        snprintf(dateStr, sizeof(dateStr), "%s", flintTr(FlintStr::WaitingForTime));
      }
      gfx.fillRect(r.x, r.y, r.w, r.h, pal.bgTop);
      displaySetFont(gfx, flintFont(FlintFontId::Body));
      const int tw = displayTextWidth(gfx, dateStr);
      int x = r.x + (r.w - tw) / 2;
      if (x < r.x) x = r.x;
      displayDrawText(gfx, dateStr, x, r.y + r.h - 4, pal.textSecondary, pal.bgTop,
                      flintFont(FlintFontId::Body));
      return true;
    }

    case GLANCE_DAILY_RANGE: {
      if (!ctx.weather.dailyOk) {
        return false;
      }
      char line[24];
      const float tMin = ctx.weather.daily.tMinCx10 / 10.0f;
      const float tMax = ctx.weather.daily.tMaxCx10 / 10.0f;
      snprintf(line, sizeof(line), "%+.0f/%+.0f", static_cast<double>(tMin),
               static_cast<double>(tMax));
      gfx.fillRect(r.x, r.y, r.w, r.h, pal.bgTop);
      // Temp font matches current-temp size budget; Body glyphs overflow a 16px band.
      displayDrawText(gfx, line, r.x + UiTok::kGapSm, r.y + r.h - 2, pal.textSecondary, pal.bgTop,
                      flintFont(FlintFontId::Temp));
      return true;
    }
  }
  return false;
}
