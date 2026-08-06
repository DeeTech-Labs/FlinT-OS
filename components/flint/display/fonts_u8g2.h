#pragma once

// Vendored u8g2 font blobs (src/display/fonts/*.c) wrapped for LovyanGFX.

#include <LovyanGFX.hpp>
#include <lgfx/v1/lgfx_fonts.hpp>

#ifdef __cplusplus
extern "C" {
#endif
extern const uint8_t u8g2_font_logisoso46_tn[];
extern const uint8_t u8g2_font_10x20_t_cyrillic[];
#ifdef __cplusplus
}
#endif

inline const lgfx::IFont* flintFontTimeLarge() {
  static const lgfx::U8g2font font(u8g2_font_logisoso46_tn);
  return &font;
}

inline const lgfx::IFont* flintFontBody() {
  static const lgfx::U8g2font font(u8g2_font_10x20_t_cyrillic);
  return &font;
}
