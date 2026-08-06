#include "faces/glance.h"

#include "display/display.h"
#include "sdk/resources.h"
#include "time/date_fmt.h"

bool flintGlanceDraw(const FaceContext& ctx, GlanceKind kind, GlanceRect r,
                     const char* label) {
  auto& gfx = displayGfx();
  const UiPalette& pal = ctx.palette;

  switch (kind) {
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
  }
  return false;
}
