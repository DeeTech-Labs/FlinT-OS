#include "sdk/resources.h"
#include "display/fonts_u8g2.h"

#include <math.h>

const lgfx::IFont* flintFont(FlintFontId id) {
  switch (id) {
    case FlintFontId::TimeLarge:
      return flintFontTimeLarge();
    case FlintFontId::Temp:
      return flintFontTemp();
    case FlintFontId::Body:
    default:
      return flintFontBody();
  }
}

FlintIconId flintWeatherIcon(int code) {
  if (code < 0) {
    return FlintIconId::Unknown;
  }
  if (code == 0) {
    return FlintIconId::WeatherClear;
  }
  if (code <= 3) {
    return FlintIconId::Cloudy;
  }
  if (code == 45 || code == 48) {
    return FlintIconId::Fog;
  }
  if (code >= 71 && code <= 86) {
    return FlintIconId::Snow;
  }
  if (code >= 95) {
    return FlintIconId::Storm;
  }
  return FlintIconId::Rain;
}

void flintDrawIcon(lgfx::LGFXBase& gfx, FlintIconId id, int cx, int cy,
                   uint16_t fg, uint16_t bg) {
  gfx.fillRect(cx - 10, cy - 10, 20, 20, bg);

  switch (id) {
    case FlintIconId::WeatherClear:
      gfx.fillCircle(cx, cy, 4, fg);
      for (int a = 0; a < 8; a++) {
        const float ang = a * 0.785398f;
        gfx.drawPixel(cx + (int)(8 * cosf(ang)), cy + (int)(8 * sinf(ang)), fg);
      }
      break;

    case FlintIconId::Cloudy:
      gfx.fillCircle(cx - 2, cy + 1, 4, fg);
      gfx.fillCircle(cx + 3, cy + 1, 5, fg);
      gfx.fillCircle(cx, cy - 2, 3, fg);
      break;

    case FlintIconId::Fog:
      gfx.drawFastHLine(cx - 7, cy - 2, 14, fg);
      gfx.drawFastHLine(cx - 5, cy + 2, 10, fg);
      break;

    case FlintIconId::Snow:
      gfx.drawLine(cx, cy - 5, cx, cy + 5, fg);
      gfx.drawLine(cx - 4, cy - 2, cx + 4, cy + 2, fg);
      gfx.drawLine(cx - 4, cy + 2, cx + 4, cy - 2, fg);
      break;

    case FlintIconId::Storm:
      gfx.fillTriangle(cx - 3, cy - 5, cx + 4, cy - 5, cx, cy + 1, fg);
      gfx.fillTriangle(cx, cy - 1, cx + 3, cy - 1, cx - 3, cy + 6, fg);
      break;

    case FlintIconId::Rain:
      gfx.fillCircle(cx - 2, cy - 2, 4, fg);
      gfx.fillCircle(cx + 3, cy - 1, 4, fg);
      gfx.drawLine(cx - 3, cy + 4, cx - 4, cy + 7, fg);
      gfx.drawLine(cx + 2, cy + 4, cx + 1, cy + 7, fg);
      break;

    case FlintIconId::Unknown:
    default:
      gfx.drawCircle(cx, cy, 6, fg);
      break;
  }
}
