#include "faces/glance.h"

#include "display/display.h"
#include "display/ui_tokens.h"
#include "sdk/resources.h"
#include "time/date_fmt.h"

bool flintGlanceDraw(const FaceContext& ctx, GlanceKind kind, GlanceRect r,
                     const char* label) {
  auto& gfx = displayGfx();
  const UiPalette& pal = ctx.palette;

  switch (kind) {
    case GLANCE_WEATHER: {
      if (!ctx.dayProfile.showWeather) {
        return false;
      }
      char tempLocal[12];
      const char* temp = label;
      int iconCode = -1;
      const WeatherState& w = *ctx.weather;
      if (!temp) {
        weatherFormatTempC(tempLocal, sizeof(tempLocal), w);
        temp = tempLocal;
      }
      if (w.ok) {
        iconCode = w.code;
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
      const uint16_t fg = ctx.wifiBars > 0 ? pal.wifiOn : pal.wifiOff;
      flintDrawIcon(gfx, flintWifiIcon(ctx.wifiBars), cx, cy, fg, pal.bgTop);
      return true;
    }

    case GLANCE_DATE: {
      char dateLocal[40];
      const char* dateStr = label;
      if (!dateStr) {
        flintFormatDateLine(dateLocal, sizeof(dateLocal), ctx.hasTime, ctx.localTime,
                            ctx.dateFormat, ctx.dateSep);
        dateStr = dateLocal;
      }
      gfx.fillRect(r.x, r.y, r.w, r.h, pal.bgTop);
      displaySetFont(gfx, flintFont(FlintFontId::Body));
      const int tw = displayTextWidth(gfx, dateStr);
      int x = r.x + (r.w - tw) / 2;
      if (x < r.x) {
        x = r.x;
      }
      displayDrawText(gfx, dateStr, x, r.y + r.h - 4, pal.textSecondary, pal.bgTop,
                      flintFont(FlintFontId::Body));
      return true;
    }

    case GLANCE_DAILY_RANGE: {
      if (!ctx.weather->dailyOk) {
        return false;
      }
      char line[24];
      weatherFormatDailyRange(line, sizeof(line), ctx.weather->daily);
      gfx.fillRect(r.x, r.y, r.w, r.h, pal.bgTop);
      // Temp font matches current-temp size budget; Body glyphs overflow a 16px band.
      displayDrawText(gfx, line, r.x + UiTok::kGapSm, r.y + r.h - 2, pal.textSecondary, pal.bgTop,
                      flintFont(FlintFontId::Temp));
      return true;
    }
  }
  return false;
}
