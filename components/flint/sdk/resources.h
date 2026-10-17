#pragma once

#include <stdint.h>
#include <LovyanGFX.hpp>

// Shared font / icon pack. Faces pick IDs — never vendoring their own fonts/bitmaps.

enum class FlintFontId : uint8_t {
  TimeLarge = 0,
  Body = 1,
  TimeCompact = 2,
};

enum class FlintIconId : uint8_t {
  BrandMark = 4,
};

// Canonical on-screen sizes for asset pack entries.
enum class FlintIconSize : uint8_t {
  Status = 16,
  Glance = 24,
  Hero = 48,
  Brand = 52,
};

const lgfx::IFont* flintFont(FlintFontId id);

bool flintIconAvailable(FlintIconId id, FlintIconSize size = FlintIconSize::Glance);

void flintDrawIcon(lgfx::LGFXBase& gfx, FlintIconId id, int cx, int cy,
                   uint16_t fg, uint16_t bg);

void flintDrawIconSized(lgfx::LGFXBase& gfx, FlintIconId id, FlintIconSize size,
                        int cx, int cy, uint16_t fg, uint16_t bg);
