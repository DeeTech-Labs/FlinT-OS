#pragma once

#include <stdint.h>
#include <LovyanGFX.hpp>

// Shared font / icon pack. Faces pick IDs — never vendoring their own fonts/bitmaps.

enum class FlintFontId : uint8_t {
  TimeLarge = 0,
  Body = 1,
  Temp = 2,
};

enum class FlintIconId : uint8_t {
  WeatherClear = 0,
  Cloudy = 1,
  Fog = 2,
  Snow = 3,
  Storm = 4,
  Rain = 5,
  Unknown = 6,
  WifiOff = 7,
  Wifi1 = 8,   // connected, weak RSSI
  Wifi2 = 9,   // medium
  Wifi3 = 10,  // strong
  BrandMark = 11,
};

// WMO weather class — single ladder for icon, label, and accent tint.
enum class FlintWxClass : uint8_t {
  Clear = 0,
  Cloudy,
  Fog,
  Drizzle,
  Rain,
  Showers,
  Snow,
  Storm,
  Unknown,
};

// Canonical on-screen sizes for asset pack entries.
enum class FlintIconSize : uint8_t {
  Status = 16,
  Glance = 24,
  Hero = 48,
  Brand = 52,
};

const lgfx::IFont* flintFont(FlintFontId id);

FlintWxClass flintWeatherClass(int code);
FlintIconId flintWeatherIcon(int code);
// Accent tint for the class; returns `base` when Unknown / unmapped.
uint16_t flintWeatherAccent(int code, uint16_t base);

// Status glyph for Wi‑Fi bars (0 = off / disconnected, 1–3 = signal).
FlintIconId flintWifiIcon(uint8_t bars);

bool flintIconAvailable(FlintIconId id, FlintIconSize size = FlintIconSize::Glance);

void flintDrawIcon(lgfx::LGFXBase& gfx, FlintIconId id, int cx, int cy,
                   uint16_t fg, uint16_t bg);

void flintDrawIconSized(lgfx::LGFXBase& gfx, FlintIconId id, FlintIconSize size,
                        int cx, int cy, uint16_t fg, uint16_t bg);
