#pragma once

#include <stdint.h>
#include <LovyanGFX.hpp>

// Shared font / icon pack. Faces pick IDs — never vendoring their own .c fonts.

enum class FlintFontId : uint8_t {
  TimeLarge = 0,
  Body = 1,
  Temp = 2,
};

enum class FlintIconId : uint8_t {
  WeatherClear = 0,
  Cloudy,
  Fog,
  Snow,
  Storm,
  Rain,
  Unknown,
};

const lgfx::IFont* flintFont(FlintFontId id);

// Map Open-Meteo weather code → icon id.
FlintIconId flintWeatherIcon(int code);

void flintDrawIcon(lgfx::LGFXBase& gfx, FlintIconId id, int cx, int cy,
                   uint16_t fg, uint16_t bg);
