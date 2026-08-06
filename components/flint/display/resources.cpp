#include "sdk/resources.h"
#include "assets/asset_store.h"
#include "display/display.h"
#include "display/fonts_u8g2.h"

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

FlintWxClass flintWeatherClass(int code) {
  // Single WMO ladder — icons, i18n labels, and accent all derive from this.
  if (code < 0) {
    return FlintWxClass::Unknown;
  }
  if (code == 0) {
    return FlintWxClass::Clear;
  }
  if (code <= 3) {
    return FlintWxClass::Cloudy;
  }
  if (code == 45 || code == 48) {
    return FlintWxClass::Fog;
  }
  if (code >= 51 && code <= 57) {
    return FlintWxClass::Drizzle;
  }
  if (code >= 61 && code <= 67) {
    return FlintWxClass::Rain;
  }
  if (code >= 71 && code <= 77) {
    return FlintWxClass::Snow;
  }
  if (code >= 80 && code <= 82) {
    return FlintWxClass::Showers;
  }
  if (code >= 85 && code <= 86) {
    return FlintWxClass::Snow;
  }
  if (code >= 95) {
    return FlintWxClass::Storm;
  }
  return FlintWxClass::Unknown;
}

FlintIconId flintWeatherIcon(int code) {
  switch (flintWeatherClass(code)) {
    case FlintWxClass::Clear:
      return FlintIconId::WeatherClear;
    case FlintWxClass::Cloudy:
      return FlintIconId::Cloudy;
    case FlintWxClass::Fog:
      return FlintIconId::Fog;
    case FlintWxClass::Snow:
      return FlintIconId::Snow;
    case FlintWxClass::Storm:
      return FlintIconId::Storm;
    case FlintWxClass::Drizzle:
    case FlintWxClass::Rain:
    case FlintWxClass::Showers:
      return FlintIconId::Rain;
    case FlintWxClass::Unknown:
    default:
      return FlintIconId::Unknown;
  }
}

uint16_t flintWeatherAccent(int code, uint16_t base) {
  switch (flintWeatherClass(code)) {
    case FlintWxClass::Clear:
      return rgb565(255, 200, 70);
    case FlintWxClass::Cloudy:
      return rgb565(160, 190, 220);
    case FlintWxClass::Fog:
      return rgb565(170, 180, 190);
    case FlintWxClass::Drizzle:
      return rgb565(100, 180, 255);
    case FlintWxClass::Rain:
      return rgb565(80, 160, 255);
    case FlintWxClass::Showers:
      return rgb565(70, 140, 255);
    case FlintWxClass::Snow:
      return rgb565(200, 230, 255);
    case FlintWxClass::Storm:
      return rgb565(180, 120, 255);
    case FlintWxClass::Unknown:
    default:
      return base;
  }
}

FlintIconId flintWifiIcon(uint8_t bars) {
  switch (bars) {
    case 1:
      return FlintIconId::Wifi1;
    case 2:
      return FlintIconId::Wifi2;
    case 3:
      return FlintIconId::Wifi3;
    default:
      return FlintIconId::WifiOff;
  }
}

bool flintIconAvailable(FlintIconId id, FlintIconSize size) {
  return assetIconPresent(id, static_cast<uint8_t>(size));
}

void flintDrawIconSized(lgfx::LGFXBase& gfx, FlintIconId id, FlintIconSize size,
                        int cx, int cy, uint16_t fg, uint16_t bg) {
  const uint8_t px = static_cast<uint8_t>(size);
  // Transparent L1/P* blit — do not fill a solid size×size plate (visible frame).
  (void)assetBlitIcon(gfx, id, px, cx, cy, fg, bg);
}

void flintDrawIcon(lgfx::LGFXBase& gfx, FlintIconId id, int cx, int cy,
                   uint16_t fg, uint16_t bg) {
  FlintIconSize size = FlintIconSize::Glance;
  if (id == FlintIconId::WifiOff || id == FlintIconId::Wifi1 || id == FlintIconId::Wifi2 ||
      id == FlintIconId::Wifi3) {
    size = FlintIconSize::Status;
  } else if (id == FlintIconId::BrandMark) {
    size = FlintIconSize::Brand;
  }
  flintDrawIconSized(gfx, id, size, cx, cy, fg, bg);
}
